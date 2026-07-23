# Guides

!!! note "Looking for the Waveform Generator?"

    [Waveform Generator (wfmgen)](wfmgen/index.md) has its own top-level nav
    section — composing and streaming waveforms — so it isn't listed below.

- [DSSS Burst Acquisition](dsss-acquisition.md) — acquiring code phase and Doppler with `Acquisition`
- [Lock Detection Across doppler.track](lock-detection.md) — which `configure_lock` to call, and why, for every tracking loop
- [Power Spectra & Measurements](spectral-psd.md) — PSD estimation, tone and NPR measurement
- [Real-Time Pacing & Timestamping](timing.md) — sample-accurate playback timing
- [Fixed-Point Arithmetic](fixed-point.md) — Q15 / UQ15 types and quantization
- [Checkpoint & Resume](state-serialization.md) — bit-exact `get_state`/`set_state`, composed receivers, and elastic pod hand-off
