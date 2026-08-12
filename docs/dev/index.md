# Contributing

The pages a contributor actually needs, in reading order:

- [Repository Map](repository-map.md) — a whole-repository view, and why an
    algorithm is implemented once, in C
- [Module Layout](module-layout.md) — where C headers, sources, and tests go
- [Adding a Module](adding-a-module.md) — step-by-step guide using `jm`
- [Error Convention](error-convention.md) — how errors are returned across the C ABI
- [DSSS Acquisition Use Cases](dsss-use-cases.md) — the two wide-Doppler operating regimes driving the acquisition design
- [Benchmarking](benchmarking.md) — running and interpreting benchmark results
- [Object Validation](validation.md) — how an object is certified: header claims → C tests proven by sabotage → the generated evidence report and its two gates
- [Validation Log](validation-log.md) — which objects are certified, their limit and finding counts, and a link straight to each object's evidence
- [Doc Examples](doc-examples.md) — every docs code snippet is discovered and tested, no opt-in list
- [Docstring Authoring](docstring-authoring.md) — write the C header Doxygen so `jm` derives top-notch Python docstrings on both faces
- [Docs Conventions](docs-conventions.md) — what's generated vs. hand-owned under `docs/`, and the nav-index/Related-pages CI gates

## Maintainer internals

Release-owner plumbing — a library user or drive-by contributor never
needs these:

- [Release](release.md) — versioning, tagging, and publishing
- [Build Internals](build-internals.md) — how the build/release pipeline turns source into a published wheel and C library tarball
- [Coverage](coverage.md) — clang source-based coverage across the C/Python/Rust harnesses

## Historical records

Kept for provenance in [`docs/dev/archive/`](https://github.com/doppler-dsp/doppler/tree/main/docs/dev/archive)
(out of the site nav; each carries a status banner):

- [wfmgen API](archive/wfmgen-api.md) — decision record for the 0.11.0 API cleanup + 0.23.0 ranged-fields addendum; the [Waveform Generator guide](../guide/wfmgen/index.md) is the current surface
- [wfm Validation Findings](archive/wfm-validation-findings.md) — what the exhaustive wfm/wfmgen validation pass uncovered (all resolved)
- [Streaming Roadmap](archive/streaming-roadmap.md) — the NATS-JetStream-for-k8s phased plan (complete)
- [NATS JetStream Transport Migration](archive/nats-jetstream-transport-migration.md) — record of the ZMQ → NATS transport migration (complete)
