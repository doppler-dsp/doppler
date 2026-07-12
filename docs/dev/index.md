# Contributing

- [Module Layout](module-layout.md) — where C headers, sources, and tests go
- [Repository Map](repository-map.md) — a whole-repository view, and why an
    algorithm is implemented once, in C
- [Adding a Module](adding-a-module.md) — step-by-step guide using `jm`
- [Doc Examples](doc-examples.md) — every docs code snippet is discovered and tested, no opt-in list
- [Docs Conventions](docs-conventions.md) — what's generated vs. hand-owned under `docs/`, and the nav-index/Related-pages CI gates
- [Error Convention](error-convention.md) — how errors are returned across the C ABI
- [wfmgen API](wfmgen/api.md) — user-facing API surface, target, and decisions for the 0.11 cleanup
- [wfm Validation Findings](wfm-validation-findings.md) — what the exhaustive wfm/wfmgen validation pass uncovered
- [DSSS Acquisition Use Cases](dsss-use-cases.md) — the two wide-Doppler operating regimes driving the acquisition design
- [Benchmarking](benchmarking.md) — running and interpreting benchmark results
- [Coverage](coverage.md) — clang source-based coverage across the C/Python/Rust harnesses
- [Streaming Roadmap](streaming-roadmap.md) — NATS JetStream transport for k8s; phased plan gated on P0 benchmark
- [NATS JetStream Transport Migration](nats-jetstream-transport-migration.md) — historical record of the ZMQ → NATS transport migration
- [Release](release.md) — versioning, tagging, and publishing
- [Build Internals](build-internals.md) — how the build/release pipeline turns source into a published wheel and C library tarball
