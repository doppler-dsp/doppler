- **A forged boxcar or delay index is refused, not written.** A boxcar
    `pos` past its ring was an out-of-bounds write, and a delay `head` past
    its capacity read outside the window; `set_state` now refuses both
    with the object unchanged. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
