#!/usr/bin/env python3
"""Generate a thin Python C extension for a doppler C module.

Given a C source or header file from the doppler library, this tool
produces the minimal Python C extension needed to expose every public
dp_* function to Python.  The generated code compiles and delegates
everything to the C library — no business logic.

Usage
-----
    python tools/gen_pyext.py c/src/fir.c
    python tools/gen_pyext.py c/include/dp/fir.h
    python tools/gen_pyext.py --dry-run c/src/simd.c

Output
------
    python/src/dp_{module}.c

The generated code is a starting point.  Hand-tune afterward for
GIL release on blocking calls, zero-copy recv, or ergonomics.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PY_SRC = ROOT / "python" / "src"

# ── type tables ────────────────────────────────────────────

ARRAY_DTYPES: dict[str, tuple[str, int]] = {
    "dp_cf32_t": ("NPY_COMPLEX64", 1),
    "dp_cf64_t": ("NPY_COMPLEX128", 1),
    "dp_cf128_t": ("NPY_CLONGDOUBLE", 1),
    "dp_ci8_t": ("NPY_INT8", 2),
    "dp_ci16_t": ("NPY_INT16", 2),
    "dp_ci32_t": ("NPY_INT32", 2),
    "double complex": ("NPY_COMPLEX128", 1),
    "float": ("NPY_FLOAT32", 1),
    "double": ("NPY_FLOAT64", 1),
    "uint32_t": ("NPY_UINT32", 1),
    "uint8_t": ("NPY_UINT8", 1),
}

SCALAR_FMT: dict[str, str] = {
    "int": "i", "size_t": "n", "double": "d",
    "float": "f", "uint64_t": "K", "int64_t": "L",
    "uint32_t": "I", "int32_t": "i",
}

# ── data structures ────────────────────────────────────────


@dataclass
class Param:
    raw: str; base_type: str; name: str
    is_const: bool; is_ptr: bool


@dataclass
class Func:
    ret: str; name: str; params: list[Param]

    @property
    def returns_ptr(self) -> bool:
        return "*" in self.ret

    @property
    def returns_void(self) -> bool:
        return self.ret.strip() == "void"

    @property
    def returns_int(self) -> bool:
        return self.ret.strip() == "int"

    @property
    def returns_const_str(self) -> bool:
        r = self.ret.strip()
        return "char" in r and "*" in r and "const" in r

    @property
    def returns_complex(self) -> bool:
        return ("double complex" in self.ret
                and "*" not in self.ret)


@dataclass
class HandleType:
    c_type: str; py_class: str
    constructors: list[Func] = field(default_factory=list)
    destructor: Func | None = None
    methods: list[Func] = field(default_factory=list)


@dataclass
class ArrayIn:
    name: str; npy: str; factor: int; cast: str


@dataclass
class ArrayOut:
    name: str; npy: str; factor: int; cast: str


@dataclass
class ScalarP:
    name: str; fmt: str; c_type: str


@dataclass
class StringP:
    name: str


@dataclass
class ComplexP:
    name: str


@dataclass
class UnknownP:
    name: str; raw: str


CatParam = (ArrayIn | ArrayOut | ScalarP
            | StringP | ComplexP | UnknownP)

# ── parser ─────────────────────────────────────────────────


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def _strip_pp(text: str) -> str:
    out: list[str] = []
    skip = False
    for line in text.split("\n"):
        s = line.strip()
        if s.startswith("#"):
            skip = s.endswith("\\")
            continue
        if skip:
            skip = s.endswith("\\")
            continue
        out.append(line)
    return "\n".join(out)


def _parse_one_param(s: str) -> Param:
    s = s.strip()
    is_const = "const " in s
    clean = s.replace("const ", "").strip()
    is_ptr = "*" in clean
    if is_ptr:
        parts = clean.split("*", 1)
        base, name = parts[0].strip(), parts[1].strip()
    else:
        words = clean.split()
        name = words[-1] if len(words) > 1 else ""
        base = (" ".join(words[:-1])
                if len(words) > 1 else words[0])
    return Param(s, base, name, is_const, is_ptr)


def _parse_params(s: str) -> list[Param]:
    s = s.strip()
    if not s or s == "void":
        return []
    return [_parse_one_param(p) for p in s.split(",")]


def parse_header(
    path: Path,
) -> tuple[dict[str, str], list[Func]]:
    text = _strip_comments(path.read_text())
    handles: dict[str, str] = {}
    for m in re.finditer(
        r"typedef\s+struct\s+(\w+)\s+(\w+)\s*;", text
    ):
        handles[m.group(2)] = m.group(1)

    text = _strip_pp(text)
    text = re.sub(
        r"__attribute__\s*\(\s*\([^)]*\)\s*\)", "", text
    )
    flat = re.sub(r"\s+", " ", text)

    funcs: list[Func] = []
    for m in re.finditer(r"\b(dp_\w+)\s*\(", flat):
        fname, start = m.group(1), m.start()
        depth, i = 1, m.end()
        while i < len(flat) and depth:
            if flat[i] == "(":
                depth += 1
            elif flat[i] == ")":
                depth -= 1
            i += 1
        params_str = flat[m.end():i - 1].strip()
        if not flat[i:].lstrip().startswith(";"):
            continue
        rstart = start
        while rstart > 0 and flat[rstart - 1] not in ";{}":
            rstart -= 1
        ret = flat[rstart:start].strip()
        if not ret or ret in ("typedef", "struct", "enum"):
            continue
        funcs.append(
            Func(ret, fname, _parse_params(params_str))
        )
    return handles, funcs


# ── classifier ─────────────────────────────────────────────


def classify(
    handles: dict[str, str], funcs: list[Func],
) -> tuple[dict[str, HandleType], list[Func]]:
    hts: dict[str, HandleType] = {}
    for tn in handles:
        base = tn
        if base.startswith("dp_"):
            base = base[3:]
        if base.endswith("_t"):
            base = base[:-2]
        py = "".join(w.title() for w in base.split("_"))
        hts[tn] = HandleType(tn, py)

    free: list[Func] = []
    for f in funcs:
        done = False
        if f.returns_ptr:
            for tn, ht in hts.items():
                if tn in f.ret:
                    ht.constructors.append(f)
                    done = True
                    break
        if not done and f.returns_void and "destroy" in f.name:
            for tn, ht in hts.items():
                if f.params and f.params[0].base_type == tn:
                    ht.destructor = f
                    done = True
                    break
        if not done and f.params and f.params[0].is_ptr:
            for tn, ht in hts.items():
                if f.params[0].base_type == tn:
                    ht.methods.append(f)
                    done = True
                    break
        if not done:
            free.append(f)
    return hts, free


# ── parameter analyser ────────────────────────────────────


def analyse_params(
    params: list[Param], skip_first: bool = False,
) -> tuple[list[CatParam], set[str]]:
    rest = params[1:] if skip_first else params[:]
    result: list[CatParam] = []
    consumed: set[str] = set()

    for i, p in enumerate(rest):
        if p.name in consumed:
            continue
        if p.base_type == "double complex" and not p.is_ptr:
            result.append(ComplexP(p.name))
            continue
        if p.is_ptr and p.base_type in ARRAY_DTYPES:
            npy, factor = ARRAY_DTYPES[p.base_type]
            cp = "const " if p.is_const else ""
            cast = f"{cp}{p.base_type} *"
            if i + 1 < len(rest):
                nxt = rest[i + 1]
                if (nxt.base_type == "size_t"
                        and not nxt.is_ptr
                        and nxt.name not in consumed):
                    consumed.add(nxt.name)
            cls = ArrayIn if p.is_const else ArrayOut
            result.append(cls(p.name, npy, factor, cast))
            continue
        if p.is_ptr and p.base_type == "char" and p.is_const:
            result.append(StringP(p.name))
            continue
        if not p.is_ptr and p.base_type in SCALAR_FMT:
            result.append(
                ScalarP(p.name, SCALAR_FMT[p.base_type],
                        p.base_type))
            continue
        result.append(UnknownP(p.name, p.raw))
    return result, consumed


def _mname(func_name: str, mod: str) -> str:
    prefix = f"dp_{mod}_"
    if func_name.startswith(prefix):
        return func_name[len(prefix):]
    return func_name.replace("dp_", "", 1)


# ── code generator ─────────────────────────────────────────


class Gen:
    def __init__(self, mod: str):
        self.mod = mod
        self.L: list[str] = []
        self._numpy = False
        self._complex = False

    def w(self, *lines: str) -> None:
        self.L.extend(lines)

    def generate(
        self, hts: dict[str, HandleType], free: list[Func],
    ) -> str:
        for f in (list(free)
                  + [m for ht in hts.values()
                     for m in ht.constructors + ht.methods]):
            cats, _ = analyse_params(f.params)
            if any(isinstance(c, (ArrayIn, ArrayOut))
                   for c in cats):
                self._numpy = True
            if (any(isinstance(c, ComplexP) for c in cats)
                    or f.returns_complex):
                self._complex = True

        self._banner()
        self._includes()
        for ht in hts.values():
            if ht.constructors or ht.methods:
                self._handle_class(ht)
        if free:
            self._free_funcs(free)
        self._module_def(hts, free)
        self._init_func(hts)
        return "\n".join(self.L) + "\n"

    # ── banner / includes ──────────────────────────────────

    def _banner(self) -> None:
        m = self.mod
        self.w(
            "/*",
            f" * dp_{m}.c — Python C extension"
            f" for dp/{m}.h",
            " *",
            " * AUTO-GENERATED by tools/gen_pyext.py"
            " — DO NOT EDIT.",
            " * Regenerate:",
            f" *   python tools/gen_pyext.py"
            f" c/include/dp/{m}.h",
            " */", "")

    def _includes(self) -> None:
        self.w("#define PY_SSIZE_T_CLEAN",
               "#include <Python.h>")
        if self._numpy:
            self.w("#define NPY_NO_DEPRECATED_API"
                   " NPY_1_7_API_VERSION",
                   "#include <numpy/arrayobject.h>")
        if self._complex:
            self.w("#include <complex.h>")
        self.w("",
               f"#include <dp/{self.mod}.h>",
               "#include <dp/stream.h>"
               "  /* error codes, types */", "")

    # ── handle class ───────────────────────────────────────

    def _handle_class(self, ht: HandleType) -> None:
        C, T = ht.py_class, ht.c_type
        D = ht.destructor.name if ht.destructor else "/* TODO */"

        # struct
        self.w(
            f"/* {'=' * 56} */",
            f"/* {C}Object — wraps {T} *",
            f" * {'=' * 56} */", "",
            "typedef struct", "{",
            "  PyObject_HEAD",
            f"  {T} *handle;",
            f"}} {C}Object;", "")

        # dealloc
        self.w(
            "static void",
            f"{C}_dealloc ({C}Object *self)",
            "{",
            "  if (self->handle)",
            f"    {D} (self->handle);",
            "  Py_TYPE (self)->tp_free ((PyObject *)self);",
            "}", "")

        # tp_new
        self.w(
            "static PyObject *",
            f"{C}_new (PyTypeObject *type, PyObject *args,",
            "         PyObject *kwds)",
            "{",
            f"  {C}Object *self",
            f"      = ({C}Object *)type->tp_alloc (type, 0);",
            "  if (self)",
            "    self->handle = NULL;",
            "  return (PyObject *)self;",
            "}", "")

        # constructors
        if ht.constructors:
            self._gen_init(ht, ht.constructors[0])
        for ctor in ht.constructors[1:]:
            self._gen_classmethod_ctor(ht, ctor)

        # methods
        for meth in ht.methods:
            self._gen_method(ht, meth)

        # destroy wrapper
        if ht.destructor:
            mn = _mname(ht.destructor.name, self.mod)
            self.w(
                "static PyObject *",
                f"{C}_{mn} ({C}Object *self,"
                " PyObject *Py_UNUSED (ignored))",
                "{",
                "  if (self->handle)",
                "    {",
                f"      {D} (self->handle);",
                "      self->handle = NULL;",
                "    }",
                "  Py_RETURN_NONE;",
                "}", "")

        # context manager
        self.w(
            "static PyObject *",
            f"{C}_enter ({C}Object *self,"
            " PyObject *Py_UNUSED (ignored))",
            "{",
            "  Py_INCREF (self);",
            "  return (PyObject *)self;",
            "}", "",
            "static PyObject *",
            f"{C}_exit ({C}Object *self, PyObject *args)",
            "{",
            "  (void)args;",
            "  if (self->handle)",
            "    {",
            f"      {D} (self->handle);",
            "      self->handle = NULL;",
            "    }",
            "  Py_RETURN_NONE;",
            "}", "")

        # method table
        self._gen_method_table(ht)

        # type object
        init_ln = (f"    .tp_init = (initproc){C}_init,"
                   if ht.constructors else "")
        self.w(
            f"static PyTypeObject {C}Type = {{",
            "    PyVarObject_HEAD_INIT (NULL, 0)",
            f'    .tp_name = "dp_{self.mod}.{C}",',
            f"    .tp_basicsize = sizeof ({C}Object),",
            f"    .tp_dealloc = (destructor){C}_dealloc,",
            "    .tp_flags = Py_TPFLAGS_DEFAULT,",
            f'    .tp_doc = "Wraps {T}.",',
            f"    .tp_methods = {C}_methods,",
            f"    .tp_new = {C}_new,",
            init_ln,
            "};", "")

    # ── __init__ (primary constructor) ─────────────────────

    def _gen_init(self, ht: HandleType, ctor: Func) -> None:
        C = ht.py_class
        cats, _ = analyse_params(ctor.params)
        kw, fmt, decls, refs, pre, post, call = (
            self._ctor_args(cats))

        kwl = ", ".join(f'"{k}"' for k in kw)
        fmts = "".join(fmt)
        refstr = ", ".join(refs)
        callstr = ",\n      ".join(call)

        self.w(
            "static int",
            f"{C}_init ({C}Object *self, PyObject *args,",
            "          PyObject *kwds)",
            "{",
            f"  static char *kwlist[] = {{{kwl}, NULL}};",
            *decls, "",
            f'  if (!PyArg_ParseTupleAndKeywords (',
            f'          args, kwds, "{fmts}",'
            f' kwlist, {refstr}))',
            "    return -1;",
            *pre, "",
            f"  self->handle = {ctor.name} (",
            f"      {callstr});",
            *post, "",
            "  if (!self->handle)",
            "    {",
            f'      PyErr_SetString (PyExc_MemoryError,',
            f'                       "{ctor.name}'
            f' returned NULL");',
            "      return -1;",
            "    }",
            "  return 0;",
            "}", "")

    def _ctor_args(
        self, cats: list[CatParam], ret_err: str = "-1",
    ):
        """Build components for constructor arg parsing."""
        kw: list[str] = []
        fmt: list[str] = []
        decls: list[str] = []
        refs: list[str] = []
        pre: list[str] = []
        post: list[str] = []
        call: list[str] = []

        for c in cats:
            if isinstance(c, ArrayIn):
                kw.append(c.name)
                fmt.append("O")
                decls.append(
                    f"  PyObject *{c.name}_obj = NULL;")
                refs.append(f"&{c.name}_obj")
                pre += self._array_conv(
                    c, c.name, ret_err=ret_err)
                call.append(
                    f"({c.cast}) PyArray_DATA"
                    f" ({c.name}_arr)")
                call.append(f"(size_t) {c.name}_n")
                post.append(
                    f"  Py_DECREF ({c.name}_arr);")
            elif isinstance(c, ScalarP):
                kw.append(c.name)
                fmt.append(c.fmt)
                decls.append(
                    f"  {c.c_type} {c.name} = 0;")
                refs.append(f"&{c.name}")
                call.append(c.name)
            elif isinstance(c, StringP):
                kw.append(c.name)
                fmt.append("z")
                decls.append(
                    f"  const char *{c.name} = NULL;")
                refs.append(f"&{c.name}")
                call.append(c.name)
            else:
                call.append(f"/* TODO: {c} */")
        return kw, fmt, decls, refs, pre, post, call

    def _array_conv(
        self, c: ArrayIn, nm: str, ret_err: str = "NULL",
    ) -> list[str]:
        """Lines to convert a PyObject* to typed NumPy array."""
        lines = [
            "",
            f"  PyArrayObject *{nm}_arr"
            f" = (PyArrayObject *)PyArray_FROM_OTF (",
            f"      {nm}_obj, {c.npy},"
            f" NPY_ARRAY_C_CONTIGUOUS);",
            f"  if (!{nm}_arr)",
            f"    return {ret_err};",
        ]
        if c.factor == 2:
            lines += [
                f"  if (PyArray_SIZE ({nm}_arr) % 2 != 0)",
                "    {",
                f"      Py_DECREF ({nm}_arr);",
                '      PyErr_SetString (PyExc_ValueError,',
                '          "I/Q array must have even length");',
                f"      return {ret_err};",
                "    }",
                f"  Py_ssize_t {nm}_n"
                f" = PyArray_SIZE ({nm}_arr) / 2;",
            ]
        else:
            lines.append(
                f"  Py_ssize_t {nm}_n"
                f" = PyArray_SIZE ({nm}_arr);")
        return lines

    # ── classmethod constructor ────────────────────────────

    def _gen_classmethod_ctor(
        self, ht: HandleType, ctor: Func,
    ) -> None:
        C = ht.py_class
        mn = _mname(ctor.name, self.mod)
        cats, _ = analyse_params(ctor.params)
        kw, fmt, decls, refs, pre, post, call = (
            self._ctor_args(cats, ret_err="NULL"))
        fmts = "".join(fmt)
        refstr = ", ".join(refs)
        callstr = ",\n      ".join(call)
        dfn = (ht.destructor.name
               if ht.destructor else "free")

        self.w(
            "static PyObject *",
            f"{C}_{mn} (PyTypeObject *type, PyObject *args)",
            "{",
            *decls, "",
            f'  if (!PyArg_ParseTuple (args, "{fmts}",'
            f" {refstr}))",
            "    return NULL;",
            *pre, "",
            f"  {ht.c_type} *handle = {ctor.name} (",
            f"      {callstr});",
            *post, "",
            "  if (!handle)",
            "    {",
            f'      PyErr_SetString (PyExc_MemoryError,',
            f'          "{ctor.name} returned NULL");',
            "      return NULL;",
            "    }",
            "",
            f"  {C}Object *self",
            f"      = ({C}Object *)type->tp_alloc (type, 0);",
            "  if (!self)",
            "    {",
            f"      {dfn} (handle);",
            "      return NULL;",
            "    }",
            "  self->handle = handle;",
            "  return (PyObject *)self;",
            "}", "")

    # ── method dispatcher ──────────────────────────────────

    def _gen_method(self, ht: HandleType, func: Func) -> None:
        cats, _ = analyse_params(func.params, skip_first=True)
        ins = [c for c in cats if isinstance(c, ArrayIn)]
        outs = [c for c in cats if isinstance(c, ArrayOut)]
        if ins and outs and func.returns_int:
            self._gen_array_exec(ht, func, ins[0], outs[0])
        elif outs and func.returns_void:
            self._gen_void_array_exec(ht, func, ins, outs)
        elif not cats and func.returns_void:
            self._gen_void_noargs(ht, func)
        elif cats and not ins and not outs:
            self._gen_scalar_method(ht, func, cats)
        else:
            self._gen_stub(ht, func)

    # ── void array execute (allocate-and-return) ───────────
    #
    # Handles the NCO / free-running execute family:
    #   void f(handle, [*in_arr,] *out1 [, *out2], size_t n)
    #
    # Python signature:
    #   • no input array  → caller passes n as an int;
    #                        returns ndarray (or tuple of ndarrays)
    #   • with input array → n derived from len(input);
    #                         returns ndarray (or tuple of ndarrays)

    def _gen_void_array_exec(
        self, ht: HandleType, func: Func,
        ins: list[ArrayIn], outs: list[ArrayOut],
    ) -> None:
        C = ht.py_class
        mn = _mname(func.name, self.mod)
        fn = f"{C}_{mn}"
        multi = len(outs) > 1

        lines: list[str] = [
            "static PyObject *",
            f"{fn} ({C}Object *self, PyObject *args)",
            "{",
            "  if (!self->handle)",
            "    {",
            '      PyErr_SetString (PyExc_RuntimeError,',
            '                       "destroyed");',
            "      return NULL;",
            "    }",
        ]

        if ins:
            inp = ins[0]
            lines += [
                "  PyObject *in_obj = NULL;",
                '  if (!PyArg_ParseTuple (args, "O", &in_obj))',
                "    return NULL;",
                "",
                "  PyArrayObject *in_arr"
                " = (PyArrayObject *)PyArray_FROM_OTF (",
                f"      in_obj, {inp.npy},"
                " NPY_ARRAY_C_CONTIGUOUS);",
                "  if (!in_arr)",
                "    return NULL;",
                "  Py_ssize_t n = PyArray_SIZE (in_arr);",
            ]
        else:
            lines += [
                "  Py_ssize_t n = 0;",
                '  if (!PyArg_ParseTuple (args, "n", &n))',
                "    return NULL;",
            ]

        def _prev_arr(idx: int) -> str:
            return f"out{idx}_arr" if multi else "out_arr"

        for k, out in enumerate(outs):
            av = _prev_arr(k)
            od = "n * 2" if out.factor == 2 else "n"
            cleanup: list[str] = []
            if ins:
                cleanup.append("      Py_DECREF (in_arr);")
            for j in range(k):
                cleanup.append(
                    f"      Py_DECREF ({_prev_arr(j)});")
            lines += [
                "",
                f"  npy_intp dims{k}[] = {{{od}}};",
                f"  PyObject *{av}",
                f"      = PyArray_SimpleNew"
                f" (1, dims{k}, {out.npy});",
                f"  if (!{av})",
                "    {",
                *cleanup,
                "      return NULL;",
                "    }",
            ]

        cargs = ["self->handle"]
        if ins:
            cargs.append(
                f"({ins[0].cast}) PyArray_DATA (in_arr)")
        for k, out in enumerate(outs):
            av = _prev_arr(k)
            cargs.append(
                f"({out.cast}) PyArray_DATA"
                f" ((PyArrayObject *){av})")
        cargs.append("(size_t) n")
        call = ",\n      ".join(cargs)

        lines += ["", f"  {func.name} (", f"      {call});"]

        if ins:
            lines.append("  Py_DECREF (in_arr);")

        if multi:
            pack = ", ".join(_prev_arr(k) for k in range(len(outs)))
            lines += [
                "",
                f"  PyObject *result ="
                f" PyTuple_Pack ({len(outs)}, {pack});",
            ]
            for k in range(len(outs)):
                lines.append(f"  Py_DECREF ({_prev_arr(k)});")
            lines.append("  return result;")
        else:
            lines.append(f"  return {_prev_arr(0)};")

        lines += ["}", ""]
        self.w(*lines)

    # ── array execute ──────────────────────────────────────

    def _gen_array_exec(
        self, ht: HandleType, func: Func,
        inp: ArrayIn, out: ArrayOut,
    ) -> None:
        C = ht.py_class
        mn = _mname(func.name, self.mod)
        fn = f"{C}_{mn}"
        od = "n" if out.factor == 1 else "n * 2"

        sz: list[str]
        if inp.factor == 2:
            sz = [
                "  if (total % 2 != 0)",
                "    {",
                "      Py_DECREF (in_arr);",
                '      PyErr_SetString (PyExc_ValueError,',
                '          "I/Q array must have even length");',
                "      return NULL;",
                "    }",
                "  Py_ssize_t n = total / 2;",
            ]
        else:
            sz = ["  Py_ssize_t n = total;"]

        self.w(
            "static PyObject *",
            f"{fn} ({C}Object *self, PyObject *args)",
            "{",
            "  PyObject *in_obj = NULL;",
            '  if (!PyArg_ParseTuple (args, "O", &in_obj))',
            "    return NULL;",
            "",
            "  if (!self->handle)",
            "    {",
            '      PyErr_SetString (PyExc_RuntimeError,',
            '                       "destroyed");',
            "      return NULL;",
            "    }",
            "",
            "  PyArrayObject *in_arr"
            " = (PyArrayObject *)PyArray_FROM_OTF (",
            f"      in_obj, {inp.npy},"
            " NPY_ARRAY_C_CONTIGUOUS);",
            "  if (!in_arr)",
            "    return NULL;",
            "",
            "  Py_ssize_t total = PyArray_SIZE (in_arr);",
            *sz, "",
            f"  npy_intp dims[] = {{{od}}};",
            "  PyObject *out_arr",
            f"      = PyArray_SimpleNew (1, dims, {out.npy});",
            "  if (!out_arr)",
            "    {",
            "      Py_DECREF (in_arr);",
            "      return NULL;",
            "    }",
            "",
            f"  int rc = {func.name} (",
            "      self->handle,",
            f"      ({inp.cast}) PyArray_DATA (in_arr),",
            f"      ({out.cast}) PyArray_DATA"
            " ((PyArrayObject *)out_arr),",
            "      (size_t) n);",
            "  Py_DECREF (in_arr);",
            "",
            "  if (rc != DP_OK)",
            "    {",
            "      Py_DECREF (out_arr);",
            "      PyErr_Format (PyExc_RuntimeError,",
            f'          "{func.name}: %s",'
            " dp_strerror (rc));",
            "      return NULL;",
            "    }",
            "  return out_arr;",
            "}", "")

    # ── void noargs ────────────────────────────────────────

    def _gen_void_noargs(
        self, ht: HandleType, func: Func,
    ) -> None:
        C = ht.py_class
        mn = _mname(func.name, self.mod)
        self.w(
            "static PyObject *",
            f"{C}_{mn} ({C}Object *self,"
            " PyObject *Py_UNUSED (ignored))",
            "{",
            "  if (!self->handle)",
            "    {",
            '      PyErr_SetString (PyExc_RuntimeError,',
            '                       "destroyed");',
            "      return NULL;",
            "    }",
            f"  {func.name} (self->handle);",
            "  Py_RETURN_NONE;",
            "}", "")

    # ── scalar / string method ─────────────────────────────

    def _gen_scalar_method(
        self, ht: HandleType, func: Func,
        cats: list[CatParam],
    ) -> None:
        C = ht.py_class
        mn = _mname(func.name, self.mod)
        fn = f"{C}_{mn}"
        fmts, decls, refs, cargs = [], [], [], ["self->handle"]
        for c in cats:
            if isinstance(c, ScalarP):
                fmts.append(c.fmt)
                decls.append(f"  {c.c_type} {c.name} = 0;")
                refs.append(f"&{c.name}")
                cargs.append(c.name)
            elif isinstance(c, StringP):
                fmts.append("z")
                decls.append(
                    f"  const char *{c.name} = NULL;")
                refs.append(f"&{c.name}")
                cargs.append(c.name)
            else:
                cargs.append(f"/* TODO: {c} */")

        has_args = bool(fmts)
        arg_d = ("PyObject *args" if has_args
                 else "PyObject *Py_UNUSED (ignored)")
        fmt_s = "".join(fmts)
        ref_s = ", ".join(refs)
        ca = ", ".join(cargs)

        self.w("static PyObject *",
               f"{fn} ({C}Object *self, {arg_d})",
               "{",
               "  if (!self->handle)",
               "    {",
               '      PyErr_SetString (PyExc_RuntimeError,',
               '                       "destroyed");',
               "      return NULL;",
               "    }",
               *decls)
        if has_args:
            self.w(
                f'  if (!PyArg_ParseTuple (args,'
                f' "{fmt_s}", {ref_s}))',
                "    return NULL;")
        self.w("")
        self._emit_call_and_return(func, ca)
        self.w("}", "")

    def _emit_call_and_return(
        self, func: Func, cargs: str,
    ) -> None:
        if func.returns_void:
            self.w(f"  {func.name} ({cargs});",
                   "  Py_RETURN_NONE;")
        elif func.returns_int:
            self.w(
                f"  int rc = {func.name} ({cargs});",
                "  if (rc != DP_OK)",
                "    {",
                "      PyErr_Format (PyExc_RuntimeError,",
                f'          "{func.name}: %s",'
                " dp_strerror (rc));",
                "      return NULL;",
                "    }",
                "  Py_RETURN_NONE;")
        elif func.returns_const_str:
            self.w(
                f"  const char *rv = {func.name} ({cargs});",
                "  if (!rv)",
                "    Py_RETURN_NONE;",
                "  return PyUnicode_FromString (rv);")
        elif "uint64_t" in func.ret:
            self.w(
                f"  uint64_t rv = {func.name} ({cargs});",
                "  return PyLong_FromUnsignedLongLong (rv);")
        else:
            self.w(f"  /* TODO: return */ {func.name}"
                   f" ({cargs});",
                   "  Py_RETURN_NONE;")

    # ── stub ───────────────────────────────────────────────

    def _gen_stub(self, ht: HandleType, func: Func) -> None:
        C = ht.py_class
        mn = _mname(func.name, self.mod)
        sig = ", ".join(p.raw for p in func.params)
        self.w(
            f"/* TODO: {func.name} — unrecognised pattern.",
            f" * {func.ret} {func.name} ({sig})",
            " */",
            "static PyObject *",
            f"{C}_{mn} ({C}Object *self, PyObject *args)",
            "{",
            "  (void)self; (void)args;",
            "  PyErr_SetString (PyExc_NotImplementedError,",
            f'      "{func.name}: binding not generated");',
            "  return NULL;",
            "}", "")

    # ── method table ───────────────────────────────────────

    def _gen_method_table(self, ht: HandleType) -> None:
        C = ht.py_class
        entries: list[str] = []

        for ctor in ht.constructors[1:]:
            mn = _mname(ctor.name, self.mod)
            entries.append(
                f'  {{"{mn}", (PyCFunction){C}_{mn},')
            entries.append(
                "   METH_VARARGS | METH_CLASS,")
            entries.append(f'   "{ctor.name}."}},')

        for meth in ht.methods:
            mn = _mname(meth.name, self.mod)
            cats, _ = analyse_params(
                meth.params, skip_first=True)
            fl = "METH_VARARGS" if cats else "METH_NOARGS"
            entries.append(
                f'  {{"{mn}", (PyCFunction){C}_{mn},')
            entries.append(f"   {fl},")
            entries.append(f'   "{meth.name}."}},')

        if ht.destructor:
            mn = _mname(ht.destructor.name, self.mod)
            entries.append(
                f'  {{"{mn}", (PyCFunction){C}_{mn},')
            entries.append('   METH_NOARGS,')
            entries.append('   "Release resources."},')

        entries += [
            f'  {{"__enter__",'
            f" (PyCFunction){C}_enter,",
            "   METH_NOARGS, NULL},",
            f'  {{"__exit__",'
            f" (PyCFunction){C}_exit,",
            "   METH_VARARGS, NULL},",
            "  {NULL}",
        ]

        self.w(f"static PyMethodDef {C}_methods[] = {{",
               *entries,
               "};", "")

    # ── free (module-level) functions ──────────────────────

    def _free_funcs(self, funcs: list[Func]) -> None:
        self.w(
            "/* =========================================="
            "============== */",
            "/* Module-level functions"
            "                            */",
            "/* =========================================="
            "============== */", "")
        for f in funcs:
            self._gen_free_func(f)

    def _gen_free_func(self, func: Func) -> None:
        cats, _ = analyse_params(func.params)
        mn = _mname(func.name, self.mod)
        fn = f"py_{mn}"

        # complex scalar (e.g. dp_c16_mul)
        if func.returns_complex and all(
                isinstance(c, ComplexP) for c in cats):
            self._gen_complex_func(func, fn, cats)
            return

        # array in → array out
        ins = [c for c in cats if isinstance(c, ArrayIn)]
        outs = [c for c in cats if isinstance(c, ArrayOut)]
        if ins and outs:
            self._gen_free_array(func, fn, ins[0], outs[0])
            return

        # scalar / void / string-return
        self._gen_free_scalar(func, fn, cats)

    def _gen_complex_func(
        self, func: Func, fn: str, cats: list[CatParam],
    ) -> None:
        fmts, decls, refs, conv, ca = [], [], [], [], []
        for c in cats:
            assert isinstance(c, ComplexP)
            fmts.append("D")
            decls.append(f"  Py_complex py_{c.name};")
            refs.append(f"&py_{c.name}")
            conv.append(
                f"  double complex {c.name}"
                f" = py_{c.name}.real"
                f" + py_{c.name}.imag * I;")
            ca.append(c.name)
        fmt_s = "".join(fmts)
        ref_s = ", ".join(refs)
        ca_s = ", ".join(ca)

        self.w(
            "static PyObject *",
            f"{fn} (PyObject *self, PyObject *args)",
            "{",
            "  (void)self;",
            *decls, "",
            f'  if (!PyArg_ParseTuple (args, "{fmt_s}",'
            f" {ref_s}))",
            "    return NULL;",
            *conv, "",
            f"  double complex rv = {func.name} ({ca_s});",
            "  return PyComplex_FromDoubles"
            " (creal (rv), cimag (rv));",
            "}", "")

    def _gen_free_array(
        self, func: Func, fn: str,
        inp: ArrayIn, out: ArrayOut,
    ) -> None:
        od = "n" if out.factor == 1 else "n * 2"
        sz: list[str]
        if inp.factor == 2:
            sz = [
                "  if (total % 2 != 0)",
                "    {",
                "      Py_DECREF (in_arr);",
                '      PyErr_SetString (PyExc_ValueError,',
                '          "I/Q array must have even'
                ' length");',
                "      return NULL;",
                "    }",
                "  Py_ssize_t n = total / 2;",
            ]
        else:
            sz = ["  Py_ssize_t n = total;"]

        self.w(
            "static PyObject *",
            f"{fn} (PyObject *self, PyObject *args)",
            "{",
            "  (void)self;",
            "  PyObject *in_obj = NULL;",
            '  if (!PyArg_ParseTuple (args, "O", &in_obj))',
            "    return NULL;",
            "",
            "  PyArrayObject *in_arr"
            " = (PyArrayObject *)PyArray_FROM_OTF (",
            f"      in_obj, {inp.npy},"
            " NPY_ARRAY_C_CONTIGUOUS);",
            "  if (!in_arr)",
            "    return NULL;",
            "",
            "  Py_ssize_t total = PyArray_SIZE (in_arr);",
            *sz, "",
            f"  npy_intp dims[] = {{{od}}};",
            "  PyObject *out_arr"
            f" = PyArray_SimpleNew (1, dims, {out.npy});",
            "  if (!out_arr)",
            "    {",
            "      Py_DECREF (in_arr);",
            "      return NULL;",
            "    }",
            "")
        if func.returns_void:
            self.w(
                f"  {func.name} (",
                f"      ({inp.cast}) PyArray_DATA (in_arr),",
                f"      ({out.cast}) PyArray_DATA"
                " ((PyArrayObject *)out_arr));",
                "  Py_DECREF (in_arr);",
                "  return out_arr;")
        else:
            self.w(
                f"  int rc = {func.name} (",
                f"      ({inp.cast}) PyArray_DATA (in_arr),",
                f"      ({out.cast}) PyArray_DATA"
                " ((PyArrayObject *)out_arr));",
                "  Py_DECREF (in_arr);",
                "  if (rc != DP_OK)",
                "    {",
                "      Py_DECREF (out_arr);",
                "      PyErr_Format (PyExc_RuntimeError,",
                f'          "{func.name}: %s",'
                " dp_strerror (rc));",
                "      return NULL;",
                "    }",
                "  return out_arr;")
        self.w("}", "")

    def _gen_free_scalar(
        self, func: Func, fn: str, cats: list[CatParam],
    ) -> None:
        fmts, decls, refs, ca = [], [], [], []
        for c in cats:
            if isinstance(c, ScalarP):
                fmts.append(c.fmt)
                decls.append(f"  {c.c_type} {c.name} = 0;")
                refs.append(f"&{c.name}")
                ca.append(c.name)
            elif isinstance(c, StringP):
                fmts.append("z")
                decls.append(
                    f"  const char *{c.name} = NULL;")
                refs.append(f"&{c.name}")
                ca.append(c.name)
            else:
                ca.append(f"/* TODO: {c} */")

        has_args = bool(fmts)
        arg_d = ("PyObject *args" if has_args
                 else "PyObject *Py_UNUSED (ignored)")
        ca_s = ", ".join(ca)

        self.w("static PyObject *",
               f"{fn} (PyObject *self, {arg_d})",
               "{",
               "  (void)self;",
               *decls)
        if has_args:
            fmt_s = "".join(fmts)
            ref_s = ", ".join(refs)
            self.w(
                f'  if (!PyArg_ParseTuple (args,'
                f' "{fmt_s}", {ref_s}))',
                "    return NULL;")
        self.w("")
        self._emit_call_and_return(func, ca_s)
        self.w("}", "")

    # ── module def ─────────────────────────────────────────

    def _module_def(
        self, hts: dict[str, HandleType],
        free: list[Func],
    ) -> None:
        self.w(
            "/* =========================================="
            "============== */",
            "/* Module definition"
            "                                 */",
            "/* =========================================="
            "============== */", "")

        if free:
            entries: list[str] = []
            for f in free:
                mn = _mname(f.name, self.mod)
                cats, _ = analyse_params(f.params)
                fl = ("METH_VARARGS" if cats
                      else "METH_NOARGS")
                entries += [
                    f'  {{"{mn}", py_{mn},',
                    f"   {fl},",
                    f'   "{f.name}."}},',
                ]
            entries.append("  {NULL}")
            self.w("static PyMethodDef module_methods[] = {",
                   *entries, "};", "")
            mm = "module_methods"
        else:
            mm = "NULL"

        m = self.mod
        self.w(
            f"static PyModuleDef dp_{m}_module = {{",
            "    PyModuleDef_HEAD_INIT,",
            f'    .m_name = "dp_{m}",',
            f'    .m_doc = "Python binding for dp/{m}.h'
            f' (auto-generated).",',
            "    .m_size = -1,",
            f"    .m_methods = {mm},",
            "};", "")

    # ── PyInit ─────────────────────────────────────────────

    def _init_func(
        self, hts: dict[str, HandleType],
    ) -> None:
        m = self.mod
        self.w("PyMODINIT_FUNC",
               f"PyInit_dp_{m} (void)",
               "{")
        if self._numpy:
            self.w("  import_array ();")
        for ht in hts.values():
            C = ht.py_class
            self.w(
                f"  if (PyType_Ready (&{C}Type) < 0)",
                "    return NULL;")
        self.w("",
               f"  PyObject *m"
               f" = PyModule_Create (&dp_{m}_module);",
               "  if (!m)",
               "    return NULL;")
        for ht in hts.values():
            C = ht.py_class
            self.w(
                "",
                f"  Py_INCREF (&{C}Type);",
                f"  if (PyModule_AddObject (",
                f'          m, "{C}",',
                f"          (PyObject *)&{C}Type) < 0)",
                "    {",
                f"      Py_DECREF (&{C}Type);",
                "      Py_DECREF (m);",
                "      return NULL;",
                "    }")
        self.w("",
               "  return m;",
               "}")


# ── entry point ────────────────────────────────────────────


def resolve_header(
    input_path: Path,
) -> tuple[str, Path]:
    if input_path.suffix == ".h":
        return input_path.stem, input_path
    mod = input_path.stem
    header = ROOT / "c" / "include" / "dp" / f"{mod}.h"
    if not header.exists():
        sys.exit(f"error: header not found: {header}")
    return mod, header


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument(
        "input", type=Path,
        help="C source (.c) or header (.h)")
    ap.add_argument(
        "--dry-run", action="store_true",
        help="Print to stdout instead of writing file")
    ap.add_argument(
        "-o", "--output", type=Path, default=None,
        help="Override output path")
    args = ap.parse_args()

    mod, header = resolve_header(args.input)
    handles, funcs = parse_header(header)
    hts, free = classify(handles, funcs)

    gen = Gen(mod)
    code = gen.generate(hts, free)

    if args.dry_run:
        sys.stdout.write(code)
    else:
        out = args.output or (PY_SRC / f"dp_{mod}.c")
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(code)
        print(f"wrote {out}  ({len(code)} bytes)")
        print(f"  module : dp_{mod}")
        if hts:
            print(f"  classes: "
                  f"{', '.join(h.py_class for h in hts.values())}")
        if free:
            print(f"  funcs  : {len(free)}")


if __name__ == "__main__":
    main()
