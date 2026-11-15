- **The TSan job no longer dies on an ubuntu-26.04 host.** There the TSan
    runtime aborts at startup ("unexpected memory mapping") before any test
    runs, so `ubuntu-latest`'s move to 26.04 blocked every code PR. The
    sanitizer job now runs on `ubuntu-24.04`; the fix on 26.04 is #3058.
