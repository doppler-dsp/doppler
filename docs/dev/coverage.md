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

> **Status:** Phases 1–2 (C tests **∪ Python**) are wired. Phase 3 (fold in the
> Rust `.profraw`) and Phase 4 (a `diff-cover` patch-coverage gate on PRs) are
> planned — see the roadmap at the bottom.

## Run it locally

```sh
make coverage          # instrumented build → C tests + pytest → merged report
# open the HTML:
xdg-open build-cov/html/index.html        # or: python -m http.server -d build-cov/html
```

Requirements: **clang** + matching **`llvm-profdata`** / **`llvm-cov`** (same
LLVM version). Override version-suffixed tools if needed:

```sh
make coverage LLVM_PROFDATA=llvm-profdata-22 LLVM_COV=llvm-cov-22
```

The `coverage` target configures a dedicated `build-cov/` tree
(`-DDOPPLER_COVERAGE=ON -DCMAKE_C_COMPILER=clang -DBUILD_PYTHON=ON`,
`Debug`/`-O0` for accurate line mapping), then:

1. runs the **CTest** suite (`LLVM_PROFILE_FILE` → `build-cov/prof/c-*.profraw`);
1. **stages** the instrumented Python extension into a throwaway package
    (`build-cov/pkg/`) — `PYTHON_PACKAGE_DIR` is redirected there so the build
    never clobbers the dev's working `src/doppler/` `.so`; the Python source is
    layered over the staged `.so` with `tar`;
1. runs **pytest** against that staged package (`LLVM_PROFILE_FILE` →
    `py-*.profraw`), so the same cores get coverage from what Python exercises;
1. `llvm-profdata merge`s C ∪ Python and emits the HTML + `lcov`, reported
    against `libdoppler.so` **and** every module `.so` (a line only Python reaches
    still attributes correctly).

A live NATS broker on `127.0.0.1:4222` lets the `nats://` stream path count too
(the CI job starts one); without it `stream_nats.c` self-skips and shows 0%.

## What's measured

- **Included:** first-party `native/src/**/*_core.c` (and the wfm/timing/measure
    cores) — the hand-written algorithms.
- **Excluded** (`COV_IGNORE` in the `Makefile`): `vendor/**`, the jm-generated
    `*_ext.c` binding aggregators, and the `tests/`/`benchmarks/` harnesses.

The report is generated against `build-cov/libdoppler.so` plus every staged
module `.so`. Hand-owned binding **fragments** (`<mod>_ext_<obj>.c`) count;
jm-generated `<mod>_ext.c` **aggregators** are excluded.

Phase 1 (C tests) covered ~72% of the cores it reached; phase 2 adds the Python
harness, which lifts the cores the C tests can't reach (`magnitude_db_cf32.c`
0%→100%, `wfm_synth_bridge.c` 0%→96%) while widening the denominator with the
binding fragments and the stream layer. Remaining 0% spots are genuine gaps
worth a test — e.g. `obw_from_power.c` (no caller in any suite) — or are
environment-gated (`stream_nats.c` needs the broker).

## CI

A non-gating `coverage (C + Python)` job (`.github/workflows/ci.yml`) runs
`make coverage` on every PR (with a NATS broker so the `nats://` path counts) and
uploads the HTML + `lcov` as the `coverage-report` artifact, with a `TOTAL` line
in the job summary. It is **not** part of the `ci-passed` required set yet —
coverage is informational until the phase-4 patch gate lands.

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

1. **C tests** — done.
1. **+ Python** — done: instrumented `.so` staged into `build-cov/pkg/`, pytest
    run against it, its `.profraw` merged in.
1. **+ Rust** — `cargo test -Cinstrument-coverage`; merge the C `.profraw` from
    `libdoppler.a`.
1. **Patch gate** — `diff-cover` over the merged report at `--fail-under=N` on
    PRs; add `coverage` to `ci-passed`. Never a retroactive global threshold.
