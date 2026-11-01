- **`dp_acq_set_threads()` starts no more workers than the search has
    tiles.** Every fan hands each tile one task, so a worker past
    `window_bins` only woke and waited each epoch. At 0, the online cores, a
    three-tile search ran 18 workers, 15 of them idle. The engine's
    `threads` (`Acquisition.threads`) now reads the capped count.
