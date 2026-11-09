- **`dp_mallocn`, `dp_reallocn`, `dp_xmallocn` and `dp_xreallocn` allocate
    `n` elements with the product checked** (`clib_common.h`). `dp_mallocn`
    and `dp_reallocn` return NULL on overflow, for a path that refuses
    (`dp_reallocn` leaves the old block untouched and the caller's, as a
    failed `realloc` does); the other two abort; a zero count is a
    one-byte allocation, never NULL and never a free. **`dp_callocn` and
    `dp_xcallocn`** are the zeroed pair for a count that is itself a
    product, `n * m` elements of `size` bytes: `n * m` is checked inside
    and `calloc` keeps its own check of the rest, so neither a hand-written
    guard nor `dp_xcalloc (dp_size_mul_sat (n, m), size)` is needed;
    `dp_callocn` returns NULL on either overflow or OOM, `dp_xcallocn`
    aborts. The `lint-alloc-helpers` gate now also fails on a new inline
    product in any size argument of an allocator, `calloc`'s included; the
    existing sites are a shrink-only ratchet
    (`scripts/.alloc-product-ratchet`).
