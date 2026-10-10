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
  included.
- **Mutators, by structure, not by name:** every writable property, and every
  manifest method that writes the object and hands back no stream or value --
  no input stream (``arg_type = "void"``), no output array, a ``void`` or
  status return, and not part of the lifecycle. A value-returning call is a
  reader.
- **C-only setters:** functions in an object's own header, on a non-const
  ``*_state_t *``, named as a mutator (``set_``, ``configure``, ``retune``,
  ``reconfigure``, ``reseed``, ``enable_``), which no manifest entry binds.
  Python cannot call them, so each is listed ``C_ONLY``.
- **The recipe:** an instance and a feed come from the serialization matrix,
  ``test_state_serialization.CASES``, the one registry that already exists.
  A class with no row there has nothing to probe with, and each of its
  mutators is listed ``NO_RECIPE``.

The probe
---------
For a mutator M, two values v and v2 that M accepts and that differ in their
readback (a property) or in what the object then does (a method) are chosen
from the type, the current value and the manifest; no per-mutator table.

- **Observable:** twins built as make, warm-up feed, M(v), and make, warm-up
  feed, M(v2), must differ in readback, continuation output or blob. Measured
  between two VALUES, not against "M not called", so a mutator that only
  resets a counter as a side effect cannot read as observable. If no two
  candidates differ, the verdict is ``UNPROBED`` -- never a pass.
- The blob of the v twin is restored into three targets: a default one, one
  with M(v) applied (the same key), and one with M(v2) applied (another
  value). **Every target that accepts the blob must then match the v twin**
  -- readback, continuation output, blob -- or the verdict is ``LOST``. A
  restore that writes M's value only when the target still holds the
  default, or that a target-side M(v2) overrides, fails on the third target.
- ``TRAVELS`` -- the default target accepts the blob and matches.
- ``KEYED`` -- the default target refuses it, and the same-key target accepts
  it and matches: a reject key such as a rate or a mode, or a size, as
  #2041's ``set_acq`` blob restores only into a despreader with the same
  acq-code length.
- ``LOST`` -- an accepting target does not match, or no target accepts.

``NONDETERMINISTIC`` -- two identical builds observe differently: the blob
carries bytes that are not state (uninitialised padding, a buffer's unused
tail), so no restore can be judged until it is a function of the object.

``TRAVELS`` and ``KEYED`` pass. Every other verdict must be listed, with
exactly that verdict and a reason, and an entry that now passes, names no
mutator, or carries the wrong verdict fails as stale.
"""

from __future__ import annotations

import itertools
import re
import sys
import warnings
from collections.abc import Callable, Iterator
from dataclasses import dataclass
from typing import Any

import numpy as np
import pytest

from doppler.tests import test_state_serialization as _matrix
from doppler.tests._repo import repo_root

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

# Walked up, not counted: the coverage job runs this from a copy two levels
# deeper (build-cov/pkg/doppler), where parents[3] found no objects/ at all.
ROOT = repo_root(__file__)
LIST = ROOT / "scripts" / ".mutator-state-exempt"

PASSING = ("TRAVELS", "KEYED")
LISTABLE = ("LOST", "NONDETERMINISTIC", "UNPROBED", "EXEMPT", "C_ONLY")
LISTABLE += ("NO_RECIPE",)

_LIFECYCLE = {"reset", "close", "destroy", "get_state", "set_state"}
_LIFECYCLE |= {"state_bytes"}
_STREAM_KEYS = (
    "variable_output",
    "out_type",
    "multi_output",
    "borrow",
    "single",
    "batch",
    "returns",
)
_C_MUTATOR = re.compile(
    r"_(set_\w+|configure\w*|retune|reconfigure|reseed\w*|enable_\w+)$"
)

WARM = _matrix._stream(1024, seed=11)
CONT = _matrix._stream(1024, seed=12)


# ── discovery ───────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class Mutator:
    cls: str
    comp: str
    name: str
    kind: str  # "property" | "method" | "c"
    params: tuple[dict[str, Any], ...] = ()
    ptype: str = ""
    choices: tuple[str, ...] = ()

    @property
    def key(self) -> str:
        return f"{self.cls}.{self.name}"


def _camel(comp: str) -> str:
    """jm's class name for a component with no ``class_name``."""
    return "".join(p[:1].upper() + p[1:] for p in comp.split("_"))


