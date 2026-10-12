- **`BurstCapture` and `BurstAcquisition` compensate a sweeping carrier:
    `rate_hypotheses=N` de-chirps the coherent block at N rates across
    `±doppler_rate` instead of capping the depth at it.** On a Zadoff-Chu
    127 x 16 preamble sweeping 0.0037 of fs, the capture places 0.55 of bursts
    capped and 0.91 - 0.93 at N = 3, 5 or 9 (1000 trials, Wilson 95%), for
    about N times the per-dwell work; `BurstAcquisition` hits carry the winning
    rate as `doppler_rate_hz_s`. Design: [`burst-capture.md` 11.5](docs/design/burst-capture.md);
    measured by `validate_acq_rate_pd`.
