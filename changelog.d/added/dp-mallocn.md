- **`dp_mallocn`, `dp_xmallocn` and `dp_xreallocn` allocate `n` elements
    with the product checked** (`clib_common.h`). `dp_mallocn` returns NULL
    on overflow, for a path that refuses; the other two abort; a zero count
    is a one-byte allocation, never NULL. The `lint-alloc-helpers` gate now
    also fails on a new inline product in any size argument of an
    allocator, `calloc`'s included; the existing sites are a shrink-only
    ratchet (`scripts/.alloc-product-ratchet`).
