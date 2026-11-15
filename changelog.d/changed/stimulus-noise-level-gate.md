- **`make lint` refuses a hand-scaled `dp_cgauss` noise level in C.**
    `dp_cgauss` is already unit power, so a further `/ sqrt (2.0)` per
    component delivers 3.01 dB less noise than its label
    ([#689](https://github.com/doppler-dsp/doppler/issues/689)).
    `scripts/check_stimulus_sources.py` names the nine sites, one
    allowlist line each; the list only shrinks.
