- **`Writer` is certified** — the 26th object, and wfm's second: 16 limits,
    all holding, 5 findings. Certifying it found
    [#1120](https://github.com/doppler-dsp/doppler/issues/1120) — the writer
    records a raw capture's sample type in a sidecar the reader does not
    read, so an untold raw capture comes back as `cf32` with garbage values
    and no error. [Evidence][writer-cert].

[writer-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_writer/results.md
