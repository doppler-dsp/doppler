- **The async receiver's extreme-SNR stress test moved to `make characterize`.**
    It fed three full 5.5 M-sample captures per push to try to break the
    receiver; the full 20–200 dB sweep is now a characterization subject, and
    the per-push suite keeps one short noiseless check of the same failure
    mode.
