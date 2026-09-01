- **The frame a caller builds is documented and worked through.**
    `wfm_source_t.frame` and the scene JSON's `frame` key shipped with tests
    and a schema entry and nothing a reader could find. Now a C demo with 16
    self-validating checks, a [gallery page][carried-frame], and the scenes
    guide's *A frame the caller built* — the C, JSON and Python routes, and
    the flat framing flags proven byte-identical to the description they
    build.
- **A derived frame field that names no producing stage is accepted**, and
    generates a different waveform: 40 bits rather than 56, exit 0, nothing on
    stderr. Documented with the measurement; the C builder cannot reach that
    state ([#1155](https://github.com/doppler-dsp/doppler/issues/1155)).

[carried-frame]: https://github.com/doppler-dsp/doppler/blob/main/docs/gallery/wfmgen-carried-frame.md
