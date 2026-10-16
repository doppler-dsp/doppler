- **A stock-image pull that ECR Public refuses for its data quota is served
    by a mirror.** `toomanyrequests: Data limit exceeded` outlasted
    `scripts/stock-pull.sh`'s 75 s of retries on all seven legs it hit (two
    on 2026-10-12, five on 2026-10-16), so it now goes at once to
    `STOCK_MIRROR` (`mirror.gcr.io/library`, whose index digests matched on
    2026-10-16), as does a rate limit that outlasts them. Each pull logs
    docker's size for the image; a per-run figure in metered bytes is
    #2381 (#2256).
