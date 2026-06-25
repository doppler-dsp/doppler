# Code coverage

doppler measures coverage with **clang source-based coverage**
(`-fprofile-instr-generate -fcoverage-mapping`). The design leverages doppler's
defining property — every algorithm is in C exactly once — so the same
`native/src/<obj>/<obj>_core.c` is exercised by three harnesses:

| Harness                                | reaches `_core.c` via               |
| -------------------------------------- | ----------------------------------- |
| C tests (`native/tests/test_*_core.c`) | link the `<obj>_core` OBJECT lib    |
| Python tests (`src/doppler/**/tests/`) | load the `.so` (ext binding → core) |
| Rust tests (`ffi/rust/`)               | link `libdoppler.a`                 |

Because one `DOPPLER_COVERAGE` build instruments those OBJECT files **once**,
every consumer carries instrumentation; each harness emits `.profraw`,
`llvm-profdata merge` unifies them, and `llvm-cov` attributes the merged result
back to `_core.c` — a single report showing what **C ∪ Python ∪ Rust** covers.

> **Status:** Phase 1 (C tests) is wired. Phases 2–3 (fold in the Python `.so`
> and Rust `.profraw`) and Phase 4 (a `diff-cover` patch-coverage gate on PRs)
> are planned — see the roadmap at the bottom.

## Run it locally

```sh
make coverage          # build-cov/ instrumented build → run C tests → report
# open the HTML:
xdg-open build-cov/html/index.html        # or: python -m http.server -d build-cov/html
```

Requirements: **clang** + matching **`llvm-profdata`** / **`llvm-cov`** (same
LLVM version). Override version-suffixed tools if needed:

```sh
make coverage LLVM_PROFDATA=llvm-profdata-22 LLVM_COV=llvm-cov-22
```

The `coverage` target configures a dedicated `build-cov/` tree
(`-DDOPPLER_COVERAGE=ON -DCMAKE_C_COMPILER=clang`, `Debug`/`-O0` for accurate
line mapping), runs the CTest suite with `LLVM_PROFILE_FILE` pointed at
`build-cov/prof/`, then merges and emits both an HTML report and an `lcov` file.

## What's measured

- **Included:** first-party `native/src/**/*_core.c` (and the wfm/timing/measure
    cores) — the hand-written algorithms.
- **Excluded** (`COV_IGNORE` in the `Makefile`): `vendor/**`, the jm-generated
    `*_ext.c` binding aggregators, and the `tests/`/`benchmarks/` harnesses.

The report is generated against `build-cov/libdoppler.so` (it contains every
core's coverage mapping). Phase 1 (C tests only) lands around ~72% line
coverage; files showing 0% (e.g. `magnitude_db_*`, `obw_from_power`,
`wfm_synth_bridge`) are reached only from the Python/Rust harnesses and fill in
at phases 2–3.

## CI

A non-gating `coverage` job (`.github/workflows/ci.yml`) runs `make coverage` on
every PR and uploads the HTML + `lcov` as the `coverage-report` artifact, with a
`TOTAL` line in the job summary. It is **not** part of the `ci-passed` required
set yet — coverage is informational until the phase-4 patch gate lands.

## Notes / caveats

- **clang only.** The gcc OS matrix is untouched; coverage is a separate
    clang+Linux build. (`make coverage` sets `clang`/`clang++` — the latter for the
    vendored C++ in the optional stream component.)
- **x86-64-v2 baseline, not `-march=native`.** Native would enable FMA, which
    breaks `wfm_synth`'s bit-exact `step()==steps()` parity test (FMA contraction).
    The coverage build stays at the shipped portable baseline. `awgn_core.c`'s weak
    libmvec `_ZGVdN8v_logf` declaration carries `__attribute__((target("avx2")))`
    so its AVX dispatcher still compiles under clang at that baseline (ABI-only;
    gcc codegen unchanged).
- **"N functions have mismatched data"** from `llvm-cov` is benign: a few
    functions' mappings differ between the `.so` and the C-test exes (inlining);
    those drop from the report. The Python phase reads the same `.so` it loaded, so
    its attribution is exact.

## Roadmap

1. **C tests** — done (this page).
1. **+ Python** — build the extension under `DOPPLER_COVERAGE`, run `pytest` with
    `LLVM_PROFILE_FILE`, merge its `.profraw`; keep `pytest-cov` for the hand-`.py`.
1. **+ Rust** — `cargo test -Cinstrument-coverage`; merge.
1. **Patch gate** — `diff-cover` over the merged report at `--fail-under=N` on
    PRs; add `coverage` to `ci-passed`. Never a retroactive global threshold.
