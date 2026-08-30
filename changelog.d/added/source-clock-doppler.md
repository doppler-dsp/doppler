- **A source can carry clock Doppler (C API).** `doppler` (ppm),
    `doppler_rate` (ppm/s) and `carrier_hz` on `wfm_source_t`, ranged like
    `freq`/`snr`, rendered through `impairment/doppler_channel`. Per source:
    two transmitters in one `sum` are on different geometries. Unlike a `freq`
    offset it rescales the received time base, so symbol and chip rates move
    with the carrier and a timing loop sees the error a carrier-only offset
    hides. The channel runs through gaps too — on the noise floor — so
    `doppler_rate` is per second, not per unit of on-time. Lifetime is
    declared; `Plan` refuses `PERSIST`, whose history-dependence its
    concurrent cache cannot honour. C-only so far; the faces are #942.
