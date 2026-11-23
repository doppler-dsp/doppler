- **802.11 zero config is 36 Mb/s on every face.** `Wlan()` and bare
    `wfmgen --type ofdm` (were 24 and 6 Mb/s) are 16-QAM 3/4 with the FCS on
    96 octets of PN15; a rate is given whole; `Wlan(mbps=N)` is new. Every
    older 6 Mb/s ofdm record, given or defaulted (its writer did not record
    the difference), reads as 36; one-field 9, 12 and 24 Mb/s records are
    refused; every `mac = none` one gains the FCS; ofdm sources of one segment
    at the default fs that differ in bandwidth are refused. C:
    `dp_wlan_create`, `_why`, `params_why` take `mbps` after `code_rate`; a
    zero-filled pair beside `wlan` is refused
    ([#3269](https://github.com/doppler-dsp/doppler/issues/3269)).
