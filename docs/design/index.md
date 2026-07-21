# Design

Architecture decisions, API contracts, and the algorithm rationale behind
shipped features — for anyone who wants the "why" and "how it works"
underneath a feature they're already using, not just contributors. (Repo
layout and forward-looking roadmaps live under [Contributing](../dev/index.md)
instead.)

- [API Taxonomy](api-taxonomy.md) — the DSP building-block hierarchy and its
    naming axis, plus concrete rename proposals
- [Quantization](QUANTIZATION.md) — fixed-point pipeline design
- [Measurement Suite](measurement-suite.md) — tone, NPR, and IMD metric internals
- [State Serialization](state-serialization.md) — the standard bytes interface for bit-exact checkpoint/resume
- [Telemetry](telemetry.md) — zero-cost scalar taps (loop stress, AGC gain) for running pipelines
- [DSSS Acquisition](dsss-acquisition.md) — stateless, parallel, dynamics-capable acquisition architecture + roadmap
- [Async DSSS Receiver Spec](async-dsss-spec.md) — the target waveform and receiver specification (CCSDS Gold-1023, 3.069 Mcps, 2700 bps, ±50 kHz, \<500 Hz/s) the async DSSS receiver is built against
- [Asynchronous Symbol Despreader](async-symbol-despreader.md) — despreading when the data-symbol rate is asynchronous to the code-epoch rate
- [Asynchronous Data on a Repeating PN Code](async-despreader-working-design.md) — the working design behind the async despreader, assuming at most one data transition per code epoch
- [Timing Lock Detector](timing_lock_detector.md) — SymbolSync's Gardner/DTTL lock statistic and sizing formula
- [MPSK Receiver](mpsk.md) — streaming M-PSK receiver architecture and carrier-recovery design
- [Corr2D Interpolated Inverse](corr2d-interpolated-inverse.md) — decoupled, pffft-friendly inverse FFT size + free sub-bin interpolation
- [Spectral & Measurement API Map](spectral-api-map.md) — module dependency graph
- [Waveform Amplitude & Composition](wfmgen-composition.md) — level/power conventions for wfmgen
- [Continuously Variable Resampler](RESAMPLER.md) — polyphase resampler architecture, testing, and performance optimizations
- [Acquisition Kernel](acq-fn.md) — pure-functional acquisition kernel for the elastic fleet
