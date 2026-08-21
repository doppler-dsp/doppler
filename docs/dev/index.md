# Contributing

**Start with [Adding an Algorithm](contributing/adding-algorithms.md)** — it
is the lifecycle spine, and it says what order the phases go in, who owns the
how for each, and which gate proves each one. Everything in
[`contributing/`](contributing/adding-algorithms.md) is a page it links to.

This list is a catalogue, not a reading order: it says what each page *is*,
so you can find the one that owns your question.

## The contributing guide

- [Adding an Algorithm](contributing/adding-algorithms.md) — the lifecycle
    spine: the order of the phases, who owns each, and the four places
    evidence lives
- [Repository Map](contributing/repository-map.md) — a whole-repository view,
    and why an algorithm is implemented once, in C
- [Module Layout](contributing/module-layout.md) — where C headers, sources, and tests go
- [Adding a Module](contributing/adding-a-module.md) — step-by-step guide using `jm`
- [Error Convention](contributing/error-convention.md) — how errors are returned across the C ABI
- [DSSS Acquisition Use Cases](contributing/dsss-use-cases.md) — the two wide-Doppler operating regimes driving the acquisition design
- [Benchmarking](contributing/benchmarking.md) — running and interpreting benchmark results
- [Measuring a Receiver](contributing/measuring-a-receiver.md) — the path from a receiver to a number you can defend: the adapter, the four metrics together, how to read a refusal, and what to gate on
- [Object Validation](contributing/validation.md) — how an object is certified: header claims → C tests proven by sabotage → the generated evidence report and its two gates
- [Validation Log](contributing/validation-log.md) — which objects are certified, their limit and finding counts, and a link straight to each object's evidence
- [Doc Examples](contributing/doc-examples.md) — every docs code snippet is discovered and tested, no opt-in list
- [Docstring Authoring](contributing/docstring-authoring.md) — write the C header Doxygen so `jm` derives top-notch Python docstrings on both faces
- [Docs Conventions](contributing/docs-conventions.md) — what's generated vs. hand-owned under `docs/`, and the nav-index/Related-pages CI gates

## Maintainer internals

Release-owner plumbing — a library user or drive-by contributor never
needs these:

- [Open Issues](issues.md) — the whole backlog, tiered by the kind of harm
    each issue does rather than by age or label; generated from
    [`issue-tiers.toml`](issue-tiers.toml) by `make issues`
- [Continuous Integration](ci.md) — the pinned toolchain image and how to run
    CI's environment yourself (`make ci-gates`), the compiler cache, and the
    gates that watch CI itself
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
