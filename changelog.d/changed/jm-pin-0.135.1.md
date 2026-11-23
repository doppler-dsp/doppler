- **just-makeit pin 0.135.0 → 0.135.1.** An inline-built `Segment` builds
    its source at the segment's `fs`: for `seg = Segment(type=..., fs=40e6)`,
    `seg.sources[0].fs` is `40e6` where it was the source type's default
    ([just-makeit#2725](https://github.com/just-buildit/just-makeit/issues/2725)).
    The re-render changes `Segment_init` in the `wfm_compose` extension; no
    core changes, and no doppler code read the old default.
