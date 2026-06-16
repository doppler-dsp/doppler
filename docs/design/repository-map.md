# Repository Map

A whole-repository view of doppler and how its pieces relate. The one idea to
hold onto: **an algorithm is implemented once, in C** — everything else
radiates from a single declaration and is generated, thin, or documentary.

## The tree

```text
doppler/                          C-first DSP library — algorithms live in C once;
│                                 Python/Rust are thin wrappers over the C ABI.
│
├── native/                       ── THE C CORE (hand-written + jm-generated glue) ──
│   ├── inc/<obj>/<obj>_core.h       public C API + opaque state struct (hand-written)
│   ├── src/<obj>/<obj>_core.c       algorithm implementation (hand-written; source of truth)
│   ├── src/<module>/<mod>_ext.c     CPython aggregator (jm-generated — do not edit)
│   ├── src/<module>/<mod>_ext_<obj>.c   per-object binding fragment (jm-generated — free to edit)
│   ├── src/fft/                     vendored PFFFT + pocketfft backends (cf32 / integer FFT)
│   ├── tests/test_<obj>_core.c      C-level unit tests (CTest)
│   └── benchmarks/bench_<obj>_core.c   C microbenchmarks
│       └── ~60 component dirs: fir, cic, fft, ddc/ddcr, resamp, lo/nco/awgn/pn,
│          psd, tonemeas/imdmeas/nprmeas, cvt converters, acc_*, corr, wfm_synth …
│
├── objects/<obj>.toml            42 jm interface fragments — the DECLARATIVE SOURCE.
│                                 `jm apply` materialises _core stubs, _ext glue,
│                                 CMakeLists, __init__.py, .pyi, tests, benches.
│
├── just-makeit.toml              jm manifest: 21 modules → their object lists; pin 0.19.15.
│
├── src/doppler/                  ── THE PYTHON PACKAGE (thin: re-export + glue only) ──
│   ├── <module>/__init__.py         re-export of the C extension type — NO logic
│   ├── <module>/<module>.pyi        type stubs + numpy docstrings (jm-synthesized)
│   ├── <module>/tests/              Python integration tests (round-trip through C)
│   └── <module>/benchmarks/         Python-level benches
│       └── modules: spectral, filter, resample, source, ddc, cvt, measure,
│          analyzer/specan, detection, agc, delay, arith, accumulator,
│          buffer, stream, wfm, cli, util
│
├── ffi/rust/                     thin Rust FFI over the C ABI (hand-maintained — jm is CPython-only)
│   ├── src/                         extern decls + safe wrappers
│   └── examples/                    Rust usage examples
│
├── examples/python/              gallery + streaming demos (matplotlib → docs/assets PNGs)
│                                 device-under-test models may use numpy; spectral math is doppler-native
│
├── docs/                         mkdocs-material site
│   ├── api/                         Python API pages (mkdocstrings)
│   ├── c-api/                       C API (mkdoxy-generated from Doxygen XML)
│   ├── design/                      architecture + per-subsystem design docs (this file)
│   ├── guide/                       user guides
│   ├── gallery/                     annotated example showcases
│   ├── dev/                         release.md checklist, contributor workflow
│   └── assets/                      committed gallery PNGs
│
├── benchmarks/                   published per-release perf snapshots (portable + native)
│   ├── published/v<ver>/            hand-measured representative numbers (not CI)
│   └── history/                     CI-committed bench history
│
├── tests/install/                install smoke tests (apt/dnf/pacman/brew/source, cmake-link, pip)
├── scripts/                      bench_interleaved.py, bench_report.py, check_api_docs.py,
│                                 capture_specan.py (standalone specan capture utility)
├── vendor/                       statically-compiled deps: cjson, libzmq (zmq is opt-in)
│
├── CMakeLists.txt / cmake/       C + Python extension build
├── Makefile                      test-all, docs-build, gallery, bump-version, tag-release …
├── pyproject.toml                Python package metadata (uv_build)
├── Dockerfile / docker-compose.yml   reproducible build env
├── Doxyfile / mkdocs.yml / mkdocs-capi.yml   docs toolchain (mkdocs-material + mkdoxy)
├── .pre-commit-config.yaml       lints/formats HAND-WRITTEN code only (jm glue excluded)
├── jb.toml                       just-buildit packaging config
├── CLAUDE.md                     codebase guide for the AI assistant + the object workflow
├── CONTRIBUTING.md / README.md / CHANGELOG.md / LICENSE
└── conftest.py                   pytest root config (doctest gate wiring)
```

## How a layer reaches the one below

Every public capability traces back to exactly one C core, with no algorithm
duplicated across language layers:

| Layer                                          | What it is                  | Who owns it                                                 |
| ---------------------------------------------- | --------------------------- | ----------------------------------------------------------- |
| `objects/<obj>.toml`                           | the interface declaration   | hand-written (the source)                                   |
| `native/inc` + `native/src/<obj>/<obj>_core.c` | the algorithm in C          | hand-written                                                |
| `native/src/<module>/<mod>_ext.c`              | CPython aggregator binding  | **jm-generated** (never edited)                             |
| `native/src/<module>/<mod>_ext_<obj>.c`        | per-object binding fragment | jm-generated, then hand-owned (regenerated only if deleted) |
| `src/doppler/<module>/__init__.py` + `.pyi`    | the Python surface          | re-export + jm-synthesized stubs                            |
| `ffi/rust/src`                                 | the Rust surface            | hand-maintained over the C ABI                              |

`jm apply` turns a changed `objects/<obj>.toml` into reconciled glue, build
files, stubs, and test/bench scaffolding; the `jm status --check` drift gate in
CI fails if any generated file diverges from what the manifest implies. The
hand-written `_core.c`, the `_ext_<obj>.c` fragments, and the Rust FFI are the
parts a human edits — the rest is materialised. See
[Adding a Module](../dev/adding-a-module.md) for the per-object workflow and
[Architecture](../architecture.md) for the C-first rationale.
