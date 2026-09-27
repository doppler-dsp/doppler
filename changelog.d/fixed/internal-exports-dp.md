- **doppler's private NATS wrappers no longer export names in nats.c's own
    namespace** ([#1565](https://github.com/doppler-dsp/doppler/issues/1565)):
    the 14 `nats_*` helpers in `stream_nats.c` and `wfm_draw_samples` are
    now `dp__*`, doppler's internal spelling. A nats.c release adding, say,
    `nats_flush` would otherwise have broken the link. Not public API.
