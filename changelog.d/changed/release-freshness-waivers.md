- **A release can answer the freshness gate in writing.**
    `release-freshness-check` is path-granular, so a change confined to a
    platform the release does not measure still read as a stale plot or
    stale benchmarks. `release-waivers/v<ver>.md` now waives one
    named item per line, each with a reason the gate prints as it runs; a
    waiver that matches nothing stale fails the gate, so it cannot outlive
    its reason.
