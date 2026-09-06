- **The open stage kind is demonstrated, not just asserted.** §6 of
    `wfmgen_frame_demo.c` allocates `WFM_STAGE_USER + 1`, supplies its kernel
    through a one-entry `wfm_frame_ops_t`, and shows the assembly refused
    without it and reversed with it. The gallery page's "a kind that is open"
    bullet was the design's central claim and the one nothing on that page ran.
- **And the Python face of it**: `frame_own_stage_demo.py` describes the same
    generic frame, runs its own kernel in Python and hands the wire the result
    — the way round the deliberate absence of a Python ops table
    ([#1125](https://github.com/doppler-dsp/doppler/issues/1125)). It is also
    the first worked frame example in Python that is not a CCSDS CADU.
