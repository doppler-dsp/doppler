- **A release no longer times out on CI that is merely queued.** `verify-ci`
    ran its own 60 x 20s poll, a 20-minute window that counted queue time.
    main's slowest push CI in two weeks took 18.2 min, so one run queued ahead
    was enough. It now runs the org standard's `make release-verify-ci`,
    which waits while the tagged SHA's run is alive, capped at 120 min.