def _choices(t: str) -> tuple[str, ...]:
    return tuple(t.split(":", 1)[1].split(",")) if ":" in t else ()


def _serializable() -> Iterator[tuple[str, str, dict[str, Any], bool]]:
    for path in sorted((ROOT / "objects").glob("*.toml")):
        for comp, cfg in tomllib.loads(path.read_text("utf-8")).items():
            if not isinstance(cfg, dict):
                continue
            if str(cfg.get("serializable", "")).lower() != "true":
                continue
            yield cfg.get("class_name") or _camel(comp), comp, cfg, False
            for view in cfg.get("views", []):
                trimmed = dict(cfg)
                gone_p = set(view.get("exclude_properties", []))
                gone_m = set(view.get("exclude_methods", []))
                trimmed["properties"] = [
                    p
                    for p in cfg.get("properties", [])
                    if p["name"] not in gone_p
                ]
                trimmed["methods"] = [
                    m
                    for m in cfg.get("methods", [])
                    if m["name"] not in gone_m
                ]
                yield view["class_name"], comp, trimmed, True


def _is_mutator_method(m: dict[str, Any]) -> bool:
    if m["name"] in _LIFECYCLE:
        return False
    if str(m.get("arg_type", "void")) != "void":
        return False  # a stream method: its effect is running state
    if any(m.get(k) for k in _STREAM_KEYS):
        return False
    # A value-returning call is a reader; a status return is a mutator's.
    return str(m.get("return_type", "void")) == "void" or bool(
        m.get("status_return")
    )


def _bound_c_names(comp: str, cfg: dict[str, Any]) -> set[str]:
    names = set()
    for m in cfg.get("methods", []):
        names.add(m.get("fn") or f"dp_{comp}_{m['name']}")
    for p in cfg.get("properties", []):
        names.add(f"dp_{comp}_get_{p['name']}")
        if p.get("writable"):  # a read-only property binds no setter
            names.add(f"dp_{comp}_set_{p['name']}")
    return names


def _c_only(cls: str, comp: str, cfg: dict[str, Any]) -> list[Mutator]:
    header = ROOT / "native" / "inc" / "doppler" / comp / f"{comp}_core.h"
    if not header.exists():
        return []
    text = re.sub(r"/\*.*?\*/", "", header.read_text("utf-8"), flags=re.S)
    bound = _bound_c_names(comp, cfg)
    found = []
    decl = re.compile(
        rf"\b(?:void|int)\s+(dp_{re.escape(comp)}_\w+)\s*\(\s*(\w+)\s*\*"
    )
    for fn, stype in decl.findall(text):
        unbound = stype.endswith("_state_t") and fn not in bound
        if unbound and _C_MUTATOR.search(fn) and not fn.endswith("_set_state"):
            found.append(Mutator(cls, comp, fn, "c"))
    return found


def discover() -> list[Mutator]:
    found: list[Mutator] = []
    for cls, comp, cfg, is_view in _serializable():
        for p in cfg.get("properties", []):
            if p.get("writable"):
                t = str(p.get("type", "double"))
                found.append(
                    Mutator(
                        cls,
                        comp,
                        p["name"],
                        "property",
                        ptype=t,
                        choices=_choices(t),
                    )
                )
        for m in cfg.get("methods", []):
            if _is_mutator_method(m):
                ps = tuple(m.get("params") or m.get("args") or ())
                found.append(Mutator(cls, comp, m["name"], "method", ps))
        if not is_view:
            found += _c_only(cls, comp, cfg)
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


# ── probe values: from the type, the current value and the manifest ─────────

_UNPROBEABLE = ("capsule", "path", "object", "str", "char")


