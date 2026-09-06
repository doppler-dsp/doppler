- **The CCSDS sync marker moved to its own module: `doppler.wfm.ccsds_asm_bits()`
    is now `doppler.ccsds.asm_bits()`.** It was a CCSDS translation unit compiled
    into the *general* `wfm_core`, declared in `wfm/wfm_core.h` and exported from
    `doppler.wfm.__all__` beside `PN` and `Gold` — and from there into
    `doppler.detection`'s doctests, tests and benchmark. Every other module states
    in its own docstring that it is general (`coding`'s opening line is *"the
    general channel codes … rather than any standard's picks"*), so there was no
    honest home for one standard's literal until this one. The new module carries
    the standard's published **data** and is not a binding of `ccsds_tm`'s
    transforms: the outer code, the randomiser and the inner code are still
    reached only by describing a CADU through `doppler.wfm.FrameDesc`, which is
    what keeps `wfm/wfm_frame.h` free of CCSDS. Closes
    [#1220](https://github.com/doppler-dsp/doppler/issues/1220), the last of the
    five extraction sites in
    [the frame design page](https://github.com/doppler-dsp/doppler/blob/main/docs/design/frame-description.md).
