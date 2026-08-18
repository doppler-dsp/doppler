- **A frame is a field list and a stage list, not a pipeline.**
    `wfm_frame_desc_t` describes a frame as an ordered list of **fields** —
    what appears on the wire, in order — and an ordered list of **stages**,
    each carrying the **span it covers**. `wfm_frame_desc_layout()` derives
    every field offset, every stage span and both lengths from the two.

    `wfm_frame_t` is now a *configuration* of it rather than a rival:
    `wfm_frame_layout()` builds the description through
    `wfm_frame_describe()` and reads the general layout back, so there is one
    implementation of the arithmetic and the two cannot drift.

    **`cover` is the load-bearing part.** `ccsds_tm_frame.h` already predicts
    the failure of leaving it out — *"any chain of optional transforms applied
    to 'the frame' is right at three stage boundaries and wrong at the fourth,
    and wrong in the direction that still encodes, still decodes against
    itself, and syncs to nothing."* A stage that inherited "whatever ran
    before me" would be that chain. CCSDS is the case that proves it: the
    marker is covered by the inner code and by neither the outer code nor the
    randomiser.

    Two rules fell out of prototyping and both simplify the model. A derived
    field **is** a field, which removes any need for a stage to expand what it
    covers and makes R-S check symbols and a CRC trailer one concept rather
    than two. And "emits a new unit" is a distinct property — the inner code
    consumes the assembled CADU and emits channel symbols — which is exactly
    why `ccsds_tm_frame_layout_t` reports `cadu_bits` and `out_bits` as two
    numbers.

    Checked against both shipped framers: the existing frame's layout is
    unchanged, and a description configured as CCSDS reproduces
    `ccsds_tm_frame_layout()`'s four spans and both lengths exactly, across
    three configurations. That is the layout half of the falsification;
    byte-for-byte output needs a general assembler and is not claimed yet.
    Design: `docs/design/frame-description.md`. Tracking: gh-853.