def _scalar_candidates(t: str, current: Any = None) -> list[Any]:
    t = t.strip()
    if t.startswith(("string_enum:", "enum:")):
        return list(_choices(t))
    if t == "bool":
        return (
            [not current, bool(current)]
            if current is not None
            else [
                True,
                False,
            ]
        )
    if "_Complex" in t:
        return [0.5 + 0.25j, -1.0 + 0.5j, 0.1 - 0.9j, 2.0 + 0.0j, -0.3 - 0.3j]
    if t in ("float", "double"):
        x = float(current) if current is not None else 0.0
        base = [x * 1.5 if x else 0.5, x * 0.5, x + 0.25, 0.1, 0.9, 2.0, 0.01]
        base += [4.0, 8.0, 16.0]  # a period or a count in samples
        return [v for v in base if v != x] or base
    if any(k in t for k in ("int", "size_t", "long")):
        x = int(current) if current is not None else 0
        base = [x + 1, 2 * x if x else 2, x + 3, 1, 2, 3, 4, 8]
        return [v for v in dict.fromkeys(base) if v != x and v >= 0]
    return []


def _array(t: str, n: int, seed: int) -> np.ndarray:
    elem = t[:-2].strip()
    rng = np.random.default_rng(seed)
    if "_Complex" in elem:
        dt = np.complex128 if "double" in elem else np.complex64
        return (rng.standard_normal(n) + 1j * rng.standard_normal(n)).astype(
            dt
        )
    if elem in ("float", "double"):
        return rng.standard_normal(n).astype(
            np.float64 if elem == "double" else np.float32
        )
    dt = np.dtype(elem.replace("_t", "")) if elem.endswith("_t") else np.int32
    return rng.integers(0, 2, n).astype(dt)


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


def _method_args(mut: Mutator, obj: Any) -> Iterator[tuple[Any, ...]] | None:
    """Candidate argument tuples, or None when a param cannot be probed."""
    per: list[list[Any]] = []
    lengths = list(dict.fromkeys([*_int_props(obj), 4, 8, 16, 31, 64, 127]))
    for i, p in enumerate(mut.params):
        t = str(p.get("type", "double"))
        if any(k in t for k in _UNPROBEABLE) or p.get("capsule"):
            return None
        if t.endswith("[]"):
            per.append([_array(t, n, seed=i + n) for n in lengths])
        else:
            enum = p.get("enum")
            cands = _scalar_candidates(t)
            if enum:
                cands = list(range(4))
            if not cands:
                return None
            per.append(cands)
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


# ── the probe ────────────────────────────────────────────────────────────────


@dataclass
class Verdict:
    name: str
    detail: str = ""


def _apply(obj: Any, mut: Mutator, value: Any) -> None:
    if mut.kind == "property":
        setattr(obj, mut.name, value)
    else:
        getattr(obj, mut.name)(*value)


def _readback(obj: Any, mut: Mutator) -> bytes:
    if mut.kind != "property":
        return b""
    return np.asarray(getattr(obj, mut.name)).tobytes()


def _observe(obj: Any, mut: Mutator, feed: Callable[..., Any]) -> tuple:
    """Everything a resumed object must reproduce: readback, the blob, the
    continuation's output, and the blob after it. Bytes, so -0.0 and NaN
    compare as what they are."""
    rb = _readback(obj, mut)
    before = obj.get_state()
    out = np.asarray(feed(obj, CONT)).tobytes()
    return rb, before, out, obj.get_state()


def _built(
    make: Callable[[], Any], feed: Callable[..., Any], mut: Mutator, value: Any
) -> Any:
    obj = make()
    feed(obj, WARM)
    _apply(obj, mut, value)
    return obj


def _accepted_values(mut: Mutator, make, feed) -> Iterator[Any]:
    """Values M takes without raising, readback distinct for a property."""
    probe = make()
    feed(probe, WARM)
    if mut.kind == "property":
        if mut.choices:
            cands: Any = list(mut.choices)
        else:
            cands = _scalar_candidates(mut.ptype, getattr(probe, mut.name))
    else:
        cands = _method_args(mut, probe)
        if cands is None:
            return
    seen: set[bytes] = set()
    for v in itertools.islice(cands, 40):
        try:
            o = _built(make, feed, mut, v)
        except Exception:  # the mutator refused this value: try the next
            continue
        rb = _readback(o, mut)
        if mut.kind == "property" and rb in seen:
            continue
        seen.add(rb)
        yield v


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


