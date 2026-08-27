- **`make test-asan` — the C suite under AddressSanitizer and LeakSanitizer,
    gated in CI.** Covers what `make test` and UBSan structurally cannot: an
    access outside an object lands on whatever the compiler put next, so the
    program is wrong while the assertions stay green. Ships with no
    suppression file and no ratchet.
    [#1024](https://github.com/doppler-dsp/doppler/issues/1024)
