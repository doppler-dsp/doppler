# Repository Map

A whole-repository view of doppler and how its pieces relate. The one idea to
hold onto: **an algorithm is implemented once, in C** — everything else
radiates from a single declaration and is generated, thin, or documentary.

## The tree

C-first DSP library — algorithms live in C once; Python/Rust are thin wrappers
over the C ABI. Paths use `<obj>` / `<module>` placeholders so the map never
enumerates (and never goes stale on) the actual file list.

```text
doppler/
│
├── just-makeit.toml                    jm manifest: modules and their object lists
├── objects/<obj>.toml                  jm interface fragments — the DECLARATIVE SOURCE
│
│   ── THE C CORE (hand-written + jm-generated glue) ──
│
├── native/
│   ├── inc/<obj>/<obj>_core.h           public C API + opaque state struct (hand-written)
│   ├── src/<obj>/<obj>_core.c           algorithm implementation (hand-written; source of truth)
│   ├── src/<module>/<mod>_ext.c         CPython aggregator (jm-generated — do not edit)
│   ├── src/<module>/<mod>_ext_<obj>.c   per-object binding fragment (jm-generated — free to edit)
│   ├── tests/test_<obj>_core.c          C-level unit tests (CTest)
│   └── benchmarks/bench_<obj>_core.c    C microbenchmarks
│
│   ── THE PYTHON PACKAGE (thin: re-export + jm-materialized stubs) ──
│
├── src/doppler/
│   ├── <module>/__init__.py             re-export of the C extension type — NO logic
│   ├── <module>/<module>.pyi            type stubs + numpy docstrings (jm-synthesized)
│   ├── <module>/tests/                  Python integration tests (round-trip through C)
│   └── <module>/benchmarks/             Python-level benches
│
├── src/doppler/examples/                     gallery + streaming demos (matplotlib → docs/assets PNGs)
│
├── ffi/rust/                            Rust FFI over the C ABI (hand-maintained)
│   ├── src/                             extern decls + safe wrappers
│   └── examples/                        Rust usage examples
│
├── docs/                                mkdocs-material site
│   ├── api/                             Python API pages (mkdocstrings)
│   ├── c-api/                           C API (mkdoxy-generated from Doxygen XML)
│   ├── design/                          architecture + per-subsystem design docs
│   ├── guide/                           user guides
│   ├── gallery/                         annotated example showcases
│   ├── dev/                             release checklist, contributor workflow (this file)
│   └── assets/                          committed gallery PNGs
│
├── benchmarks/                          published per-release perf snapshots (portable + native)
│   ├── published/v<ver>/                hand-measured representative numbers (not CI)
│   └── history/                         CI-committed bench scratch
│
├── tests/install/                       install smoke tests across package managers
├── scripts/                             repo tooling (benchmarking, API-doc coverage, capture)
├── vendor/                              third-party sources compiled straight in
│
├── CMakeLists.txt                       C + Python extension build
├── cmake/                               install exports: find_package + pkg-config templates
├── Makefile                             test-all, docs-build, gallery, bump-version, tag-release
├── pyproject.toml                       Python package metadata (uv_build)
├── Dockerfile / docker-compose.yml      multi-container streaming demo (zmq PUB/SUB tx → rx/spectrum)
├── Doxyfile / mkdocs.yml                docs toolchain (mkdocs-material + mkdoxy)
├── .pre-commit-config.yaml              lints/formats hand-written code only (jm glue excluded)
├── jb.toml                              just-buildit packaging config
├── CLAUDE.md / CONTRIBUTING.md          assistant + contributor guides
├── README.md / CHANGELOG.md / LICENSE   project docs
└── conftest.py                          pytest root config (doctest gate wiring)
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
[Adding a Module](adding-a-module.md) for the per-object workflow and
[Architecture](../architecture.md) for the C-first rationale.
