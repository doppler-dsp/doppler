- **A data source on a DSSS burst**
    ([#1719](https://github.com/doppler-dsp/doppler/issues/1719)).
    `--data` / `--data-from-file` / `Synth(data=)` on `--type dsss` sends
    one burst per `--data-len` chunk: the preamble, then that chunk's frame
    spread by the data code. The run is the bursts, and stdin ends it where
    the input ends. `dp_wfm_dsss_desc_chips_data` builds one such burst.
