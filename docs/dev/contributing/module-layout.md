# Python Extension Module Layout

Every Python C extension subpackage in doppler follows an exact layout.
This page is the authoritative reference — it supersedes
[`docs/design/archive/PYTHON_EXTENSION_DESIGN.md`](../../design/archive/PYTHON_EXTENSION_DESIGN.md).

The [just-makeit](https://just-buildit.github.io/just-makeit/) scaffold
generates this layout automatically. Reading this page tells you *why* each
rule exists, so you can make correct decisions when you write or review code
by hand.

______________________________________________________________________

## Canonical file tree

`just-makeit module` + `just-makeit object --module` scaffold the native/ side
and the Python package, including a test and a benchmark skeleton per object
under `tests/` and `benchmarks/` — you fill them in (Steps 9 and 10 of
[Adding a Module](adding-a-module.md)).

```text
native/inc/doppler/<module>/
└── <module>_core.h        # module-level function declarations (scaffold)

native/inc/doppler/<component>/
└── <component>_core.h     # per-object header (scaffold, one per object)

native/src/<module>/
├── <module>_core.c            # module-level function implementations (scaffold)
├── <module>_ext.c             # aggregator — jm-regenerated, do not edit
├── <module>_ext_<component>.c # per-object binding fragment — jm-generated if it has the banner
└── CMakeLists.txt             # auto-managed by just-makeit (the .so)

native/src/<component>/
├── <component>_core.c         # per-object algorithm (scaffold, fill in)
└── CMakeLists.txt             # auto-managed by just-makeit (<component>_core)

native/tests/
├── dp_*_test.h              # the shared harness family (hand-owned)
└── <component>/
    └── test_<component>_core.c  # C unit tests (scaffold, one per object)

native/benchmarks/
├── jm_bench.h, dp_bench.h   # the bench scaffold (vendored / hand-owned)
└── <component>/
    └── bench_<component>_core.c # C benchmarks (scaffold, one per object)

src/doppler/<module>/
├── __init__.py              # re-export only — see rules below (scaffold)
├── <module>.pyi             # type stubs for the C extension (scaffold)
├── <module>.*.so            # compiled C extension (build artifact, not in VCS)
├── tests/
│   ├── __init__.py
│   └── test_<component>.py  # scaffold, one per object (fill in)
└── benchmarks/
    ├── __init__.py
    └── bench_<component>.py # scaffold, one per object (fill in)
```

There is **no `__init__.pyi`**. Stubs live in `<module>.pyi`.

______________________________________________________________________

## `<module>_ext_<component>.c` fragments: check the banner

The aggregator `<module>_ext.c` `#include`s each object's own
`<module>_ext_<component>.c` fragment. Who owns a fragment is set by its
manifest, and the file says which on its first line:

- **Generated** — `fragment = "generated"` in `objects/<component>.toml`
    stamps the fragment `/* jm:generated */`. Most fragments are this kind.
    `jm apply` re-renders it from the manifest like a `.pyi`, and a hand edit
    turns `make drift-check` red. A behaviour the binding needs is declared
    in the manifest, not patched into the file.
- **Hand-owned** — no banner. The fragment is sacred, like
    `<component>_core.c`: `jm apply` regenerates only the aggregator, and
    bespoke binding logic there is not drift. To pick up a jm codegen
    improvement in one, delete it and re-run `jm apply`; it gets recreated
    from the manifest, `_core.c`/tests/benches untouched.

`head -1` on the fragment tells you which you have before you edit it.

## Serialization is required for stateful objects

If an object carries running state that survives between calls (a phase, a
delay line, an accumulator), its header **must** declare the ABI triplet —
`<component>_state_bytes`/`_get_state`/`_set_state` — and its
`objects/<component>.toml` **must** set `serializable = "true"`. This is
not optional: elastic resume (checkpoint / migrate / scale across
processes, pods) depends on every stateful object speaking the same bytes
interface. See [State Serialization](../../design/state-serialization.md) for
the envelope format and [Adding a Module](adding-a-module.md) for the
step-by-step.

______________________________________________________________________

## Rule: no `__init__.pyi`

Python's import system resolves `doppler.filter.FIR` through
`doppler/filter/__init__.py`, which re-exports `FIR` from
`doppler/filter/filter`. Type checkers follow the same chain: they find
`FIR`'s stub in `filter.pyi`. An `__init__.pyi` would shadow that chain
and force you to duplicate every stub. One stub file, named to match the
`.so`, is the right model.

______________________________________________________________________

## `__init__.py` rules

Only two things are permitted:

```text
from .filter import FIR, FIRKind   # plain import, no `as` aliases

__all__ = ["FIR", "FIRKind"]
```

- **Plain imports only** — no `from .filter import FIR as Fir`.
- **No wrapper classes** — the C extension types *are* the public API.
- **No business logic** — no computations, no helper functions, no constants.

The one explicit exception: pure-Python utilities that are genuinely public
and have no C equivalent (e.g., `rate_convert` in `resample/`) may live in
`__init__.py`. They are not in the jm-generated `<module>.pyi` or in
`__all__`, so their docstring is their only documentation — keep it complete.

______________________________________________________________________

## `module.pyi` rules

### Header

The first line identifies the file:

```python
# filter/filter.pyi — type stubs for the filter C extension.
```

Then standard imports:

```python
import numpy as np
from numpy.typing import NDArray
```

Only import what the stubs actually reference.

### Every public symbol must have a stub

Classes and free functions exported from the `.so` need stubs. If it
appears in `__all__`, it needs a stub.

### Docstring format

All docstrings use [numpy-style format](https://numpydoc.readthedocs.io/en/latest/format.html#docstring-standard).
This is also what `mkdocstrings` is configured to render (see the `mkdocstrings` plugin in `mkdocs.yml`).

### Class stubs: docstring + Parameters + Examples

Each entry in the Examples section opens with a plain-text description,
followed by a blank line, then the `>>>` lines. Multiple examples are
separated by a blank line, a description of the next example, and another
blank line. The closing `"""` is always preceded by a blank line:

```python
class FIR:
    """Direct-form FIR filter.

    Parameters
    ----------
    taps:
        Filter coefficients as a 1-D numpy array.

        * ``float32`` — real-tap filter.
        * ``complex64`` — complex-tap filter.

        Other dtypes raise ``TypeError``.

    Examples
    --------
    Real float32 taps — low-pass filter:

    >>> import numpy as np
    >>> from doppler.filter import FIR
    >>> taps = np.array([0.25, 0.25, 0.25, 0.25], dtype=np.float32)
    >>> fir = FIR(taps)
    >>> fir.num_taps
    4
    >>> fir.is_real
    True

    Complex float32 taps:

    >>> ctaps = taps.astype(np.complex64)
    >>> fir2 = FIR(ctaps)
    >>> fir2.is_real
    False

    """

    num_taps: int
    """Number of filter taps."""

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Filter a block of CF32 samples."""
        ...
```

- Class docstrings: full Parameters + Examples sections.
- Method stubs: one-line docstring is enough.

### Doctests must pass

Every example in a `.pyi` stub is a runnable doctest:

```sh
make test-stubs
```

All examples must pass before a module can be merged. The CI suite runs
this check. Write examples that copy-paste correctly — use realistic types,
real output values, no ellipsis (`...`) to skip important output.

______________________________________________________________________

## `benchmarks/bench_<component>.py` rules

Every object ships a Python benchmark alongside its C benchmark
(`native/benchmarks/<component>/bench_<component>_core.c`). Python benchmarks use
[pytest-benchmark](https://pytest-benchmark.readthedocs.io/) so results are
saved as JSON snapshots in `benchmarks/history/` (local scratch,
git-ignored); each release's committed numbers are measured into
`benchmarks/published/v<ver>/`.

```python
"""Benchmark for FIR."""
import numpy as np
import pytest
from doppler.filter import FIR

BLOCK = 1_048_576  # samples per benchmark round


@pytest.fixture(scope="module")
def fir():
    taps = np.zeros(64, dtype=np.float32)
    taps[32] = 1.0      # all-pass
    return FIR(taps)


@pytest.fixture(scope="module")
def x_cf32():
    return np.ones(BLOCK, dtype=np.complex64)


def test_bench_execute(benchmark, fir, x_cf32):
    benchmark(fir.execute, x_cf32)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = BLOCK / benchmark.stats["mean"] / 1e6
```

No CI job runs benchmarks; a release measures and commits its set by hand
([release step 2b](../release.md#2b-refresh-the-benchmarks-page)). Run
locally when you need an immediate result:

```sh
make bench          # C + Python (delegates to just-makeit bench)
make bench-python   # Python only (pytest-benchmark)
make bench-save     # save a baseline to compare against
```

Rules:

- One file per object: `benchmarks/bench_<component>.py`
- Name each test `test_bench_<case>` — **not** module-qualified (every
    `bench_*.py` draws from the same small vocabulary); identifiability
    across the full suite comes from the *filename*, via `conftest.py`'s
    `pytest_terminal_summary` hook and `scripts/bench_report.py`, both of
    which derive a `module::case` label from the pytest fullname. See
    [Benchmarking](benchmarking.md) for the full convention.
- Use `scope="module"` fixtures to avoid re-constructing objects per round
- Published snapshots are measured on one representative machine per
    release for comparability; local snapshots are for development only

______________________________________________________________________

## Why C extension types, not Python wrappers

A Python wrapper class duplicates the interface and diverges over time —
parameter names drift, docstrings contradict the C code, behaviour
subtly differs. The C extension type *is* the implementation; the Python
type object lives in the `.so`. The `.pyi` stub describes it to type
checkers. There is no middle layer.

This is the same model NumPy uses: `numpy.ndarray` is a C type, stubbed in
`numpy/__init__.pyi`.

______________________________________________________________________

## See also

- [Adding a module](adding-a-module.md) — step-by-step guide using
    just-makeit to scaffold a new extension module
- [just-makeit docs](https://just-buildit.github.io/just-makeit/) — scaffold
    tool that generates compliant layouts automatically
- [Build from Source](../../install/source.md) — how to compile extensions
