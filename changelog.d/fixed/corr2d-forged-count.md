- **Corr2D refuses a forged count.** The plain restore took a `count` at
    or past the dwell, which never dumps again; it is refused now, as the
    delay-Doppler flavor already did. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
