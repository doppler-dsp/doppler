"""A mutator's value is state (#2022): found, probed, and listed exactly.

The rule (docs/design/state-serialization.md, "What goes in the blob"): a
value any post-create mutator can change travels in the object's state blob,
checked on restore by that mutator's own predicate; create-time config no
mutator reaches is create()'s. This gate finds every mutator itself, decides
by BEHAVIOUR whether its value is in the blob, and holds everything that is
not to a shrink-only, verdict-exact list, ``scripts/.mutator-state-exempt``.

Registration-free
-----------------
- **Objects:** every class a manifest marks ``serializable = "true"``, views
  included, resolved to the type Python actually gets.
- **Members, from that type:** every public attribute, wherever it was bound
  -- the manifest, a view's own methods, a jm built-in or accessor, or a
  hand-written fragment. A writable property is a mutator, and so is EVERY
  method but the lifecycle (``reset``, ``close``, ``destroy`` and the state
  triplet). Nothing is judged a reader by its return type or its name: a
  reader is a call the probe sees change nothing while handing back a value.
- **Signatures, from the stub:** the class's ``.pyi`` is its declared Python
  face (jm renders it from the manifests, and it carries the ``# jm:hand``
  members). A member the stub does not declare has nothing to make values
  from, and reads ``UNPROBED``.
- **Two exact rules, from the manifest:**
  - a **handle** -- a method taking a telemetry sink or event log (its
    ``capsule`` is one of ``_HANDLES``), whose other required parameters
    are ``const char *`` labels -- reads ``HANDLE`` and passes: the design
    page's rule is that a handle to the outside world is never in a blob. A
    defaulted parameter configures the attachment (a sink's ``decim``);
  - a **stream call** -- a method whose own ``arg_type`` is not ``void``, or
    jm's built-in ``step``/``steps`` over the component's ``arg_type`` --
    takes the object's input, not a value to keep, so its arguments are
    varied but never required to differ: its effect is measured against the
    uncalled twin, as a call with no value is.
- **C-only setters:** functions in an object's own header, on a non-const
  ``*_state_t *``, named as a mutator (``set_``, ``configure``, ``retune``,
  ``reconfigure``, ``reseed``, ``enable_``), that the type does not bind.
  Python cannot call them, so each is listed ``C_ONLY``.
- **The recipe:** an instance and a feed come from the serialization matrix,
  ``test_state_serialization.CASES``, the one registry that already exists.
  A class with no row there has nothing to probe with, and each of its
  mutators is listed ``NO_RECIPE``.

The probe
---------
Candidate values come from the stub's annotation (a scalar, a ``Literal``, an
array's dtype), the current value, and the object's own lengths; there is no
per-mutator table. A parameter no probe can make (a handle, a path, a string,
an untyped array) reads ``UNPROBED``: "no value could be made" is not "not
state". A method's required parameters are one dimension; each DEFAULTED
parameter is another, varied alone and passed by keyword with the rest at
their defaults, because a new defaulted keyword is how a setter is usually
extended -- and a value it stores and the blob does not carry must read
``LOST``, not pass at its default. A defaulted dimension can only fail the
member: one the probe cannot see (an ``out=`` buffer) is no evidence.

- **Observable:** what an object does is its readback -- a property
  mutator's own, and then EVERY readable property after the continuation,
  since a method has no readback of its own -- its continuation output and
  its blob. Twins built as make, warm-up feed, M(v) and make, warm-up feed,
  M(v2) must differ in one of those: measured between two VALUES, not
  against "M not called", so a mutator that only resets a counter as a side
  effect cannot read as observable. A call with no value to vary (no
  required parameter, or a stream call) is measured against the uncalled
  twin instead, since that is the only other value it has.
- **Values are deduplicated by what the probe observes,** and a property's
  by its readback starting from a FRESH object's: a setter that refuses
  silently (#1987) leaves the readback where it was, and that value is the
  default by another name.
- **Inert:** every value M takes leaves the object exactly as the uncalled
  twin. A call that hands back a value is a reader, ``READS``, if it is
  inert in every row and takes no value of its own to keep: no required
  parameter, a stream call (a pure transform of its input), or jm's
  capacity query ``<m>_max_out`` for a method the manifest declares
  ``variable_output``. Anything else inert -- a setter, or a call taking a
  value it does not visibly keep -- is ``UNPROBED``.
- The blob of the v twin is restored into a default target, a target with
  M(v) applied (the same key), targets at the other values M took, and
  targets at values beside v: a scalar a hair either side, an array of the
  same length with other content, each argument and keyword moved in turn.
  The neighbours are the likeliest to share v's key (a blob size, a ring, a
  plan) while differing from it. **Every target that accepts the blob must
  then match the v twin** in readback, continuation output and blob, or the
  verdict is ``LOST``. A restore that writes M's value only when the target
  still holds the default, or that keeps the target's own value, fails on a
  target at another value.
- A target counts as at ANOTHER value only if its value differs from v's
  (by value, not identity) and it observes differently from a fresh default
  target: a value M silently ignored is no other value.
- ``TRAVELS`` -- the default target accepts the blob and matches, and so
  does a target at another value. The default alone is no evidence: it
  cannot tell a restore that writes v from one that writes it only into a
  target still at the default, so a blob only the default takes reads
  ``UNPROBED``.
- ``KEYED`` -- the default target refuses it and the same-value target
  accepts it and matches: a reject key such as a rate or a mode, or a size,
  as #2041's ``set_acq`` blob restores only into a despreader with the same
  acq-code length. A target beside v that shares the key was held to the
  match like every other.
- ``LOST`` -- an accepting target does not match, or no target accepts.

``NONDETERMINISTIC`` -- two identical builds observe differently: the blob
carries bytes that are not state (uninitialised padding, a buffer's unused
tail), so no restore can be judged until it is a function of the object.

``CRASHES`` -- the probe takes the interpreter down (#2095). A member listed
so is probed in a child process instead of the gate's own. Only a death by a
crash signal (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT; an error-class
NTSTATUS on Windows) after the child's ``PROBING`` sentinel reads CRASHES;
any other exit is an error that fails with the end of the child's stderr,
and an answer reads as itself, so a fix turns the entry stale.

``TRAVELS``, ``KEYED``, ``READS`` and ``HANDLE`` pass. Every other verdict
must be listed, once, with exactly that verdict and a reason, and an entry
that now passes, names no mutator, or carries the wrong verdict fails as
stale. The list may not grow either: ``make tests-ssot`` refuses a key the
merge base did not hold, and a move into LOST, NONDETERMINISTIC or CRASHES
(``check_tests_ssot.mutator_list_added``).
"""

