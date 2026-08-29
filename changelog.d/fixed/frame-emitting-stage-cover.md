- **An emitting stage must cover the whole frame, and a description may hold
    at most one.** `wfm_frame_desc_layout` sized `out_bits` from the stage's
    cover while `wfm_frame_assemble` hands the kernel the whole frame, so a
    partial cover laid out cleanly with every offset right and could then
    never assemble — reported as a bare 0, indistinguishable from bad data.
    Both are refused where the geometry is decided. Nothing in the tree
    changes shape; all three emitting stages already cover everything.
    See [frame-description.md](../../docs/design/frame-description.md).
