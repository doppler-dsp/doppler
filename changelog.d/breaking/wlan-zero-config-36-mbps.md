- **802.11 zero config is 36 Mb/s on every face.** `Wlan()` and bare
    `wfmgen --type ofdm` (were 24 and 6 Mb/s) are 16-QAM 3/4 with the FCS on
    96 octets of PN15; `Wlan(mbps=N)` is new. A rate is given whole: half a
    pair is refused where it is written, as in
    `Synth(type="ofdm", constellation="bpsk")`. Older 6 Mb/s ofdm records
    read as 36, one-field 9, 12 and 24 Mb/s records are refused,
    `mac = none` gains the FCS, and a segment's ofdm sources at the default
    fs must share one bandwidth. C: `dp_wlan_create`, `_why`, `params_why`
    take `mbps` after `code_rate`
    ([#3269](https://github.com/doppler-dsp/doppler/issues/3269)).
