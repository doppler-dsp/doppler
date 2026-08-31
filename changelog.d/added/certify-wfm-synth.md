- **`Synth` is certified — the wfmgen ladder's first object through the
    validation process** (25th overall). The evidence is
    [`src/doppler/wfm/tests/validation/wfm_synth/results.md`][synth-cert]:
    19 limits, all holding, and 6 findings. The claim inventory found the
    header's own SSOT untested — `wfm_synth_snr_over_fs`, the conversion
    it calls "the one place this arithmetic lives" and whose failure it
    calls silent, had zero mentions in every C test in the tree, as did
    `wfm_synth_bps`, `wfm_synth_set_dsss_chips`, `wfm_synth_reseed_noise`
    and all ten accessors. Seven new C sections close them, each proven by
    sabotage and each scored against a truth that is not the other path: a
    direct convolution for the RRC, hand-derived literals for the SNR
    modes, an m-sequence's own autocorrelation for the PN.

[synth-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_synth/results.md
