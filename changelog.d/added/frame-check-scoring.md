- **Scoring a coded frame, and why an outer code beats a CRC at it.**
    `wfm_frame_check()` is the receive mirror of `wfm_frame_assemble()`: it
    undoes each stage over the span the *same description* gives it, in the
    opposite order to the one they were applied in, correcting in place and
    reporting what it found. `Frame.check()` / `FrameDesc.check()` expose it,
    returning a `FrameCheck` record.

    It needs the description and the received bits and **no payload truth at
    all**, so it works on a real capture — which is what makes a truth-free
    frame error rate possible on a coded link.

    **A CRC reports one bit; an outer code reports what it cost.** Both frames
    below "pass", and only one of them is healthy:

    ```text
    clean          passed=1  units=6 ok=6  corrected=0  symbols=0
    80-symbol burst passed=1  units=6 ok=6  corrected=5  symbols=80
    E+1 in one cw   passed=0  units=6 ok=5  corrected=0  symbols=0
    ```

    A margin being spent is visible long before it is lost, and a failure
    names how much of the frame went with it rather than condemning the whole
    thing.

    **A stage the receiver does not reverse is reported as NOT CHECKED, never
    as passed** — `checked < stages` says so. The inner code is the case: a
    Viterbi is streaming and emits its decisions `depth` bits late, so frame
    checking begins after the inner decode and after frame synchronisation,
    and a frame checker never sees channel symbols. Likewise a description
    with no reversible stage at all returns "not checked" rather than "passed"
    — an FER conflating the two would score every unprotected frame as
    perfect.

- **Gallery page and worked example for the whole slice.**
    [A CCSDS CADU](docs/gallery/ccsds-link.md) walks the description end to
    end: the three fields, the three covers, why a pipeline cannot express the
    asymmetry between them, the `wfmgen` flags that reach the same thing, and
    what the receive side reports. Its numbers are executed rather than
    transcribed — the `pycon` fences run under the docs gate and the plot
    comes from `src/doppler/examples/ccsds_link_demo.py`, which self-validates
    with physical asserts (the interleaver must carry exactly a `depth * E`
    symbol burst and refuse one symbol more).
