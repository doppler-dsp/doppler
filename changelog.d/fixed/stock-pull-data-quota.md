- **A stock-image pull that ECR Public refuses for its data quota is served
    by a mirror.** `toomanyrequests: Data limit exceeded` outlasted
    `scripts/stock-pull.sh`'s 75 s of retries on every leg it hit (five red
    legs on four PRs on 2026-10-16), so it now goes at once to
    `STOCK_MIRROR` (`mirror.gcr.io/library`, the same digests), as does a
    rate limit that outlasts them. Each pull logs its size (#2256).
