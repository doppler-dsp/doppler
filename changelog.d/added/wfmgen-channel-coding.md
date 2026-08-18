- **wfmgen can generate a coded waveform, and a CCSDS CADU is a configuration
    of it.** Four new flags on `--type bits` — `--rs-depth I`, `--randomise`,
    `--asm` and `--conv` — apply channel coding as **stages over the frame's
    fields**, and they do not all cover the same bits.

    That asymmetry is the point rather than a detail. A marker, a preamble and
    a sync word are things a receiver *finds*, so all three must look the same
    in every frame: the outer code and the randomiser reach over the data
    group only, and the inner code reaches over everything. CCSDS states that
    rule for its own ASM (10.3.4: *"The ASM was not randomized"*), and the
    reason it gives is exactly as true of doppler's preamble and sync word —
    so it generalises rather than being special-cased.

    Set all four with a 223·I-octet payload and no preamble or sync word and
    the result is a **CCSDS CADU**. It is a configuration these flags reach,
    not a mode they switch into.

    Verified against the shipped encoder rather than against itself: the CLI's
    output is bit-identical to a description assembled through
    `wfm_frame_assemble`, which is byte-identical to `ccsds_tm_frame_encode`
    across five configurations — so it inherits everything that encoder is
    falsified against (figure 9-1's marker, the published randomiser prefix,
    Annex G's generator, the inner code's impulse response).

    **The record carries it**, and that was a defect worth catching before it
    shipped: a record is what makes a capture reproducible, so a stage the
    record dropped would be a capture nobody could rebuild — and the omission
    would read as a plain uncoded waveform rather than as missing
    information. `--record` now emits the four keys (only when set, so an
    uncoded record is unchanged), `--from-file` reads them back, the schema
    declares them, and a round trip is asserted end to end.

    Refusals rather than surprises: `--rs-depth` outside 4.3.5.1's
    `{1,2,3,4,5,8}` is rejected, and a payload off the `223·I` grid is refused
    rather than padded — virtual fill is not implemented
    ([gh-813](https://github.com/doppler-dsp/doppler/issues/813)), and a
    silently padded codeblock is the wrong length for the receiver it was
    aimed at. Any coding flag also *frames* the waveform, as `--sync` does: a
    CADU carries neither a preamble nor a sync word, so a source coded but
    unframed would have emitted its payload with no coding at all.
