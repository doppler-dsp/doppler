- **Nine memory defects the C suite had been carrying**, found by the new
    `make test-asan` on its first run: two out-of-bounds reads (a 64-byte copy
    out of an 8-byte global; an I/Q buffer sized in complex samples instead of
    `int16_t`) and seven leaks, every one a `*_create()` in a test or
    validation `main()` with no matching destroy.
    [#1024](https://github.com/doppler-dsp/doppler/issues/1024)
