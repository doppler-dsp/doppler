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
state".

- **Observable:** what an object does is its readback -- a property
  mutator's own, and then EVERY readable property after the continuation,
  since a method has no readback of its own -- its continuation output and
  its blob. Twins built as make, warm-up feed,
  M(v) and make, warm-up feed, M(v2) must differ in one of those: measured
  between two VALUES, not against "M not called", so a mutator that only
  resets a counter as a side effect cannot read as observable. A call with
  no value to vary (no required parameter) is measured against the uncalled
  twin instead, since that is the only other value it has.
- **Inert:** every value M takes leaves the object exactly as the uncalled
  twin. With a value handed back that is a reader, ``READS``; with none it
  is a setter whose effect the recipe cannot see, ``UNPROBED``.
- The blob of the v twin is restored into a default target, a target with
  M(v) applied (the same key), targets at the other values M took, and
  targets at values beside v: a scalar a hair either side, an array of the
  same length with other content. The neighbours are the likeliest to share
  v's key (a blob size, a ring, a plan) while differing from it. **Every
  target that accepts the blob must then match the v twin** in readback,
  continuation output and blob, or the verdict is ``LOST``. A restore that
  writes M's value only when the target still holds the default, or that
  keeps the target's own value, fails on a target at another value.
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
so is probed in a child process instead of the gate's own, where a death
reads CRASHES and an answer reads as itself, so a fix turns the entry stale.

``TRAVELS``, ``KEYED`` and ``READS`` pass. Every other verdict must be
listed, once, with exactly that verdict and a reason, and an entry that now
passes, names no mutator, or carries the wrong verdict fails as stale. The
list may not grow either: ``make tests-ssot`` refuses an entry the merge
base did not hold (``check_tests_ssot.mutator_list_added``).
"""

from __future__ import annotations

import ast
import functools
import importlib
import importlib.util
import itertools
import os
import re
import subprocess
import sys
import warnings
from collections.abc import Callable, Iterator
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
LIST = ROOT / "scripts" / ".mutator-state-exempt"
STUBS = ROOT / "src" / "doppler"

PASSING = ("TRAVELS", "KEYED", "READS")
LISTABLE = ("LOST", "NONDETERMINISTIC", "UNPROBED", "CRASHES", "C_ONLY")
LISTABLE += ("NO_RECIPE",)

_LIFECYCLE = {"reset", "close", "destroy", "get_state", "set_state"}
_LIFECYCLE |= {"state_bytes"}
_C_MUTATOR = re.compile(
    r"_(set_\w+|configure\w*|retune|reconfigure|reseed\w*|enable_\w+)$"
)

WARM = _matrix._stream(1024, seed=11)
CONT = _matrix._stream(1024, seed=12)


@functools.cache
def _ssot() -> ModuleType:
    """scripts/check_tests_ssot.py: the one reader of the list's format."""
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

    @property
    def key(self) -> str:
        return f"{self.cls}.{self.name}"


def _camel(comp: str) -> str:
    """jm's class name for a component with no ``class_name``."""
    return "".join(p[:1].upper() + p[1:] for p in comp.split("_"))


def _serializable() -> Iterator[tuple[str, str, bool]]:
    """(class, component, is a view) for every serializable class."""
    for path in sorted((ROOT / "objects").glob("*.toml")):
        for comp, cfg in tomllib.loads(path.read_text("utf-8")).items():
            if not isinstance(cfg, dict):
                continue
            if str(cfg.get("serializable", "")).lower() != "true":
                continue
            yield cfg.get("class_name") or _camel(comp), comp, False
            for view in cfg.get("views", []):
                yield view["class_name"], comp, True


@dataclass(frozen=True)
class Stub:
    """One class as its ``.pyi`` declares it."""

    module: str
    methods: dict[str, tuple[Param, ...]]
    getters: dict[str, str]  # property -> its annotation
    setters: frozenset[str]


def _module_of(stub: Path) -> str:
    """The import path a stub types: ``doppler/x/x.pyi`` is the package."""
    parts = list(stub.relative_to(STUBS.parent).with_suffix("").parts)
    return ".".join(parts[:-1] if parts[-1] == parts[-2] else parts)


def _decorated(f: ast.FunctionDef, name: str) -> bool:
    return any(ast.unparse(d) == name for d in f.decorator_list)


