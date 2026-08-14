# Design

Architecture decisions, API contracts, and the algorithm rationale behind
shipped features — for anyone who wants the "why" and "how it works"
underneath a feature they're already using, not just contributors. (Repo
layout and forward-looking roadmaps live under [Contributing](../dev/index.md)
instead.)

- [API Taxonomy](api-taxonomy.md) — the DSP building-block hierarchy and its
    naming axis, plus concrete rename proposals
- [The NCO](nco.md) — the 32-bit phase accumulator every rate in the
    library stands on: why an integer register instead of a double, the
    one float boundary, and how carrier and timing read the same thing
- [Exponential Moving Average](ema.md) — the running average every estimator
    in the library is built on: which of its two algebraic forms is the right
    one and why that was measured, what its boundaries guarantee, and why an
    EMA is not a loop filter
- [The Loop Filter](loop-filter.md) — the second-order PI every tracking loop
    in the library embeds by value: where its gains come from, the unit
    condition its `bn` promise depends on, and what it deliberately does not
    bound
- [Quantization](QUANTIZATION.md) — fixed-point pipeline design
- [CIC Decimator](cic.md) — the fixed-point input budget: DC gain and PAPR headroom
- [Measurement Suite](measurement-suite.md) — tone, NPR, and IMD metric internals
- [State Serialization](state-serialization.md) — the standard bytes interface for bit-exact checkpoint/resume
- [Telemetry](telemetry.md) — zero-cost scalar taps (loop stress, AGC gain) for running pipelines
- [DSSS Acquisition](dsss-acquisition.md) — stateless, parallel, dynamics-capable acquisition architecture + roadmap
- [Async DSSS Receiver Spec](async-dsss-spec.md) — the target waveform and receiver specification (CCSDS Gold-1023, 3.069 Mcps, 2700 bps, ±50 kHz, \<500 Hz/s) the async DSSS receiver is built against
- [Asynchronous Symbol Despreader](async-symbol-despreader.md) — despreading when the data-symbol rate is asynchronous to the code-epoch rate
- [Asynchronous Data on a Repeating PN Code](async-despreader-working-design.md) — the working design behind the async despreader, assuming at most one data transition per code epoch
- [Automatic Gain Control](agc.md) — the log-domain level loop every receiver
    stands on: why the filter is in dB and the detector is not, why the loop
    must be total under any input, and what level alone cannot tell it
- [Lock Detection](lock-detect.md) — the sizing chain every lock detector shares, and the independence it assumes
- [Timing Lock Detector](timing_lock_detector.md) — SymbolSync's Gardner/DTTL lock statistic and sizing formula
- [Symbol Timing on a Rate Cascade](ratesync-timing.md) — RateSync: why the matched filter and the interpolator are one dot product, why `ctrl` is referenced to the terminal stage's rate, and why the T/2 parity resolves itself
- [MPSK Receiver](mpsk.md) — streaming M-PSK receiver architecture and carrier-recovery design
- [Corr2D Interpolated Inverse](corr2d-interpolated-inverse.md) — decoupled, pffft-friendly inverse FFT size + free sub-bin interpolation
- [Spectral & Measurement API Map](spectral-api-map.md) — module dependency graph
- [Waveform Amplitude & Composition](wfmgen-composition.md) — level/power conventions for wfmgen
- [Continuously Variable Resampler](RESAMPLER.md) — polyphase resampler architecture, testing, and performance optimizations
- [Acquisition Kernel](acq-fn.md) — pure-functional acquisition kernel for the elastic fleet
