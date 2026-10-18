- **`AsyncDsssReceiver.configure_chain_raw()` raises `ValueError` for an
    `n` MpskReceiver would refuse, instead of aborting the process.** An
    `n` that divided `sps` but was odd, under 2 or over 8 passed the old
    check and crashed the rebuild with SIGABRT; now it is refused and the
    receiver keeps its grid. RateSync's `m` domain is one public C
    predicate, `dp_ratesync_m_ok()`, which both creates use (#2421).
