- **A data source on continuous DSSS**
    ([#1719](https://github.com/doppler-dsp/doppler/issues/1719)).
    `--data` / `--data-from-file` / `Synth(data=)` with `--symbol-rate`
    sends one bit per data symbol, with no frame, and a finite source ends
    the run with its last symbol. `--data-len`, `--fill` and `--realtime`
    over stdin are refused by name. With no data source it still sends its
    seeded PRBS, byte for byte. `dp_wfm_source_data_samples` is the one
    length a data-source run is measured in.
- **One continuous-DSSS symbol clock, exact under `-ffast-math`**
    ([#1725](https://github.com/doppler-dsp/doppler/issues/1725)).
    `dp_wfm_dsss_cont_edge` is the first chip of a data symbol. The synth's
    kernel, a data source's run length and `dp_wfm_cont_dsss_chips` all use
    it, so they cannot disagree about where a symbol starts. It computes the
    exact quotient: a fast-math rewrite had put an edge a chip late.
