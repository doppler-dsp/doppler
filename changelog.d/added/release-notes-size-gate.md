- **A release can no longer be too large to publish, and the same number says
    when one is due.** GitHub caps a release body at 125,000 characters, and
    `release.yml`'s `github-release` job lists `publish-python` in its
    `needs` — so an oversized body fails *after* the version is on PyPI, and
    PyPI refuses a re-upload, which removes the ordinary "rerun the release"
    recovery. Measured on 2026-08-22: `[Unreleased]` plus `changelog.d/`
    projected to **410,891** characters, 3.3x the cap, against 19,296 for
    v0.42.0's real notes.

    `scripts/check_release_notes_size.py` runs on `make lint` and in `gates`,
    and measures what would actually be **published** rather than what
    CHANGELOG.md holds: a version section may carry a `### Highlights` block,
    and `release-notes.sh` publishes that, with a link to the full section,
    when the whole section will not fit. CHANGELOG.md keeps its full depth —
    the record is the point of it.

    The cadence half is the same measurement, not a second mechanism.
    Deferring a release is exactly what makes the body grow, so the gate warns
    at half the budget ("a release is due") and fails at the cap. A "days
    since the last tag" rule would have been a rule about the clock, and the
    clock is not what breaks: a quiet fortnight ships fine, and one like this
    one does not.
