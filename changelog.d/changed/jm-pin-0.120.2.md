- **just-makeit pin 0.119.0 → 0.120.2** (#2890). `jm status --check`, which
    `make drift-check` runs, now fails a bench target `jm bench` never runs
    (jm#2495), so a benchmark's rows can no longer go silently absent from
    `make bench` and `make bench-ab`. `bench_stream` is allowed by name: it
    has its own runner, `make bench-stream`. The pin also brings 0.120.1's
    fix for `jm status` reporting files written mid-run as stale (jm#2488).
