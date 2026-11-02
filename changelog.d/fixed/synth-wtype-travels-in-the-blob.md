- **`_SynthEngine.set_wtype` refuses a type its engine cannot run, and the
    type travels in the blob.** A tone engine switched to PN, BPSK or QPSK
    read a PN source `create()` never built and segfaulted; that now raises
    `ValueError`. The type is the first payload word (blob v6), and a restore
    refuses one the target cannot run, unchanged.
    See [#2095](https://github.com/doppler-dsp/doppler/issues/2095).
