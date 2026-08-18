- **The general assembler, and a CCSDS CADU built from a description.**
    `wfm_frame_assemble()` materialises a `wfm_frame_desc_t`: it writes every
    field in wire order, then runs each stage over the span the layout gave it
    — over that span and no other, which is the whole content of the coverage
    table a standard's framing turns out to be. `wfm_frame_bits()` is now that
    function over the four-field configuration.

    **The kernels arrive as a table, and that is a layering requirement rather
    than a taste.** `ccsds_tm` depends on `wfm/wfm_frame.h` to describe a
    CADU, so `wfm_frame.c` must not call `ccsds_tm`'s kernels — the two would
    form a cycle. A `wfm_frame_ops_t` carries them instead, looked up by stage
    kind and extending the built-in CRC-16 rather than replacing it. A stage
    whose kind is in neither table is a **refusal**, never a silent skip: a
    stage that quietly did not run produces a frame that still assembles,
    still decodes against itself, and syncs to nothing.

    `ccsds_tm_frame_describe()` expresses 131.0-B-3 section 9 as data — three
    fields and three stages — and `ccsds_tm_frame_ops()` supplies the outer
    code, the randomiser and the inner code. They are the *same functions*
    `ccsds_tm_frame_encode()` calls, so the two paths cannot come to disagree
    about what a stage does, only about which bits it is handed, and that is
    what the description states.

    **The falsification is complete.** Across five configurations, the
    described CADU equals `ccsds_tm_frame_encode()`'s output **byte for
    byte** — including a carried `conv_enc_t` across two frames, where 3.3.2's
    continuous symbol sequence would expose an assembler that quietly owned
    its own register. That check inherits everything the shipped encoder is
    already falsified against: figure 9-1's marker, the randomiser's published
    prefix, Annex G's generator, the inner code's impulse response. A
    generalization that agreed only with itself would prove nothing.

    One rule makes a single in-place kernel signature serve a CRC, an outer
    code and a randomiser alike: **a derived field is the last field of its
    stage's cover**, so the kernel reads information at the head of its span
    and writes check symbols into the tail. Descriptions that break it are
    refused. Tracking: gh-853.
