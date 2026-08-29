- **A waveform source carries `wfm_seq_t` for its preamble, spreading code
    and sync word, instead of three pointer/length pairs.** Those pairs could
    only ever describe a literal run of bits; `wfm_seq_t` already names a run
    of bits *however produced*, so it subsumes them — the literal case is
    `kind = WFM_SEQ_LITERAL`, which is what every caller already meant. This
    is the carrier for the generated PN/Gold kinds, not yet a face that can
    spell one: **behaviour-neutral**, with `wfmgen_flag_matrix.json`
    byte-identical and 27 of its 35 cases replayed from their own `--record`.
    Enabled by just-makeit 0.71.0's `c_ptr`/`c_len`.
    Step 1 of [#762](https://github.com/doppler-dsp/doppler/issues/762).
