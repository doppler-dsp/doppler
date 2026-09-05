- **The CI image digest has one home.** It used to be written both in
    `.github/ci-images.env` and as six literal `container:` refs in `ci.yml`,
    which `ci-image-check` required to agree while nothing could move both —
    so the nightly's repin branch was born failing lint and that path had
    never once completed. Workflows now name no digest: a `pin` job reads the
    file and every containerised job consumes its output, and the gate
    resolves those expressions instead of skipping them
    ([#1215](https://github.com/doppler-dsp/doppler/issues/1215)).
