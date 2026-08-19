- **`make coverage` stopped ignoring pytest's exit code**, and the four causes
    behind the failures it was hiding are fixed rather than tolerated. The
    recipe carried a leading `-`, so make discarded the result in the one job
    that produces the coverage number — 89 results (27 failed, 62 errors)
    against 2631 passed, invisible.

    | cause                                                      | count | fix                                                         |
    | ---------------------------------------------------------- | ----- | ----------------------------------------------------------- |
    | repo root resolved by counting directories                 | 81    | `repo_root()` walks up (separate change)                    |
    | the **normal** `wfmgen` shadowing the instrumented library | 7     | install the instrumented binary into the copied tree        |
    | a threading speedup measured under profiling               | 1     | withhold that one assertion when `LLVM_PROFILE_FILE` is set |
    | files deleted from `src/` surviving in the copied tree     | —     | clear the python half before the extract                    |

    The binary one is invisible without a byte comparison: the tar copy
    excludes `*.so` but **not executables**, so a 655 KB gcc/optimised
    `wfmgen` sat beside a 1.49 MB clang/Debug library, and every test
    asserting byte parity between the CLI and the library compared two
    different builds of the same source.

    The scaling one is the only case where the TEST is wrong under coverage
    rather than the environment. `-fprofile-instr-generate` makes every
    counter update an atomic on a page shared between threads, so two threads
    serialise on the profiling runtime instead of the GIL: 0.98x against
    ~1.9x uninstrumented. Asserting anyway would turn a threading claim into
    a measurement of llvm's counters. The assertion stays live when not
    instrumented — verified, because a skip that quietly disarms an example
    is worse than the failure it silences.

    The fourth had no failure count because nothing measured it. The extract
    is additive, so anything deleted from `src/` kept running from the copy —
    found when a temporary sabotage test, removed from `src/`, failed the
    next run anyway. **A deleted test that keeps passing is worse than one
    that keeps failing**: it reports coverage for source that no longer
    exists.

    Sabotage-proven both directions: a deliberate failing test now gives
    `COVERAGE_RC=2`, where before the same failure exited 0.
