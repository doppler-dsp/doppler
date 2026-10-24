- **A release's container images no longer fail when ECR Public's
    anonymous data quota runs out.** The image builds pulled their base images
    inside BuildKit, where the helper's mirror fallback could not reach.
    v0.73.0 and v0.74.0 each lost an image job that way after PyPI had
    published. A first step now pulls the same images through
    `scripts/stock-pull.sh` as a probe, and the builds use whichever registry
    it needed ([#2572](https://github.com/doppler-dsp/doppler/issues/2572)).
