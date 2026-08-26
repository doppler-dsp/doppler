- **The `acq` engine is certified** — 15 limits re-asserted on every push,
    plus two claims that had zero mentions in either language:
    `samples_consumed` (a per-*hit* anchor, previously pinned by nothing) and
    `noise_mode`, whose four CFAR references move the gating statistic by
    ~15 dB on one burst. Two usability gaps filed rather than fixed:
    [#999](https://github.com/doppler-dsp/doppler/issues/999) (a push shorter
    than one dwell returns nothing, and nothing says how long a dwell is) and
    [#998](https://github.com/doppler-dsp/doppler/issues/998) (no property
    reports the searched Doppler reach; `doppler_span_hz` reads as though it
    does). Evidence: `src/doppler/dsss/tests/validation/acq/results.md`.
