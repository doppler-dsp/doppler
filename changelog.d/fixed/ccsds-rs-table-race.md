- **The CCSDS Reed-Solomon tables were derived under a data race, and
    `make test-tsan` now exists to say so.** `native/src/ccsds_tm/rs.c` built
    its field behind a plain `ready` flag: two threads reaching any entry
    point first would both see `ready == 0`, both call `rs_init`, and — the
    part that makes it undefined behaviour rather than a wasted
    initialisation — one could read the tables while the other was still
    writing them. Now `pthread_once`.

    It survived because it was unreachable: nothing called the encoder. Two
    things already in the tree make the first call the racy one —
    `dp_parallel.h` fans per-source signal builds across cores, and every
    block method declares `nogil = true`, so a Python encoder driven from a
    thread pool is the same race with a different scheduler. A first call is
    exactly what a freshly imported module makes.

    Precomputing the tables as `static const` would also have been
    thread-safe, and was rejected: it moves `g(x)` from something *derived*
    to something transcribed, and the derivation is what `test_ccsds_tm_rs`
    holds to Annex G. Thread safety should not cost the evidence.
