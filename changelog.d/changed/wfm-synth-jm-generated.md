- **`wfm._SynthEngine` is jm-generated** (doppler#1886). Its six setters
    (`set_rrc`, `set_bits`, `set_symbols`, `set_dsss_chips`,
    `set_dsss_cont`, `set_dsss_window`) stay hand-written, in
    `wfm_ext_wfm_synth_extra.c`, and are declared as `extra_methods`, so
    they gain stubs and numpy docstrings with tested examples for the first
    time. The binding now matches its manifest: the constructor takes its
    positional arguments in the manifest's order, with `snr_mode` after
    `snr`; `steps()` takes `n=`; and the default `seed` is 1, as the stub
    always said. It was 0, which only a seedless noisy engine could see.
    The fragment leaves the `-Wall -Wextra` exempt list.