def _near(v: Any) -> list[Any]:
    """Values beside v: the likeliest to share v's key (a blob size, a
    plan) while differing from it, which is the target a restore that
    keeps the target's own value fails on."""
    if isinstance(v, bool) or not isinstance(v, (int, float, complex)):
        return []
    if isinstance(v, int):
        return [v + 1, v - 1] if v > 1 else [v + 1]
    return [v * 1.01, v * 0.99, v + 1e-3]


def _restore_check(
    mut: Mutator, make, feed, v: Any, others: list[Any]
) -> Verdict:
    """Restore the v twin's blob into every target and judge the result."""
    ref_v = _observe(_built(make, feed, mut, v), mut, feed)
    blob = _built(make, feed, mut, v).get_state()
    # Every target that ACCEPTS the blob must then be the v twin: a default
    # one, one already at v (the same key), and one at each other value M
    # takes -- including values beside v, which share its key and differ.
    targets: list[tuple[str, Any]] = [("default", make())]
    # Ten other values are plenty to find one that shares v's key; past that
    # an expensive object (an acquirer) only pays for repetition.
    others = [c for c in others if c is not v][:10]
    candidates = [("same", v)] + [(f"M{_show(c)}", c) for c in others]
    for c in _near(v) if mut.kind == "property" else []:
        candidates.append((f"M({_show(c)})", c))
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
    if accepted["default"]:
        return Verdict("TRAVELS")
    if accepted.get("same"):
        return Verdict("KEYED")
    return Verdict(
        "LOST",
        f"blob at M{_show(v)}: no target accepts it, not even the same key",
    )


def probe(mut: Mutator, make, feed) -> Verdict:
    vals = list(_accepted_values(mut, make, feed))
    if not vals:
        return Verdict("UNPROBED", "no value the mutator accepts")
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
    if len(set(refs)) < 2:
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
    if any(verdict.name == "KEYED" for verdict in seen):
        return Verdict("KEYED")
    return Verdict("TRAVELS")


# ── the list ────────────────────────────────────────────────────────────────


def _listed() -> dict[str, tuple[str, str]]:
    out: dict[str, tuple[str, str]] = {}
    for line in LIST.read_text("utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, verdict, *reason = line.split(None, 2)
        out[key] = (verdict, reason[0].strip() if reason else "")
    return out


_MUTATORS = discover()
_RECIPES = _recipes()


def _verdict(mut: Mutator) -> Verdict:
    if mut.kind == "c":
        return Verdict("C_ONLY", "no Python face to probe")
    recipes = _RECIPES.get(mut.cls)
    if not recipes:
        return Verdict("NO_RECIPE", "no row in test_state_serialization.CASES")
    if mut.kind == "method" and _method_args(mut, recipes[0][0]()) is None:
        return Verdict(
            "EXEMPT", "a param no probe can supply (a handle, path or string)"
        )
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        seen = [probe(mut, make, feed) for make, feed in recipes]
    # Any row's failure is the verdict; a pass needs a row that probed.
    for name in ("NONDETERMINISTIC", "LOST"):
        for verdict in seen:
            if verdict.name == name:
                return verdict
    for name in ("KEYED", "TRAVELS"):
        if any(v.name == name for v in seen):
            return Verdict(name)
    return seen[0]


@pytest.mark.parametrize("mut", _MUTATORS, ids=lambda m: m.key)
def test_a_mutator_value_is_state_or_listed_exactly(mut: Mutator) -> None:
    got = _verdict(mut)
    listed = _listed().get(mut.key)
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


def test_every_entry_names_a_mutator() -> None:
    known = {m.key for m in _MUTATORS}
    stale = sorted(k for k in _listed() if k not in known)
    assert not stale, f"{LIST.name} names no such mutator: {stale}"


def test_the_list_only_holds_listable_verdicts() -> None:
    bad = {k: v for k, (v, _) in _listed().items() if v not in LISTABLE}
    assert not bad, f"verdicts a list may not hold: {bad}"