from __future__ import annotations

import ast
import functools
import importlib
import importlib.util
import itertools
import os
import re
import signal
import subprocess
import sys
import warnings
from collections.abc import Callable, Iterable, Iterator
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import pytest

from doppler.tests import test_state_serialization as _matrix
from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from types import ModuleType

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

# Walked up, not counted: the coverage job runs this from a copy two levels
# deeper (build-cov/pkg/doppler), where parents[3] found no objects/ at all.
ROOT = repo_root(__file__)
STUBS = ROOT / "src" / "doppler"

PASSING = ("TRAVELS", "KEYED", "READS", "HANDLE")
LISTABLE = ("LOST", "NONDETERMINISTIC", "UNPROBED", "CRASHES", "C_ONLY")
LISTABLE += ("NO_RECIPE",)

_LIFECYCLE = {"reset", "close", "destroy", "get_state", "set_state"}
_LIFECYCLE |= {"state_bytes"}
_C_MUTATOR = re.compile(
    r"_(set_\w+|configure\w*|retune|reconfigure|reseed\w*|enable_\w+)$"
)
# The handles the design page names as never in a blob, by the capsule type
# a manifest parameter declares: telemetry sinks and event logs.
_HANDLES = frozenset(
    {"doppler.telemetry.dp_tlm", "doppler.telemetry.dp_event_log"}
)

WARM = _matrix._stream(1024, seed=11)
CONT = _matrix._stream(1024, seed=12)


@functools.cache
def _ssot() -> ModuleType:
    """scripts/check_tests_ssot.py: the one reader of the list's format,
    and the one declaration of its path."""
    path = ROOT / "scripts" / "check_tests_ssot.py"
    sys.path.insert(0, str(path.parent))
    try:
        spec = importlib.util.spec_from_file_location("check_tests_ssot", path)
        assert spec is not None and spec.loader is not None
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    finally:
        sys.path.remove(str(path.parent))
    return mod


LIST: Path = _ssot().MUTATOR_LIST


# ── discovery ───────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class Param:
    name: str
    ann: str  # the stub's annotation, as written


@dataclass(frozen=True)
class Mutator:
    cls: str
    comp: str
    name: str
    kind: str  # "property" | "method" | "c"
    # A method's required parameters; None when the stub declares no
    # signature, so there is nothing to make a value from.
    params: tuple[Param, ...] | None = ()
    ann: str = ""  # a property's annotation
    optional: tuple[Param, ...] = ()  # defaulted: each varied by keyword
    stream: bool = False  # its input is the object's stream
    handle: bool = False  # it attaches a handle to the outside world
    accessor: bool = False  # jm's capacity query for a variable_output call

    @property
    def key(self) -> str:
        return f"{self.cls}.{self.name}"


def _camel(comp: str) -> str:
    """jm's class name for a component with no ``class_name``."""
    return "".join(p[:1].upper() + p[1:] for p in comp.split("_"))


@dataclass(frozen=True)
class Declared:
    """A serializable class, and the manifest tables that declare it."""

    cls: str
    comp: str
    cfg: dict[str, Any]
    view: dict[str, Any] | None  # its [[X.views]] entry, if it is a view

    def method(self, name: str) -> dict[str, Any] | None:
        own = (self.view or {}).get("methods", [])
        for m in [*own, *self.cfg.get("methods", [])]:
            if m["name"] == name:
                return dict(m)
        return None


def _serializable() -> Iterator[Declared]:
    """Every serializable class, views included."""
    for path in sorted((ROOT / "objects").glob("*.toml")):
        for comp, cfg in tomllib.loads(path.read_text("utf-8")).items():
            if not isinstance(cfg, dict):
                continue
            if str(cfg.get("serializable", "")).lower() != "true":
                continue
            yield Declared(
                cfg.get("class_name") or _camel(comp), comp, cfg, None
            )
            for view in cfg.get("views", []):
                yield Declared(view["class_name"], comp, cfg, view)


@dataclass(frozen=True)
class Stub:
    """One class as its ``.pyi`` declares it."""

    module: str
    # method -> (required parameters, defaulted parameters)
    methods: dict[str, tuple[tuple[Param, ...], tuple[Param, ...]]]
    getters: dict[str, str]  # property -> its annotation
    setters: frozenset[str]


