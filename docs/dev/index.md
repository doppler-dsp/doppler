# Contributing

**Start with [Adding an Algorithm](contributing/adding-algorithms.md)** — it
is the lifecycle spine, and it says what order the phases go in, who owns the
how for each, and which gate proves each one. Everything in
[`contributing/`](contributing/adding-algorithms.md) is a page it links to.

Every contributor page is in the navigation under **Internals →
Contributing**: the contributing guide, then the maintainer internals.
The maintainer internals are release-owner plumbing; a library user or
drive-by contributor never needs them.

## Historical records

Kept for provenance in [`docs/dev/archive/`](https://github.com/doppler-dsp/doppler/tree/main/docs/dev/archive)
(out of the site nav; each carries a status banner):

- [wfmgen API](archive/wfmgen-api.md) — decision record for the 0.11.0 API cleanup + 0.23.0 ranged-fields addendum; the [Waveform Generator guide](../guide/wfmgen/index.md) is the current surface
- [wfm Validation Findings](archive/wfm-validation-findings.md) — what the exhaustive wfm/wfmgen validation pass uncovered (all resolved)
- [Streaming Roadmap](archive/streaming-roadmap.md) — the NATS-JetStream-for-k8s phased plan (complete)
- [NATS JetStream Transport Migration](archive/nats-jetstream-transport-migration.md) — record of the ZMQ → NATS transport migration (complete)
