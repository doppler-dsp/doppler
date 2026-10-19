- **`BerMeter.align()` holds its false-alarm probability at BPSK.** It
    thresholded a circular null, but BPSK's is rank-one, so a 1e-6 search
    false-aligned about 3e-3 of the time (#2496). Every record alignment in
    the tree is unchanged. The receiver test harness's per-frame sync now
    states the 1e-3 it was running at (`DP_RX_SYNC_PFA`), and the Barker-13
    burst frame's sync miss reads 0.836 where it read 0.845
    ([rx-test §6](../../docs/design/rx-test.md)).
