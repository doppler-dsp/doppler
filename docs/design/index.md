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
- [DSSS Acquisition](dsss-acquisition.md) — stateless, parallel, dynamics-capable acquisition architecture + roadmap
- [Corr2D Interpolated Inverse](corr2d-interpolated-inverse.md) — decoupled, pffft-friendly inverse FFT size + free sub-bin interpolation
- [Spectral & Measurement API Map](spectral-api-map.md) — module dependency graph
- [Waveform Amplitude & Composition](wfmgen-composition.md) — level/power conventions for wfmgen
