- **`AsyncDsssPool`'s rows follow a grid changed through
    `dp_async_dsss_pool_receiver()`, now pinned.** A push sizes every row
    from the largest receiver store, read at the push; a C test puts one
    receiver on a finer grid and checks the rows cover its store (#3405).
