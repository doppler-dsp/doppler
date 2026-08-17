- **The frame assembler carries the inner encoder across frames, and takes a
    capacity.** `fec_frame_encode` now reads
    `(cfg, conv, frame, frame_len, out, max_out)`. Both parameters close a hole
    that a single-frame test cannot see, and the prototype's actual use case is
    a *stream* of frames.

    **`conv`** is the caller's `fec_conv_t`, carried from one frame to the
    next. 3.3.2 fixes the inner code's output as one uninterrupted symbol
    sequence with no per-frame flush, and `fec_ccsds.h` says so outright — the
    state is a struct precisely so a chunked caller can carry it, "or introduce
    a discontinuity every chunk boundary that no decoder expects." The
    assembler was that caller, and it called `fec_conv_init` per frame.

    Measured at depth 1, two frames: restarting differs from the continuous
    encoding in **6 of 8288 symbols**, all within the first 7 symbols of frame
    2 — the `K - 1 = 6` bits of register memory, landing on the ASM a receiver
    correlates against. A matched Viterbi absorbs it, which is what makes this
    the same class as the inversion on G2 and the dual basis: self-consistent,
    decodes against a receiver of one's own construction, and not what the
    standard says. `NULL` is still the single-frame form, and now means "this
    frame stands alone" rather than "I forgot".

    **`max_out`** is a capacity rather than a comment, because the CADU is
    assembled in the TAIL of the output buffer: a short buffer was a write past
    the end, not a truncated answer. Its sibling `wfm_frame_bits` already took one.

    Both are proven by sabotage. Restoring the per-frame `fec_conv_init`
    reddens *"a stream of frames must equal one continuous encode of the same
    CADUs"* — asserted against the two CADUs run through one `fec_conv_encode`,
    which is external truth built from the uncoded assembler and the kernel,
    neither of which knows what a frame is. Dropping the capacity check reddens
    both the refusal and *"it must not have written anything"* — the guard is
    preventing a real out-of-bounds write, not a theoretical one.

    `FEC_CONV_K` is public now, since `K - 1` is the quantity a caller reasons
    with at both ends: how far a restarted register stays wrong, and how much
    of a stream a decoder needs before its state is the data's rather than its
    own.
