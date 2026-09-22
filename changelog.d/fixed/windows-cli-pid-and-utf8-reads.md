- **The `doppler` CLI and config loaders on Windows.** `doppler stop`
    checked whether a pipeline block was alive with `os.kill(pid, 0)`,
    which on Windows is `TerminateProcess`: the check itself killed the
    process. It now asks through a query-only handle, and `doppler kill` sends
    the signal Windows has. Dopplerfiles, compose YAML, chain state and
    specan's page were read with the locale codec (cp1252 on Windows) and
    failed on the first non-ASCII byte; every read names UTF-8, and a lint
    gate keeps it that way.
