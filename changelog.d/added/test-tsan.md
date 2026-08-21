- **`make test-tsan` — the threaded C tests under ThreadSanitizer, where a
    data race fails rather than prints.** The companion to `make test-ubsan`,
    and the gate that holds the CCSDS RS fix above.

    Scoped to the threaded tests by a **ctest `-R` pattern, not a hand list
    of binaries**, so a new threaded test named for what it is gets picked up
    with no edit here — the same reasoning `check_bench_coverage` applies to
    benchmarks, for the same reason: a list maintained by hand goes stale
    silently. It fails when the pattern matches nothing, because an empty
    result set is not a pass — the trap the glibc and tarball gates were both
    caught by. `TSAN_OPTIONS=halt_on_error=1` for the reason `UBSAN_OPTS`
    already gives: without it the sanitizer prints and the suite still
    passes.

    `native/tests/test_ccsds_tm_rs_race.c` is its first case, and is a
    separate binary on purpose — the race is on the FIRST call, so a process
    that has already encoded anything cannot reach it. Eight threads released
    from one barrier must all derive the same `g(x)` and the same parity.
    Reverting the fix makes TSan report `data race ... in rs_init` and the
    target go red.

    It earned its keep immediately: the first run caught `bench_buffer_core`
    linking no `libm`, which a Release build hides by folding the `sqrt` away
    and a Debug build does not.
