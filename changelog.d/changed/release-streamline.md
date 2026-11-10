- **A release no longer serializes its gallery render behind the bench
    run, and a perf PR's verdict is one `bench-ab` run.** The gallery
    renders on the second x86 box while `bench-interleaved` measures, and
    both land as one train
    ([release checklist](docs/dev/release.md#2-check-examples)). A SLOWER
    row is rechecked by rerunning its component alone, not the whole suite
    ([benchmarking](docs/dev/contributing/benchmarking.md#comparing-two-commits-make-bench-ab)).
    `make bench-ab BENCH_ARGS=--python-only` now runs, so the two sides
    of a verdict can run on two boxes at once.
