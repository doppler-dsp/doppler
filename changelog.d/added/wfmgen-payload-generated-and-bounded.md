- **A payload can be generated, or just bounded.** The payload is a sequence
    like its three siblings now, so `--payload-gen pn:65535:16` records six
    numbers instead of a 65k-character string — and the frame descriptor's
    payload field was the last place gh-762's flattening to `WFM_SEQ_LITERAL`
    survived. `--payload-len N` bounds it at N bits filled from the source's
    own PN, which **retires #755's refusal**: `--type bpsk|qpsk|pn` with frame
    flags used to exit 2, because their data is an endless LFSR and nothing
    said where the payload stopped.
    Closes [#762](https://github.com/doppler-dsp/doppler/issues/762).