def _module_of(stub: Path) -> str:
    """The import path a stub types: ``doppler/x/x.pyi`` is the package."""
    parts = list(stub.relative_to(STUBS.parent).with_suffix("").parts)
    return ".".join(parts[:-1] if parts[-1] == parts[-2] else parts)


def _decorated(f: ast.FunctionDef, name: str) -> bool:
    return any(ast.unparse(d) == name for d in f.decorator_list)


def _params(args: Iterable[ast.arg]) -> tuple[Param, ...]:
    return tuple(Param(p.arg, ast.unparse(p.annotation or "")) for p in args)


@functools.cache
def _stubs() -> dict[str, Stub]:
    out: dict[str, Stub] = {}
    for path in sorted(STUBS.rglob("*.pyi")):
        for node in ast.parse(path.read_text("utf-8")).body:
            if not isinstance(node, ast.ClassDef):
                continue
            methods: dict[str, tuple[tuple[Param, ...], tuple[Param, ...]]]
            methods = {}
            getters: dict[str, str] = {}
            setters: set[str] = set()
            for f in node.body:
                if not isinstance(f, ast.FunctionDef):
                    continue
                if _decorated(f, "property"):
                    getters[f.name] = ast.unparse(f.returns or "")
                elif _decorated(f, f"{f.name}.setter"):
                    setters.add(f.name)
                elif f.name not in methods:
                    a = f.args
                    cut = len(a.args) - len(a.defaults)  # self never defaults
                    kw = list(zip(a.kwonlyargs, a.kw_defaults))
                    methods[f.name] = (
                        _params(
                            [*a.args[1:cut], *(k for k, d in kw if d is None)]
                        ),
                        _params(
                            [
                                *a.args[cut:],
                                *(k for k, d in kw if d is not None),
                            ]
                        ),
                    )
            assert node.name not in out, f"{node.name} is in two stubs"
            out[node.name] = Stub(
                _module_of(path), methods, getters, frozenset(setters)
            )
    return out


@functools.cache
def python_type(cls: str) -> type:
    """The class Python gets, from the package its stub types."""
    return getattr(importlib.import_module(_stubs()[cls].module), cls)


def _members(t: type) -> tuple[list[str], list[str]]:
    """(properties, methods): every public attribute of the type."""
    props, methods = [], []
    for name in sorted(dir(t)):
        if name.startswith("_"):
            continue
        raw = next(k.__dict__[name] for k in t.__mro__ if name in k.__dict__)
        if hasattr(raw, "__set__"):
            props.append(name)
        elif callable(raw):
            methods.append(name)
    return props, methods


def _takes_a_write(obj: Any, name: str, declared: bool) -> bool:
    """Whether the type binds a setter: asked by writing the property its
    own value. A property that will not even read falls back to what the
    stub declares."""
    try:
        current = getattr(obj, name)
    except Exception:  # unreadable here: the stub is all there is
        return declared
    try:
        setattr(obj, name, current)
    except AttributeError:  # no setter is bound
        return False
    except Exception:  # a setter, refusing the value it was handed
        return True
    return True


def _writable(cls: str, props: list[str]) -> list[str]:
    """The properties that take a write: asked of an instance when the
    matrix can build one, else of the stub's setters."""
    setters = _stubs()[cls].setters
    recipes = _RECIPES.get(cls)
    if not recipes:
        return [p for p in props if p in setters]
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        return [
            p
            for p in props
            if _takes_a_write(recipes[0][0](), p, p in setters)
        ]


def _is_stream(d: Declared, name: str) -> bool:
    """A call whose input is the object's stream, by the manifest exactly:
    its own non-void ``arg_type``, or jm's built-in step/steps over the
    component's."""
    entry = d.method(name)
    if entry is not None:
        return str(entry.get("arg_type", "void")) != "void"
    built_in = str(d.cfg.get("no_step", "false")).lower() != "true"
    return (
        name in ("step", "steps")
        and built_in
        and str(d.cfg.get("arg_type", "void")) != "void"
    )


def _is_handle(d: Declared, name: str) -> bool:
    """A call that attaches a handle, by the manifest exactly: at least one
    parameter whose capsule is in _HANDLES, and every other REQUIRED one a
    ``const char *`` label. A defaulted one configures the attachment (a
    telemetry sink's ``decim``), which set_state keeps on the target with
    the handle itself."""
    entry = d.method(name) or {}
    ps = entry.get("params") or entry.get("args") or []
    handles = [p for p in ps if p.get("capsule") in _HANDLES]
    rest = [
        p
        for p in ps
        if p not in handles
        and "default" not in p
        and str(p.get("type")) != "const char *"
    ]
    return bool(handles) and not rest


def _is_accessor(d: Declared, name: str) -> bool:
    """jm's capacity query, by the manifest exactly: ``<m>_max_out`` for a
    method ``m`` declared ``variable_output``, which reports how much ``m``
    would write and writes nothing itself."""
    base = (
        d.method(name[: -len("_max_out")])
        if name.endswith("_max_out")
        else None
    )
    return bool(base and base.get("variable_output"))


