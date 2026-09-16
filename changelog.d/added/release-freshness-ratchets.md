- **A tag is refused when its gallery plots or published benchmarks are
    stale.** `docs/dev/release.md` §2 and §2b both said "regenerate it if it
    changed since the last release" and nothing checked either, so forgetting
    was invisible: a gallery page shows a PNG of code that no longer exists,
    and a release publishes numbers measured against a different tree. Both
    read as correct. `release-freshness-check` is a `tag-release`
    prerequisite — the one moment the question means anything — and is
    diff-based rather than regenerate-and-compare, because a PNG is not
    byte-stable across a re-render and a flapping gate gets disabled.
