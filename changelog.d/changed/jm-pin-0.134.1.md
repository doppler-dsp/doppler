- **just-makeit pin 0.134.0 → 0.134.1.** A `Segment`'s flat `window` now
    reads the source's `get_fn` at the segment's own `fs`, the same count a
    `Synth` reads, closing the Segment half of #3211
    ([just-makeit#2717](https://github.com/just-buildit/just-makeit/issues/2717)).
    The re-render changes `wfm_compose_ext.c` only.
