- **`doppler.wfm.ccsds_asm_bits()` is now `doppler.ccsds.asm_bits()`.** A
    standard's marker was a CCSDS translation unit compiled into the *general*
    `wfm_core` and a CCSDS name in `doppler.wfm.__all__`; it moved to a module
    of its own, beside the general layer rather than under it. `ccsds_tm`'s
    transforms still have no binding — describe a CADU with `FrameDesc`.
    [#1220](https://github.com/doppler-dsp/doppler/issues/1220),
    [the frame design page](https://github.com/doppler-dsp/doppler/blob/main/docs/design/frame-description.md).
