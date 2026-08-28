- **`make test-ubsan` and `make test-tsan` now run in CI**, joining `test-asan`
    in the `sanitizers` job. Both existed and had never run on a pull request.
    TSan is whole-suite now: its old `-R 'race|parallel|thread'` pattern
    selected 2 tests, one by accident, and missed three genuinely threaded ones.
    [#1026](https://github.com/doppler-dsp/doppler/issues/1026)
