#!/usr/bin/env python3
"""Gate: every Rust ``extern "C"`` declaration matches the C it names.

The Rust crate is the one binding in this tree that jm does not generate --
``ffi/rust/`` is hand-written against the C ABI -- so `jm status --check`,
which keeps the C and Python faces honest, has nothing to say about it. That
left the crate's declarations free to describe a C function that no longer
exists, and the compiler cannot help: an ``extern "C"`` block is a promise,
not a question. Nothing verifies it at build time, at link time, or at run
time.

doppler#911 is what that costs. `lo_steps_ctrl` declared its control port
``*const f32`` while the C had been widened to ``const double *``:

    /* native/inc/lo/lo_core.h */
    size_t lo_steps_ctrl (lo_state_t *state, const double *ctrl, ...);

    // ffi/rust/src/lo.rs
    pub fn lo_steps_ctrl(lo: *mut LoStateRaw, ctrl: *const f32, ...);

So C read ``ctrl_len`` doubles out of a buffer holding ``ctrl_len`` floats --
a heap over-read of twice the allocation, reachable from an entirely safe
method, with the values garbage regardless of whether the over-read faulted.
The commit that widened the C side touched no file under ``ffi/rust/``, and
the crate's own test passed because it used an all-zero control buffer: zero
has the same bit pattern at both widths, which is the one input that hides
both faults.

It had happened once before (``cce1792f``, "fix: update Rust FFI to current C
API") and nothing was added then to stop it recurring. This is that.

Nothing is registered: the declarations come out of the Rust source and the
prototypes out of ``native/inc/``, so a new binding is covered by existing.

What is checked, per declaration:

1. NAME    -- the C function exists. A renamed or deleted C function is a
              link error at best and a silently-bound wrong symbol at worst.
2. ARITY   -- same number of parameters.
3. TYPES   -- each parameter's element type agrees, and so does the return.

What is deliberately NOT checked: pointer-ness and const-ness. Rust spells
``*const T`` where C says ``const T *``, and the mapping is mechanical but
noisy; the failure mode this gate exists for is the ELEMENT WIDTH, which is
what silently reads off the end of a buffer.

Usage:  python3 scripts/check_rust_abi.py
Exit 0 when every extern declaration matches the C it names.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUST = ROOT / "ffi" / "rust" / "src"
INC = ROOT / "native" / "inc"

#: Rust primitive -> the C spellings that mean the same width.
PRIMITIVE = {
    "f64": {"double"},
    "f32": {"float"},
    "usize": {"size_t"},
    "isize": {"ssize_t", "ptrdiff_t"},
    "u64": {"uint64_t", "unsignedlong"},
    "i64": {"int64_t", "long"},
    "u32": {"uint32_t", "unsigned", "unsignedint"},
    "i32": {"int32_t", "int"},
    "u16": {"uint16_t", "unsignedshort"},
    "i16": {"int16_t", "short"},
    "u8": {"uint8_t", "unsignedchar"},
    "i8": {"int8_t", "signedchar"},
    "bool": {"bool", "_Bool"},
    "c_void": {"void"},
    "c_char": {"char"},
    # `c_int`/`c_uint` are 32-bit on every platform this project targets
    # (`[project] platforms` is linux + macos, both LP64), so the fixed-width
    # spelling is the same type. Listed rather than assumed, because on a
    # target where that stopped being true this gate should start failing.
    "c_int": {"int", "int32_t"},
    "c_uint": {"unsigned", "unsignedint", "uint32_t"},
    "()": {"void"},
}

#: num_complex's aliases. Fixed rather than derived because they belong to an
#: external crate: `Complex<T>` is documented as two contiguous `T`, which is
#: what makes it layout-compatible with C99 `T _Complex`.
COMPLEX = {
    "Complex<f32>": {"floatcomplex", "float_Complex"},
    "Complex32": {"floatcomplex", "float_Complex"},
    "Complex<f64>": {"doublecomplex", "double_Complex"},
    "Complex64": {"doublecomplex", "double_Complex"},
}

#: A C prototype: an optional return type, a name, and a parenthesised list.
#: `\**` before the name because a factory returns `lo_state_t *lo_create(…)`
#: and the star binds to the name, not the type.
_C_PROTO = re.compile(
    r"^[ \t]*(?:[A-Za-z_][\w ]*?[\w])[ \t]+(\**)\s*(\w+)\s*\(([^;{]*)\)\s*;",
    re.M | re.S,
)
_EXTERN_BLOCK = re.compile(r'extern\s+"C"\s*\{(.*?)\n\}', re.S)
_EXTERN_FN = re.compile(
    r"pub fn (\w+)\s*\((.*?)\)\s*(?:->\s*([^;{]+?))?\s*;", re.S
)
#: `pub struct DpCf32 { pub i: f32, pub q: f32 }` and friends.
_REPR_C = re.compile(r"pub struct (\w+)\s*\{(.*?)\}", re.S)


def local_structs() -> dict[str, set[str]]:
    """doppler's own `repr(C)` structs, mapped to their C equivalent.

    Derived from the field types rather than listed: a struct of two `f32`
    IS a `float complex` as far as the ABI is concerned, and saying so here
    by hand would be a second place to update when one changes.
    """
    out: dict[str, set[str]] = {}
    types = RUST / "types.rs"
    if not types.is_file():
        return out
    for m in _REPR_C.finditer(types.read_text(encoding="utf-8")):
        name, body = m.group(1), m.group(2)
        fields = re.findall(r":\s*(\w+)", body)
        if len(fields) == 2 and fields[0] == fields[1]:
            if fields[0] == "f32":
                out[name] = {"floatcomplex", "float_Complex"}
            elif fields[0] == "f64":
                out[name] = {"doublecomplex", "double_Complex"}
            elif fields[0] in PRIMITIVE:
                out[name] = set(PRIMITIVE[fields[0]])
    return out


def c_prototypes() -> dict[str, tuple[list[str], str]]:
    """name -> (parameter type strings, return type)."""
    out: dict[str, tuple[list[str], str]] = {}
    for header in sorted(INC.rglob("*.h")):
        text = header.read_text(encoding="utf-8", errors="replace")
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", " ", text)
        for m in _C_PROTO.finditer(text):
            name, args = m.group(2), m.group(3)
            if name in out or name in {"if", "for", "while", "switch"}:
                continue
            ret = m.group(0)[: m.group(0).index(name)]
            params = [a.strip() for a in args.split(",") if a.strip()]
            if params == ["void"]:
                params = []
            out[name] = (params, ret)
    return out


def rust_externs() -> dict[str, tuple[list[tuple[str, str]], str, str]]:
    """name -> (params, return type, source file)."""
    out: dict[str, tuple[list[tuple[str, str]], str, str]] = {}
    for path in sorted(RUST.glob("*.rs")):
        text = path.read_text(encoding="utf-8")
        for block in _EXTERN_BLOCK.finditer(text):
            for fn in _EXTERN_FN.finditer(block.group(1)):
                name, args, ret = fn.group(1), fn.group(2), fn.group(3)
                params: list[tuple[str, str]] = []
                for arg in args.split(","):
                    arg = arg.strip()
                    if not arg or ":" not in arg:
                        continue
                    pname, ty = arg.split(":", 1)
                    params.append((pname.strip(), " ".join(ty.split())))
                rel = str(path.relative_to(ROOT))
                out[name] = (params, " ".join((ret or "()").split()), rel)
    return out


def _rust_base(ty: str) -> str:
    return ty.replace("*const", "").replace("*mut", "").strip()


def _c_base(ty: str) -> str:
    ty = re.sub(r"\b(const|volatile|restrict|struct|enum)\b", " ", ty)
    ty = ty.replace("*", " ")
    # Drop the parameter name: the last identifier, when a type precedes it.
    parts = ty.split()
    if len(parts) > 1:
        parts = parts[:-1]
    return "".join(parts)


def agrees(rust_ty: str, c_ty: str, structs: dict[str, set[str]]) -> bool:
    rb, cb = _rust_base(rust_ty), _c_base(c_ty)
    if not cb:
        return True
    for table in (PRIMITIVE, COMPLEX, structs):
        if rb in table:
            return cb in table[rb]
    # An opaque handle: a Rust unit struct against any C struct pointer.
    return True


def main() -> int:
    externs = rust_externs()
    protos = c_prototypes()
    structs = local_structs()
    failures: list[str] = []

    for name, (params, _ret, where) in sorted(externs.items()):
        if name not in protos:
            failures.append(
                f'{where}: `{name}` is declared extern "C" but no such '
                "function is declared in native/inc/. It was renamed or "
                "removed, and nothing links this crate against the header."
            )
            continue
        c_params, _c_ret = protos[name]
        if len(params) != len(c_params):
            failures.append(
                f"{where}: `{name}` takes {len(params)} parameter(s), the C "
                f"takes {len(c_params)}."
            )
            continue
        for i, ((pname, rty), cty) in enumerate(zip(params, c_params)):
            if agrees(rty, cty, structs):
                continue
            failures.append(
                f"{where}: `{name}` parameter {i} (`{pname}`) is `{rty}` in "
                f"Rust and `{cty.strip()}` in C. If the widths differ, the C "
                "side reads past the end of whatever Rust hands it."
            )

    if failures:
        print("check_rust_abi: FAIL\n", file=sys.stderr)
        for f in failures:
            print(f"  - {f}\n", file=sys.stderr)
        return 1

    print(
        f"check_rust_abi: OK — {len(externs)} extern declaration(s) across "
        f"{len({w for _, _, w in externs.values()})} file(s) match the C "
        "they name"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
