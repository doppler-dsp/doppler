- **`DsssBurstReceiver` composes `BurstCapture`.** The ring, the refine stage,
    the retention rule and the claim rule moved out to the object certified for
    them; what is left is driving the demodulator and owning the frame. The
    public API is unchanged and the output is **bit-identical** — every payload
    bit, event field and LLR, across single-call, blocked and mid-preamble
    resume. Its blob nests the capture's (`STATE_VERSION` 4 → 5), and
    `reset()`'s ring bug is gone by construction.
    Closes [#1169](https://github.com/doppler-dsp/doppler/issues/1169).
