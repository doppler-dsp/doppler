- **`DsssBurstReceiver`: a spurious detection no longer costs the next real
    burst** ([#1004](https://github.com/doppler-dsp/doppler/issues/1004)). The
    dedup rule armed its suppression window on **every** detection,
    unconditionally and at detection time, so one noise crossing blinded the
    search for a whole burst length — 2 of 5 bursts lost on the example
    capture. It conflated two jobs now done separately: coalescing detections
    of the *same preamble*, and excluding a burst's *own payload*. Now 5/5 at
    every frame phase, zero spurious returns; the genuine near-knee framing
    residual is split out as
    [#1006](https://github.com/doppler-dsp/doppler/issues/1006).
