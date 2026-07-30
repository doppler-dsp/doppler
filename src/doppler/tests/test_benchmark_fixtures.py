"""Fail-closed gate: every benchmark fixture still constructs.

Why this exists
---------------
``pyproject.toml`` collects ``bench_*.py`` alongside ``test_*.py``, so a
benchmark whose fixture stopped working normally surfaces as a red test.
But **20 of the 75 bench files contain no test function at all** -- jm
scaffolds a fixture-only module for any object whose methods it cannot
shape into a block benchmark (a by-value ``analyze`` record, a push/frame
object). pytest collects those files, finds nothing that requests ``obj``,
and never calls it. The constructor in a fixture-only bench is therefore
the one call site in the repo that no gate reads.

It rotted exactly as you would expect. jm's bench scaffold constructs
**positionally**, and the generated ``kwlist`` order is not stable: adding
an init_param, or changing one's kind, reorders it. Three objects had
their order change under a regeneration (``PSD``, ``Specan``,
``AccTrace`` -- string-enum params stopped sorting to the front) and five
more drifted in arity. All eight lived in fixture-only files, so the suite
stayed green while ``make bench`` could not have run; the perf-regression
workflow (since removed) was ``continue-on-error``, so it never reported
the breakage either.

The fix in the fixtures themselves is to construct with **keywords**,
which no reorder can touch. This gate is the backstop that keeps the
whole set honest -- including the files pytest would otherwise skip past.

It is deliberately cheap: it constructs each fixture and throws it away.
It does not benchmark anything, so it costs milliseconds and can live in
the ordinary suite.

Run locally
-----------
    uv run pytest src/doppler/tests/test_benchmark_fixtures.py
"""

from __future__ import annotations

import importlib.util
import inspect
import pathlib

import pytest

_ROOT = pathlib.Path(__file__).resolve().parents[1]
_BENCHES = sorted(_ROOT.rglob("benchmarks/bench_*.py"))


def _load(path: pathlib.Path):
    """Import a bench module under a unique name (basenames repeat)."""
    name = f"_bench_{path.parent.parent.name}_{path.stem}"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _fixture_names(module) -> list[str]:
    """Every pytest fixture defined *in* this module, in definition order.

    ``@pytest.fixture`` returns a ``FixtureFunctionDefinition`` wrapping the
    original function, so ``__wrapped__`` is the reliable tell across the
    pytest versions this repo supports. Fixtures imported from elsewhere are
    skipped -- they are not this benchmark's to validate.
    """
    out = []
    for name, value in vars(module).items():
        fn = getattr(value, "__wrapped__", None)
        if (
            fn is not None
            and getattr(fn, "__module__", None) == module.__name__
        ):
            out.append(name)
    return out


def _call_fixture(module, name: str, tmp_path: pathlib.Path, depth: int = 0):
    """Call a pytest fixture outside pytest, resolving its dependencies.

    A bench fixture may depend on sibling fixtures in the same module
    (``code``, ``hb_fir``) or on ``tmp_path``. pytest's own injection is
    not available here, so walk the signature and satisfy each parameter
    from the module, recursively. Anything unresolvable raises, which is
    the correct outcome: a fixture pytest could not build is a benchmark
    that cannot run.
    """
    if depth > 5:  # a fixture cycle; without this it would recurse forever
        raise RuntimeError(f"fixture dependency cycle at {name!r}")
    fixture = getattr(module, name)
    fn = getattr(fixture, "__wrapped__", fixture)
    kwargs = {}
    for param in inspect.signature(fn).parameters:
        if param == "tmp_path":
            kwargs[param] = tmp_path
        else:
            kwargs[param] = _call_fixture(module, param, tmp_path, depth + 1)
    result = fn(**kwargs)
    # A generator fixture yields its value; take the first item and let the
    # generator be collected (bench fixtures have no meaningful teardown).
    if inspect.isgenerator(result):
        result = next(result)
    return result


@pytest.mark.parametrize(
    "path",
    _BENCHES,
    ids=[f"{p.parent.parent.name}/{p.stem}" for p in _BENCHES],
)
def test_benchmark_fixture_constructs(path: pathlib.Path, tmp_path) -> None:
    """The bench module imports and every fixture it defines builds."""
    module = _load(path)
    names = _fixture_names(module)
    if not names:
        pytest.skip(f"{path.name} defines no fixtures")
    for name in names:
        assert _call_fixture(module, name, tmp_path) is not None, name


def test_the_gate_covers_every_bench_file() -> None:
    """Discovered, not registered -- a new bench file is gated on arrival."""
    assert _BENCHES, "no bench_*.py found; the glob or the layout moved"
