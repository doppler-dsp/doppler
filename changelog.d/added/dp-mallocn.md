- **`dp_mallocn`, `dp_xmallocn` and `dp_xreallocn` allocate `n` elements
    with the product checked** (`clib_common.h`). `dp_mallocn` returns NULL
    on overflow, for a path that refuses; the other two abort. `make   lint-alloc-helpers` now also fails on a new `malloc`/`realloc` sized by
    an inline product; the 200 existing sites are a shrink-only ratchet
    (`scripts/.alloc-product-ratchet`).
