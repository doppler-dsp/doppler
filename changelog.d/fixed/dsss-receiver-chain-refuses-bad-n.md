- **`DsssReceiver.configure_chain_raw()` raises `ValueError` for an `n`
    MpskReceiver would refuse, instead of aborting the process.** The same
    defect `AsyncDsssReceiver` had (#2421): an `n` that divided `sps` but
    was odd, under 2 or over 8 crashed the rebuild with SIGABRT; it is now
    refused through `dp_ratesync_m_ok()` and the receiver keeps its grid
    (#2426).
