- **`check_bench_coverage` now gates Python benchmarks too**
    ([#1010](https://github.com/doppler-dsp/doppler/issues/1010)). Phase 9 of
    the lifecycle owes a benchmark "C **and** Python"; the gate read
    `native/benchmarks/` and nothing else, and 24 of 86 `bench_*.py` took the
    `benchmark` fixture without ever calling it — collecting zero rounds
    while every other signal said covered. Five are filled in and the rest
    are ratcheted. The first version of the check was itself wrong and sabotage
    caught it: `^[^#]*\bbenchmark\s*\(` spans newlines, so it matched the
    prose "The C benchmark (" in a docstring and passed a deliberately
    hollowed file. It walks the AST now — a mention in prose is not a Call.
