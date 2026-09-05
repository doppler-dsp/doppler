- **A pending CI-image repin is now a gate, not a PR nobody could open.** The
    nightly rebuild asks whether upstream packages moved — the one drift
    `ci-image-check` cannot see — and reported it by opening a PR the org
    forbids Actions from creating, so it died on `gh pr create` and sat red
    for three releases while gating nothing. It now pushes `ci/repin-image`
    and stops; `make ci-image-repin-check` compares the package fingerprints
    (never the digests) and fails as a required check until the repin lands
    ([#1212](https://github.com/doppler-dsp/doppler/issues/1212)).
