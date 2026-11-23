- **802.11 zero config is 36 Mb/s on every face.** `Wlan()` and a bare
    `wfmgen --type ofdm` (were 24 and 6 Mb/s) are 16-QAM at 3/4 with the FCS
    on a 96-octet PN15 payload; a rate is given whole, and `Wlan(mbps=N)` is
    new. Old ofdm records that left the rate at its default read as 36 Mb/s;
    one-field records at 9, 12 or 24 Mb/s are refused; `mac = none` records
    without the key gain the FCS. C: `dp_wlan_create`, `_why` and
    `params_why` take `mbps` after `code_rate`, and a zero-filled pair beside
    `wlan` is refused ([#3269](https://github.com/doppler-dsp/doppler/issues/3269)).
