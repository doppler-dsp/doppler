- **`Frame` is certified** — the 28th object, and the one that answers "are
    we CCSDS-locked?" with evidence: arbitrary frames containing nothing
    CCSDS build and self-check, and a CADU is the same three calls with
    different covers declared. 16 limits, 4 findings. Certifying it produced
    [#1125](https://github.com/doppler-dsp/doppler/issues/1125) — the open
    stage kind the design is staked on is unusable from Python.
    [Evidence][frame-cert].

[frame-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_frame/results.md
