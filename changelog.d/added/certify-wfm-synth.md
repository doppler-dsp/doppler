- **`Synth` is certified** — the wfmgen ladder's first object through the
    validation process, and the 25th overall: 19 limits, all holding, 6
    findings. The claim inventory found the header's own SSOT untested —
    `wfm_synth_snr_over_fs`, `wfm_synth_bps`, `wfm_synth_set_dsss_chips`,
    `wfm_synth_reseed_noise` and all ten accessors had zero mentions in any
    C test. [Evidence][synth-cert].

[synth-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_synth/results.md
