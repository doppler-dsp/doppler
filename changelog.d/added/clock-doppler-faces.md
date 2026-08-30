- **Clock Doppler reaches JSON, Python and `wfmgen`** (#942). `doppler` /
    `doppler_rate` (ranged, `LO:HI` or a `(lo, hi)` tuple), `carrier_hz` and
    `doppler_lifetime` are scene keys, `Segment`/`Synth` kwargs and
    `--doppler` / `--doppler-rate` / `--carrier-hz` / `--doppler-lifetime`.
    Omitted at their defaults, so every recorded spec is byte-unchanged.
    `wfm_compose_draws()` and the SigMF sidecar report the **drawn** value per
    instance, so a ranged pass records its span and its flights separately.
    Guide: [Clock Doppler](../guide/wfmgen/doppler.md). `Plan.prepare()`
    refuses a Doppler source outright — a cached on-time carries no channel
    history, measured against `compose()`; the fix is #1109.
