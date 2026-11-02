- **`dp_symsync_create` refuses an interpolator order outside 0..2.** It
    used to read any other order as cubic; it now returns NULL, as
    `dp_farrow_create` does. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
