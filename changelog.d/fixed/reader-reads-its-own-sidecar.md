- **A round trip through `Writer` and `Reader` no longer returns garbage.**
    The writer records a raw or CSV capture's wire type and rate in a
    `<path>.sigmf-meta` sidecar; the reader opened the same path and did not
    read it, so an untold `ci16` capture came back as `cf32` -- half the
    samples, at the wrong stride, silently
    ([#1120](https://github.com/doppler-dsp/doppler/issues/1120)).
    `sample_type` now defaults to `"auto"`, which takes the type, byte order
    and rate from that sidecar; naming a type still overrides it, and a
    capture with no sidecar still reads as `cf32`.
