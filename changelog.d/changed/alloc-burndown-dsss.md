- **The DSSS composers size their buffers through the checked
    helpers.** The 21 inline size products in `AsyncDsssReceiver`,
    `AsyncDsssPool`, `DsssReceiver`, `DsssBurstReceiver` and
    `dp_parallel.h` moved onto `dp_xmallocn`/`dp_xreallocn`/`dp_mallocn`,
    and those five files leave `scripts/.alloc-product-ratchet`.
