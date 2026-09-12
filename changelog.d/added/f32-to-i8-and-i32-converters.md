- **`cvt.F32ToI8` and `cvt.F32ToI32`** complete the converter family — the
    write-direction duals of the existing `I8ToF32`/`I32ToF32`, so every
    integer wire width has a canonical converter to call rather than a private
    quantiser ([#1117](https://github.com/doppler-dsp/doppler/issues/1117)).
    Both saturate, round to nearest and carry the sticky `clipped` flag and the
    serializable-state triplet their siblings have.