def _c_only(cls: str, comp: str, bound: set[str]) -> list[Mutator]:
    header = ROOT / "native" / "inc" / "doppler" / comp / f"{comp}_core.h"
    if not header.exists():
        return []
    text = re.sub(r"/\*.*?\*/", "", header.read_text("utf-8"), flags=re.S)
    prefix = f"dp_{comp}_"
    found = []
    decl = re.compile(
        rf"\b(?:void|int)\s+({re.escape(prefix)}\w+)\s*\(\s*(\w+)\s*\*"
    )
    for fn, stype in decl.findall(text):
        if not stype.endswith("_state_t") or fn.endswith("_set_state"):
            continue
        if fn[len(prefix) :] in bound or not _C_MUTATOR.search(fn):
            continue
        found.append(Mutator(cls, comp, fn, "c"))
    return found


def discover() -> list[Mutator]:
    found: list[Mutator] = []
    for d in _serializable():
        stub = _stubs()[d.cls]
        props, methods = _members(python_type(d.cls))
        writable = _writable(d.cls, props)
        for p in writable:
            found.append(
                Mutator(
                    d.cls, d.comp, p, "property", ann=stub.getters.get(p, "")
                )
            )
        for m in methods:
            if m in _LIFECYCLE:
                continue
            sig = stub.methods.get(m)
            found.append(
                Mutator(
                    d.cls,
                    d.comp,
                    m,
                    "method",
                    sig[0] if sig else None,
                    optional=sig[1] if sig else (),
                    stream=_is_stream(d, m),
                    handle=_is_handle(d, m),
                    accessor=_is_accessor(d, m),
                )
            )
        if d.view is None:  # a view shares its parent's core and its header
            # What the type binds, as the C suffix it would carry: a
            # method's own name, and set_<p> for a writable property.
            bound = set(methods) | {f"set_{p}" for p in writable}
            found += _c_only(d.cls, d.comp, bound)
    return found


# ── the recipe: one row of the serialization matrix per class ────────────────


Recipe = tuple[Callable[[], Any], Callable[..., Any]]


def _recipes() -> dict[str, list[Recipe]]:
    """Every matrix row, by the class its make() builds: a class can have
    several (a segmented Dll is where set_symbol_period exists at all)."""
    out: dict[str, list[Recipe]] = {}
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        for make, feed in _matrix.CASES.values():
            out.setdefault(type(make()).__name__, []).append((make, feed))
    return out


# ── probe values: from the stub, the current value and the object ───────────

_ARRAY = re.compile(r"NDArray\[(?:np\.)?(\w+)\]")


@dataclass(frozen=True, eq=False)
class Call:
    """One call's arguments: the required ones positionally, and at most one
    defaulted one by keyword. Compared by _key, never by ``==`` (arrays)."""

    args: tuple[Any, ...]
    kw: tuple[tuple[str, Any], ...] = ()


def _key(v: Any) -> tuple:
    """A value as something comparable by VALUE: an equal call built twice,
    or an array with the same bytes, is the same value."""
    if isinstance(v, Call):
        return ("call", _key(v.args), tuple((k, _key(x)) for k, x in v.kw))
    if isinstance(v, tuple):
        return ("tuple", *(_key(x) for x in v))
    if isinstance(v, np.ndarray):
        return ("array", v.dtype.str, v.shape, v.tobytes())
    return ("scalar", type(v).__name__, repr(v))


def _bare(ann: str) -> str:
    """An annotation with its ``| None`` and module prefixes dropped."""
    parts = [p.strip() for p in ann.replace("npt.", "").split("|")]
    return " | ".join(p for p in parts if p != "None")


def _literal(ann: str) -> list[Any]:
    if not ann.startswith("Literal["):
        return []
    node = ast.parse(ann, mode="eval").body
    assert isinstance(node, ast.Subscript)
    elts = getattr(node.slice, "elts", [node.slice])
    return [ast.literal_eval(e) for e in elts]


def _scalar_candidates(t: str, current: Any = None) -> list[Any]:
    if _literal(t):
        return _literal(t)
    if t == "bool":
        return (
            [not current, bool(current)]
            if current is not None
            else [
                True,
                False,
            ]
        )
    if t == "complex":
        return [0.5 + 0.25j, -1.0 + 0.5j, 0.1 - 0.9j, 2.0 + 0.0j, -0.3 - 0.3j]
    if t == "float":
        x = float(current) if current is not None else 0.0
        base = [x * 1.5 if x else 0.5, x * 0.5, x + 0.25, 0.1, 0.9, 2.0, 0.01]
        base += [4.0, 8.0, 16.0]  # a period or a count in samples
        return [v for v in base if v != x] or base
    if t == "int":
        x = int(current) if current is not None else 0
        base = [x + 1, 2 * x if x else 2, x + 3, 1, 2, 3, 4, 8]
        return [v for v in dict.fromkeys(base) if v != x and v >= 0]
    return []


def _dtype(ann: str) -> np.dtype | None:
    m = _ARRAY.search(ann)
    if not m or m.group(1) == "Any":
        return None
    return np.dtype(m.group(1))


