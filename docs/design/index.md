# Design

Internal design documents — architecture decisions, API contracts, and module
design rationale.

- [Repository Map](repository-map.md) — where everything lives and why
- [Quantization](QUANTIZATION.md) — fixed-point pipeline design
- [Measurement Suite](measurement-suite.md) — tone, NPR, and IMD metric internals
- [DSSS Acquisition](dsss-acquisition.md) — stateless, parallel, dynamics-capable acquisition architecture + roadmap
- [Spectral & Measurement API Map](spectral-api-map.md) — module dependency graph
- [Waveform Amplitude & Composition](wfmgen-composition.md) — level/power conventions for wfmgen
- [Streaming Roadmap](streaming-roadmap.md) — NATS JetStream transport for k8s; phased plan gated on P0 benchmark
