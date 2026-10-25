- **The CI-image rebuild no longer fails when a registry rate-limits it.** It
    used to pull its Ubuntu bases anonymously from Docker Hub, inside
    BuildKit, where nothing retried a 429. It now gets them through
    `STOCK_REGISTRY`: a probe fetches each pinned base's manifest first,
    retrying, and falls back to `STOCK_MIRROR`. The build then pulls from
    whichever registry answered. This uses canonical's new
    `CI_IMAGE_BASE_REGISTRY`/`CI_IMAGE_BASE_MIRROR` settings, and the
    stock-image gate now exempts no container build
    ([#1982](https://github.com/doppler-dsp/doppler/issues/1982)).
