- **`CellAsyncDsssPool`** (C: `async_dsss_pool_create_cell`, part of #1283):
    the population on `CellAsyncDsssReceiver`s — no refine, every slot
    corrected on the searcher's own block timing. The lifecycle, the table,
    the zone and the blob are `AsyncDsssPool`'s; it refuses a searcher a cell
    receiver cannot take (`D ≥ 13` at 5 Mcps over Gold-1023, the carrier
    loop's pull-in bound now named once, `ASYNC_DSSS_RX_CARRIER_PULLIN_HZ`).
    A cell seed's carrier residual is now estimated on the live chain's own
    despread stream (the refine's estimator, no second chain), and the pool
    advances a never-locked row on the seed's clock (design §8.2; measured
    at parity with the hand-off pool on the soak, §12.27).