@functools.cache
def _stubs() -> dict[str, Stub]:
    out: dict[str, Stub] = {}
    for path in sorted(STUBS.rglob("*.pyi")):
        for node in ast.parse(path.read_text("utf-8")).body:
            if not isinstance(node, ast.ClassDef):
                continue
            methods: dict[str, tuple[Param, ...]] = {}
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
                    # Required parameters only: a defaulted one (an ``out=``
                    # buffer, an optional mode) keeps its default.
                    pos = a.args[1 : len(a.args) - len(a.defaults)]
                    kw = [
                        k
                        for k, d in zip(a.kwonlyargs, a.kw_defaults)
                        if d is None
                    ]
                    methods[f.name] = tuple(
                        Param(p.arg, ast.unparse(p.annotation or ""))
                        for p in [*pos, *kw]
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
    for cls, comp, is_view in _serializable():
        stub = _stubs()[cls]
        props, methods = _members(python_type(cls))
        writable = _writable(cls, props)
        for p in writable:
            found.append(
                Mutator(cls, comp, p, "property", ann=stub.getters.get(p, ""))
            )
        for m in methods:
            if m not in _LIFECYCLE:
                found.append(
                    Mutator(cls, comp, m, "method", stub.methods.get(m))
                )
        if not is_view:  # a view shares its parent's core and its header
            # What the type binds, as the C suffix it would carry: a
            # method's own name, and set_<p> for a writable property.
            bound = set(methods) | {f"set_{p}" for p in writable}
            found += _c_only(cls, comp, bound)
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


_NEAR_SEED = 7919  # an array beside another: same length, other content


def _method_args(mut: Mutator, obj: Any) -> Iterator[tuple[Any, ...]]:
    """Candidate argument tuples, for a method whose params can be made."""
    assert mut.params is not None
    per: list[list[Any]] = []
    lengths = list(dict.fromkeys([*_int_props(obj), 4, 8, 16, 31, 64, 127]))
    for i, p in enumerate(mut.params):
        a = _bare(p.ann)
        dt = _dtype(a)
        if dt is not None:
            per.append([_array(dt, n, seed=i + n) for n in lengths])
        else:
            per.append(_scalar_candidates(a))
    if not per:
        return iter([()])
    # Each param starts at its own offset, so two params of one type are
    # never handed the same value (a pd that must exceed its pfa).
    width = max(len(c) for c in per)
    staggered = (
        tuple(c[(k + i) % len(c)] for i, c in enumerate(per))
        for k in range(width)
    )
    # ...then combinations, for params that constrain each other (pfa < pd).
    combos = itertools.product(*(c[:4] for c in per))
    return itertools.chain(staggered, combos)


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
    moved in turn with the rest held."""
    if isinstance(v, np.ndarray):
        return [_array(v.dtype, v.size, seed=_NEAR_SEED + v.size)]
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
    return getattr(obj, mut.name)(*value)


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


def _observe(obj: Any, mut: Mutator, feed: Callable[..., Any]) -> tuple:
    """Everything a resumed object must reproduce: a property mutator's own
    readback, the blob, the continuation's output, the blob after it, and
    then every property's readback. Bytes, so -0.0 and NaN compare as what
    they are."""
    own = b""
    if mut.kind == "property":
        own = _as_bytes(getattr(obj, mut.name)) or b""
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


def _accepted_values(mut: Mutator, make, feed) -> tuple[list[Any], bool]:
    """Values M takes without raising, readback distinct for a property;
    and whether every call handed back a value."""
    probe = _warm(make, feed)
    if mut.kind == "property":
        current = getattr(probe, mut.name)
        cands: Any = _scalar_candidates(_bare(mut.ann), current)
        if not cands:  # the stub is silent or vague: type the value itself
            for kind in (bool, int, float, complex):
                if isinstance(current, kind):
                    cands = _scalar_candidates(kind.__name__, current)
                    break
    else:
        cands = _method_args(mut, probe)
    vals: list[Any] = []
    returned = True
    seen: set[bytes] = set()
    for v in itertools.islice(cands, 40):
        o = _warm(make, feed)
        try:
            got = _apply(o, mut, v)
        except Exception:  # the mutator refused this value: try the next
            continue
        if mut.kind == "property":
            rb = _as_bytes(getattr(o, mut.name)) or b""
            if rb in seen:
                continue
            seen.add(rb)
        returned = returned and got is not None
        vals.append(v)
    return vals, returned and mut.kind == "method"


def _restore_into(target: Any, blob: bytes) -> bool:
    try:
        target.set_state(blob)
    except (ValueError, TypeError):
        return False
    return True


def _show(v: Any) -> str:
    """A value as a failure message can carry it: an array by its shape."""
    if isinstance(v, tuple):
        return "(" + ", ".join(_show(x) for x in v) + ")"
    if isinstance(v, np.ndarray):
        return f"<{v.dtype}[{v.size}]>"
    return repr(v)


def _restore_check(
    mut: Mutator, make, feed, v: Any, others: list[Any]
) -> Verdict:
    """Restore the v twin's blob into every target and judge the result."""
    ref_v = _observe(_built(make, feed, mut, v), mut, feed)
    blob = _built(make, feed, mut, v).get_state()
    # Every target that ACCEPTS the blob must then be the v twin: a default
    # one, one already at v (the same key), one at each other value M
    # takes, and one at each value beside v, which share its key and differ.
    targets: list[tuple[str, Any]] = [("default", make())]
    # Ten other values are plenty to find one that shares v's key; past that
    # an expensive object (an acquirer) only pays for repetition.
    others = [c for c in others if c is not v][:10]
    candidates = [("same", v)]
    candidates += [(f"M{_show(c)}", c) for c in others]
    candidates += [(f"M{_show(c)}, beside v", c) for c in _near(v)]
    for label, val in candidates:
        t = make()
        try:
            _apply(t, mut, val)
        except Exception:  # a value M refuses is no target
            continue
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
    # target refused to take would pass on it alone. With no value to vary,
    # the uncalled default is that other value.
    other = [
        lab
        for lab in accepted
        if accepted[lab] and lab not in ("default", "same")
    ]
    if v == () and mut.kind == "method":
        other.append("default")
    if accepted["default"]:
        if other:
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


def probe(mut: Mutator, make, feed) -> Verdict:
    why = _unmakeable(mut)
    if why:
        return Verdict("UNPROBED", why)
    vals, returned = _accepted_values(mut, make, feed)
    if not vals:
        return Verdict("UNPROBED", "no value the mutator accepts")
    uncalled = _observe(_warm(make, feed), mut, feed)
    refs = [_observe(_built(make, feed, mut, c), mut, feed) for c in vals]
    # Two builds of the same object, fed the same, must observe the same: a
    # blob that differs between them carries bytes that are not state
    # (uninitialised padding or buffer tails), and no restore can be judged.
    if _observe(_built(make, feed, mut, vals[0]), mut, feed) != refs[0]:
        return Verdict(
            "NONDETERMINISTIC",
            "two identical builds differ: the blob carries bytes that are "
            "not state",
        )
    if all(r == uncalled for r in refs):
        if returned:
            return Verdict("READS")
        return Verdict(
            "UNPROBED",
            "no value it accepts changes anything the probe can see",
        )
    valued = mut.kind == "property" or bool(mut.params)
    if valued and len(set(refs)) < 2:
        return Verdict(
            "UNPROBED",
            "no two accepted values differ in readback, output or blob",
        )
    # Each of the first values takes a turn as v: whether a restore keeps
    # the target's own value can depend on v (a rate whose plan, and so blob
    # size, only some neighbours share), so one v is not enough.
    seen = [_restore_check(mut, make, feed, v, vals) for v in vals[:4]]
    for verdict in seen:
        if verdict.name == "LOST":
            return verdict
    for name in ("KEYED", "TRAVELS"):
        if any(verdict.name == name for verdict in seen):
            return Verdict(name)
    return seen[0]


# ── the list ────────────────────────────────────────────────────────────────


def _entries() -> list[tuple[str, str, str]]:
    return _ssot().parse_mutator_list(LIST.read_text("utf-8"))


def _listed() -> dict[str, tuple[str, str]]:
    return {k: (v, r) for k, v, r in _entries()}


_RECIPES = _recipes()
_MUTATORS = discover()


def _verdict(mut: Mutator) -> Verdict:
    if mut.kind == "c":
        return Verdict("C_ONLY", "no Python face to probe")
    recipes = _RECIPES.get(mut.cls)
    if not recipes:
        return Verdict("NO_RECIPE", "no row in test_state_serialization.CASES")
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        seen = [probe(mut, make, feed) for make, feed in recipes]
    # Any row's failure is the verdict; a pass needs a row that probed.
    for name in ("NONDETERMINISTIC", "LOST"):
        for verdict in seen:
            if verdict.name == name:
                return verdict
    for name in PASSING:
        if any(v.name == name for v in seen):
            return Verdict(name)
    return seen[0]


_CHILD = """\
import sys, warnings
warnings.simplefilter("ignore")
from doppler.tests import test_mutator_state as g
v = g._verdict(next(m for m in g._MUTATORS if m.key == sys.argv[1]))
print(v.name)
print(v.detail)
"""


def _verdict_in_a_child(mut: Mutator) -> Verdict:
    """The verdict of a mutator listed CRASHES, from a process of its own.

    Its probe takes the interpreter down with it (#2095), so it cannot run
    in the gate's process; asked in a child, a death is CRASHES and an
    answer is whatever the probe now reads, which makes a fixed entry stale
    like any other."""
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
    if run.returncode != 0:
        tail = (run.stderr.strip().splitlines() or [""])[0]
        return Verdict("CRASHES", f"exit {run.returncode}: {tail}")
    name, detail = [*run.stdout.splitlines(), "", ""][:2]
    return Verdict(name, detail)


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
    found = {cls for cls, *_ in _serializable()}
    assert found, f"no serializable class found under {ROOT / 'objects'}"
    missing = sorted(c for c in _RECIPES if c not in found)
    assert not missing, f"matrix classes discovery missed: {missing}"
    assert _MUTATORS, "no mutator discovered"


def test_every_serializable_class_resolves_to_its_type_and_stub() -> None:
    """Discovery reads the type Python gets and the stub that declares it;
    a class missing either would contribute no mutators at all."""
    unresolved = []
    for cls, *_ in _serializable():
        try:
            assert isinstance(python_type(cls), type)
        except (KeyError, AttributeError, ImportError) as e:
            unresolved.append(f"{cls}: {e!r}")
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
