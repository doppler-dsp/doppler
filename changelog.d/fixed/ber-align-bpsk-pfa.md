- **`BerMeter.align()` holds its false-alarm probability at BPSK.** It
    thresholded a circular null, but BPSK's is rank-one, so a 1e-6 search
    false-aligned about 3e-3 of the time (#2496). Every record alignment in
    the tree is unchanged. The receiver test harness's per-frame sync states
    the 1e-3 it was running at (`DP_RX_SYNC_PFA`) and confirms only at lag 0.
    The Barker-13 burst frame's sync miss now reads 0.836 where it read 0.845
    ([rx-test §6](../../docs/design/rx-test.md)).
