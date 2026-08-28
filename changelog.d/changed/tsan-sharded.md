- **TSan runs as 3 parallel shards.** It was the whole sanitizer job's long
    pole at 15m03s, of which 13m54s is ctest — so the lever is the test run,
    not the build. `SHARDS` comes from `strategy.job-total`, so the leg count
    is declared once; `make test-tsan` refuses a `SHARD` outside `1..SHARDS`,
    because `ctest -I` does not know the shard count and a mismatch is a
    partial run every leg reports green.
