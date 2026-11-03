- **A forged `sym_idx` no longer hangs `wfm_synth.set_state`.** The
    continuous-DSSS symbol clock is checked against the chips emitted
    before anything commits, so a blob whose `sym_idx` or `chip_n` is not
    one the kernel could have reached is refused with the synth unchanged.
    It used to spin forever in `dp_wfm_dsss_cont_edge`. See [#2745](https://github.com/doppler-dsp/doppler/issues/2745).