def _array(dt: np.dtype, n: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    if dt.kind == "c":
        z = rng.standard_normal(n) + 1j * rng.standard_normal(n)
        return z.astype(dt)
    if dt.kind == "f":
        return rng.standard_normal(n).astype(dt)
    return rng.integers(0, 2, n).astype(dt)  # a code: chips or bits


def _makeable(ann: str) -> bool:
    a = _bare(ann)
    return _dtype(a) is not None or bool(_scalar_candidates(a))


def _unmakeable(mut: Mutator) -> str:
    """Why no value can be made for M, or "" when one can."""
    if mut.kind == "property":
        return ""  # typed from its current value when the stub is silent
    if mut.params is None:
        return "the stub declares no signature to make a value from"
    for p in mut.params:
        if not _makeable(p.ann):
            return f"a parameter no probe can make: `{p.name}: {p.ann}`"
    return ""


def _int_props(obj: Any) -> list[int]:
    out = []
    for name in dir(type(obj)):
        if name.startswith("_"):
            continue
        try:
            v = getattr(obj, name)
        except Exception:  # a property that refuses to read: not a length
            continue
        if isinstance(v, int) and not isinstance(v, bool) and 0 < v <= 1 << 16:
            out.append(v)
    return out


def _lengths(obj: Any) -> list[int]:
    return list(dict.fromkeys([*_int_props(obj), 4, 8, 16, 31, 64, 127]))


_NEAR_SEED = 7919  # an array beside another: same length, other content
_KW_SEED = 97  # a defaulted array's own content, apart from the required


def _values(ann: str, lengths: list[int], seed: int) -> list[Any]:
    a = _bare(ann)
    dt = _dtype(a)
    if dt is not None:
        return [_array(dt, n, seed=seed + n) for n in lengths]
    return _scalar_candidates(a)


def _calls(mut: Mutator, obj: Any) -> Iterator[Call]:
    """Candidate calls over the required parameters."""
    assert mut.params is not None
    per = [_values(p.ann, _lengths(obj), i) for i, p in enumerate(mut.params)]
    if not per:
        return iter([Call(())])
    # Each param starts at its own offset, so two params of one type are
    # never handed the same value (a pd that must exceed its pfa).
    width = max(len(c) for c in per)
    staggered = (
        tuple(c[(k + i) % len(c)] for i, c in enumerate(per))
        for k in range(width)
    )
    # ...then combinations, for params that constrain each other (pfa < pd).
    combos = itertools.product(*(c[:4] for c in per))
    return (Call(args) for args in itertools.chain(staggered, combos))


def _kw_calls(base: Call, p: Param, obj: Any) -> list[Call]:
    """One defaulted parameter varied, by keyword, over a base call."""
    xs = _values(p.ann, _lengths(obj), _KW_SEED)
    return [Call(base.args, ((p.name, x),)) for x in xs[:8]]


def _near_scalar(v: Any) -> list[Any]:
    if isinstance(v, (bool, str)) or not isinstance(v, (int, float, complex)):
        return []
    if isinstance(v, int):
        return [v + 1, v - 1] if v > 1 else [v + 1]
    return [v * 1.01, v * 0.99, v + 1e-3]


def _near(v: Any) -> list[Any]:
    """Values beside v: the likeliest to share v's key (a blob size, a ring,
    a plan) while differing from it, which is the target a restore that
    keeps the target's own value fails on. A scalar a hair either side; an
    array of the same length with other content; for a call, each argument
    and its keyword moved in turn with the rest held."""
    if isinstance(v, np.ndarray):
        return [_array(v.dtype, v.size, seed=_NEAR_SEED + v.size)]
    if isinstance(v, Call):
        out = [Call(a, v.kw) for a in _near(v.args)]
        for i, (k, x) in enumerate(v.kw):
            for y in _near(x):
                kw = (*v.kw[:i], (k, y), *v.kw[i + 1 :])
                out.append(Call(v.args, kw))
        return out
    if not isinstance(v, tuple):
        return _near_scalar(v)
    out = []
    for i, x in enumerate(v):
        for y in _near(x):
            out.append((*v[:i], y, *v[i + 1 :]))
    return out


# ── the probe ────────────────────────────────────────────────────────────────


@dataclass
class Verdict:
    name: str
    detail: str = ""


def _apply(obj: Any, mut: Mutator, value: Any) -> Any:
    if mut.kind == "property":
        setattr(obj, mut.name, value)
        return None
    return getattr(obj, mut.name)(*value.args, **dict(value.kw))


def _as_bytes(v: Any) -> bytes | None:
    """A property's value as bytes, or None when it is not a value (an
    object whose only rendering is its address)."""
    if isinstance(v, (bytes, bytearray)):
        return bytes(v)
    if isinstance(v, str):
        return v.encode()
    if v is None or isinstance(v, (bool, int, float, complex, np.generic)):
        return np.asarray(v).tobytes()
    if isinstance(v, (np.ndarray, list, tuple)):
        a = np.asarray(v)
        return a.tobytes() if a.dtype != object else repr(v).encode()
    if isinstance(v, dict):
        return repr(sorted(v.items())).encode()
    return None


@functools.cache
def _readable(t: type) -> tuple[str, ...]:
    return tuple(_members(t)[0])


def _readback(obj: Any) -> tuple[tuple[str, bytes], ...]:
    """Every readable property: a method has no readback of its own, so a
    value it stores is only seen through the properties it lands in
    (RateSync.configure's bn and zeta). Taken after the continuation, so a
    readout of the last call (CorrDetector.last_corr) is the continuation's,
    not one a restore was never meant to carry."""
    out = []
    for name in _readable(type(obj)):
        try:
            b = _as_bytes(getattr(obj, name))
        except Exception as e:  # a read that refuses is itself a reading
            b = f"!{type(e).__name__}".encode()
        if b is not None:
            out.append((name, b))
    return tuple(out)


def _snap(obj: Any) -> tuple:
    """What a built object shows without being run: readback and blob."""
    return _readback(obj), obj.get_state()


def _own(obj: Any, mut: Mutator) -> bytes:
    return _as_bytes(getattr(obj, mut.name)) or b""


def _observe(obj: Any, mut: Mutator, feed: Callable[..., Any]) -> tuple:
    """Everything a resumed object must reproduce: a property mutator's own
    readback, the blob, the continuation's output, the blob after it, and
    then every property's readback. Bytes, so -0.0 and NaN compare as what
    they are."""
    own = _own(obj, mut) if mut.kind == "property" else b""
    before = obj.get_state()
    out = np.asarray(feed(obj, CONT)).tobytes()
    return own, before, out, obj.get_state(), _readback(obj)


def _warm(make: Callable[[], Any], feed: Callable[..., Any]) -> Any:
    obj = make()
    feed(obj, WARM)
    return obj


def _built(
    make: Callable[[], Any], feed: Callable[..., Any], mut: Mutator, value: Any
) -> Any:
    obj = _warm(make, feed)
    _apply(obj, mut, value)
    return obj


def _accepted(
    mut: Mutator, make, feed, cands: Iterable[Any]
) -> tuple[list[Any], list[tuple], bool]:
    """Values M takes without raising, deduplicated by what the probe
    observes, with their observations; and whether every call handed back
    a value. A property's are deduplicated by readback too, starting from a
    FRESH object's: a setter that refuses silently (#1987) leaves the
    readback where it was, and that value is no other value."""
    vals: list[Any] = []
    refs: list[tuple] = []
    returned = True
    seen_rb: set[bytes] = set()
    if mut.kind == "property":
        seen_rb.add(_own(make(), mut))
    for v in itertools.islice(cands, 40):
        o = _warm(make, feed)
        try:
            got = _apply(o, mut, v)
        except Exception:  # the mutator refused this value: try the next
            continue
        if mut.kind == "property":
            rb = _own(o, mut)
            if rb in seen_rb:
                continue
            seen_rb.add(rb)
        ref = _observe(o, mut, feed)
        if mut.kind == "method" and ref in refs:
            continue  # observes as a value already taken
        returned = returned and got is not None
        vals.append(v)
        refs.append(ref)
    return vals, refs, returned and mut.kind == "method"


def _restore_into(target: Any, blob: bytes) -> bool:
    try:
        target.set_state(blob)
    except (ValueError, TypeError):
        return False
    return True


def _show(v: Any) -> str:
    """A value as a failure message can carry it: an array by its shape."""
    if isinstance(v, Call):
        parts = [_show(x) for x in v.args]
        parts += [f"{k}={_show(x)}" for k, x in v.kw]
        return "(" + ", ".join(parts) + ")"
    if isinstance(v, tuple):
        return "(" + ", ".join(_show(x) for x in v) + ")"
    if isinstance(v, np.ndarray):
        return f"<{v.dtype}[{v.size}]>"
    return repr(v)


def _restore_check(
    mut: Mutator, make, feed, v: Any, others: list[Any], valued: bool
) -> Verdict:
    """Restore the v twin's blob into every target and judge the result."""
    ref_v = _observe(_built(make, feed, mut, v), mut, feed)
    blob = _built(make, feed, mut, v).get_state()
    fresh = _snap(make())
    # Every target that ACCEPTS the blob must then be the v twin: a default
    # one, one already at v (the same key), one at each other value M
    # takes, and one at each value beside v, which share its key and differ.
    targets: list[tuple[str, Any]] = [("default", make())]
    # Ten other values are plenty to find one that shares v's key; past that
    # an expensive object (an acquirer) only pays for repetition.
    keys = {_key(v)}
    cands: list[tuple[str, Any]] = [("same", v)]
    for c in [*others[:10], *_near(v)]:
        if _key(c) not in keys:  # by value: an equal call is no other
            keys.add(_key(c))
            cands.append((f"M{_show(c)}", c))
    other: list[str] = []
    for label, val in cands:
        t = make()
        try:
            _apply(t, mut, val)
        except Exception:  # a value M refuses is no target
            continue
        # A target M left looking like a fresh default -- a value it
        # silently ignored -- is the default by another name: held to the
        # match below, but no evidence of another value.
        if label != "same" and _snap(t) != fresh:
            other.append(label)
        targets.append((label, t))
    accepted = {label: _restore_into(t, blob) for label, t in targets}
    for label, t in targets:
        if accepted[label] and _observe(t, mut, feed) != ref_v:
            return Verdict(
                "LOST",
                f"blob at M{_show(v)}: the {label} target accepts it, "
                "then differs",
            )
    # A default target that takes the blob shows the value travels only
    # with a target at ANOTHER value beside it: the default alone cannot
    # tell a restore that writes v from one that writes it only into a
    # target still at the default (sabotage d), and a mutator every other
    # target refused to take would pass on it alone. With no value to vary
    # (no required parameter, or a stream call's input), the uncalled
    # default is that other value.
    evidence = [lab for lab in other if accepted[lab]]
    if not valued:
        evidence.append("default")
    if accepted["default"]:
        if evidence:
            return Verdict("TRAVELS")
        return Verdict(
            "UNPROBED",
            f"blob at M{_show(v)}: only the default target accepts it, so "
            "nothing shows the restore writes the value",
        )
    # Refused by the default, taken at the same value: a key. Any target at
    # another value that shared it was held to the match above.
    if accepted.get("same"):
        return Verdict("KEYED")
    return Verdict(
        "LOST",
        f"blob at M{_show(v)}: no target accepts it, not even the same key",
    )


def _judge(
    mut: Mutator,
    make,
    feed,
    cands: Iterable[Any],
    *,
    valued: bool,
    may_read: bool,
) -> tuple[Verdict, list[Any]]:
    """One dimension's verdict, and the values it accepted."""
    vals, refs, returned = _accepted(mut, make, feed, cands)
    if not vals:
        return Verdict("UNPROBED", "no value the mutator accepts"), vals
    # Two builds of the same object, fed the same, must observe the same: a
    # blob that differs between them carries bytes that are not state
    # (uninitialised padding or buffer tails), and no restore can be judged.
    if _observe(_built(make, feed, mut, vals[0]), mut, feed) != refs[0]:
        return Verdict(
            "NONDETERMINISTIC",
            "two identical builds differ: the blob carries bytes that are "
            "not state",
        ), vals
    uncalled = _observe(_warm(make, feed), mut, feed)
    if all(r == uncalled for r in refs):
        if returned and may_read:
            return Verdict("READS"), vals
        return Verdict(
            "UNPROBED",
            "no value it accepts changes anything the probe can see",
        ), vals
    if valued and len(set(refs)) < 2:
        return Verdict(
            "UNPROBED",
            "no two accepted values differ in readback, output or blob",
        ), vals
    # Each of the first values takes a turn as v: whether a restore keeps
    # the target's own value can depend on v (a rate whose plan, and so blob
    # size, only some neighbours share), so one v is not enough.
    seen = [_restore_check(mut, make, feed, v, vals, valued) for v in vals[:4]]
    for verdict in seen:
        if verdict.name == "LOST":
            return verdict, vals
    for name in ("KEYED", "TRAVELS"):
        if any(verdict.name == name for verdict in seen):
            return Verdict(name), vals
    return seen[0], vals


def probe(mut: Mutator, make, feed) -> Verdict:
    why = _unmakeable(mut)
    if why:
        return Verdict("UNPROBED", why)
    if mut.kind == "property":
        current = getattr(_warm(make, feed), mut.name)
        cands: list[Any] = _scalar_candidates(_bare(mut.ann), current)
        if not cands:  # the stub is silent or vague: type the value itself
            for kind in (bool, int, float, complex):
                if isinstance(current, kind):
                    cands = _scalar_candidates(kind.__name__, current)
                    break
        return _judge(mut, make, feed, cands, valued=True, may_read=False)[0]
    obj = _warm(make, feed)
    verdict, vals = _judge(
        mut,
        make,
        feed,
        _calls(mut, obj),
        valued=bool(mut.params) and not mut.stream,
        # An inert call taking a value it does not visibly keep is no
        # reader: it may be a setter the recipe cannot see. Only a call
        # with no value, a stream call (a pure transform of its input), or
        # jm's declared capacity query reads.
        may_read=not mut.params or mut.stream or mut.accessor,
    )
    if not vals:
        return verdict
    # Each defaulted parameter, alone, over the first accepted call. Only a
    # failure counts: a keyword the probe cannot see change anything (an
    # out= buffer) shows nothing either way.
    for p in mut.optional:
        if not _makeable(p.ann):
            continue
        kw, _ = _judge(
            mut,
            make,
            feed,
            _kw_calls(vals[0], p, obj),
            valued=True,
            may_read=False,
        )
        if kw.name in ("LOST", "NONDETERMINISTIC"):
            return Verdict(kw.name, f"{p.name}=: {kw.detail}")
    return verdict


# ── the list ────────────────────────────────────────────────────────────────


def _entries() -> list[tuple[str, str, str]]:
    return _ssot().parse_mutator_list(LIST.read_text("utf-8"))


def _listed() -> dict[str, tuple[str, str]]:
    return {k: (v, r) for k, v, r in _entries()}


_RECIPES = _recipes()
_MUTATORS = discover()


def _aggregate(seen: list[Verdict]) -> Verdict:
    """One verdict from every row's. A failure in any row is the verdict,
    and a pass needs a row that probed. READS needs EVERY row inert: a call
    one row sees write is no reader for being inert in another."""
    for name in ("NONDETERMINISTIC", "LOST"):
        for verdict in seen:
            if verdict.name == name:
                return verdict
    for name in ("KEYED", "TRAVELS"):
        if any(v.name == name for v in seen):
            return Verdict(name)
    if all(v.name == "READS" for v in seen):
        return Verdict("READS")
    return next(v for v in seen if v.name != "READS")


def _verdict(mut: Mutator) -> Verdict:
    if mut.kind == "c":
        return Verdict("C_ONLY", "no Python face to probe")
    if mut.handle:
        return Verdict("HANDLE")
    recipes = _RECIPES.get(mut.cls)
    if not recipes:
        return Verdict("NO_RECIPE", "no row in test_state_serialization.CASES")
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        return _aggregate([probe(mut, make, feed) for make, feed in recipes])


_CHILD = """\
import sys, warnings
warnings.simplefilter("ignore")
from doppler.tests import test_mutator_state as g
m = next(m for m in g._MUTATORS if m.key == sys.argv[1])
print("PROBING", flush=True)
v = g._verdict(m)
print("VERDICT", v.name, flush=True)
print(v.detail, flush=True)
"""

# A death the PROBE caused: a fault in the C it called, or an abort from a
# check inside it. Not SIGKILL or SIGTERM, which come from outside (an OOM
# kill, a timeout), and not an exit status, which is Python failing.
_CRASH_SIGNALS = frozenset(
    getattr(signal, s)
    for s in ("SIGSEGV", "SIGBUS", "SIGILL", "SIGFPE", "SIGABRT")
    if hasattr(signal, s)
)


def _died_of_a_crash(returncode: int) -> bool:
    if os.name == "nt":  # an error-class NTSTATUS: 0xC0000005 and kin
        return returncode >= 0xC0000000
    return returncode < 0 and -returncode in _CRASH_SIGNALS


def _verdict_in_a_child(mut: Mutator) -> Verdict:
    """The verdict of a mutator listed CRASHES, from a process of its own.

    Its probe takes the interpreter down with it (#2095), so it cannot run
    in the gate's process. Asked in a child, a crash-signal death after the
    ``PROBING`` sentinel is CRASHES; any other failure is an error carrying
    the END of the child's stderr; and an answer is whatever the probe now
    reads, which makes a fixed entry stale like any other."""
    import doppler

    env = dict(os.environ)
    # The child imports the same doppler as this process: the coverage job
    # runs from a copied tree that nothing else on the path names.
    pkg = str(Path(doppler.__file__).resolve().parents[1])
    env["PYTHONPATH"] = os.pathsep.join(
        [pkg, *filter(None, [env.get("PYTHONPATH")])]
    )
    run = subprocess.run(
        [sys.executable, "-c", _CHILD, mut.key],
        capture_output=True,
        text=True,
        env=env,
        timeout=600,
    )
    out = run.stdout.splitlines()
    if "PROBING" in out and _died_of_a_crash(run.returncode):
        return Verdict("CRASHES", f"died by exit {run.returncode}")
    if run.returncode != 0 or not any(x.startswith("VERDICT ") for x in out):
        tail = " | ".join(run.stderr.strip().splitlines()[-3:])
        return Verdict(
            "ERROR",
            f"the child exited {run.returncode} without a crash signal: "
            f"{tail}",
        )
    at = next(i for i, x in enumerate(out) if x.startswith("VERDICT "))
    return Verdict(out[at].split(" ", 1)[1], "".join(out[at + 1 : at + 2]))


@pytest.mark.parametrize("mut", _MUTATORS, ids=lambda m: m.key)
def test_a_mutator_value_is_state_or_listed_exactly(mut: Mutator) -> None:
    listed = _listed().get(mut.key)
    if listed is not None and listed[0] == "CRASHES":
        got = _verdict_in_a_child(mut)
    else:
        got = _verdict(mut)
    if got.name in PASSING:
        assert listed is None, (
            f"{mut.key} now {got.name}: delete its stale entry "
            f"'{listed[0]}' from {LIST.name}"
        )
        return
    assert listed is not None, (
        f"{mut.key}: {got.name} ({got.detail}). Its value is not shown to "
        f"travel in the state blob. Make get_state/set_state carry it, or "
        f"list it in {LIST.name} as `{mut.key}  {got.name}  <reason>`."
    )
    assert listed[0] == got.name, (
        f"{mut.key} is listed {listed[0]} but reads {got.name} ({got.detail})"
    )
    assert listed[1], f"{mut.key}: a listed {got.name} needs a reason"


def test_discovery_sees_every_matrix_class() -> None:
    """Fail closed: every class the serialization matrix builds must be one
    discovery found. A root that misses objects/ finds no classes, and the
    parametrized gate above then has nothing to check -- a pass by absence,
    which is what the coverage job's copied tree produced."""
    found = {d.cls for d in _serializable()}
    assert found, f"no serializable class found under {ROOT / 'objects'}"
    missing = sorted(c for c in _RECIPES if c not in found)
    assert not missing, f"matrix classes discovery missed: {missing}"
    assert _MUTATORS, "no mutator discovered"


def test_every_serializable_class_resolves_to_its_type_and_stub() -> None:
    """Discovery reads the type Python gets and the stub that declares it;
    a class missing either would contribute no mutators at all."""
    unresolved = []
    for d in _serializable():
        try:
            assert isinstance(python_type(d.cls), type)
        except (KeyError, AttributeError, ImportError) as e:
            unresolved.append(f"{d.cls}: {e!r}")
    assert not unresolved, unresolved


def test_every_entry_names_a_mutator() -> None:
    known = {m.key for m in _MUTATORS}
    stale = sorted(k for k in _listed() if k not in known)
    assert not stale, f"{LIST.name} names no such mutator: {stale}"


def test_the_list_names_each_mutator_once() -> None:
    """A key written twice is two claims about one mutator, and only one
    of them can be its verdict: refused, rather than the last one winning."""
    keys = [k for k, _, _ in _entries()]
    twice = sorted({k for k in keys if keys.count(k) > 1})
    assert not twice, f"{LIST.name} lists these more than once: {twice}"


def test_the_list_only_holds_listable_verdicts() -> None:
    bad = {k: v for k, (v, _) in _listed().items() if v not in LISTABLE}
    assert not bad, f"verdicts a list may not hold: {bad}"


@pytest.mark.parametrize(
    ("returncode", "crash"),
    [
        (-int(signal.SIGSEGV), True),
        (-int(signal.SIGABRT), True),
        (-int(signal.SIGKILL) if hasattr(signal, "SIGKILL") else -9, False),
        (1, False),  # an uncaught Python exception
        (0, False),
    ],
)
def test_only_a_crash_signal_reads_crashes(
    returncode: int, crash: bool
) -> None:
    """An OOM kill (SIGKILL) and a Python failure (exit 1) are not the
    probe crashing, so neither may read as the listed CRASHES."""
    if os.name == "nt":
        pytest.skip("POSIX signal numbers")
    assert _died_of_a_crash(returncode) is crash
