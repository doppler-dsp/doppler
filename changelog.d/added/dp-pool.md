- **`dp_pool_create` / `dp_pool_run` / `dp_pool_destroy`** in `dp_parallel.h`:
    the bounded parallel-for's contract — every index to exactly one worker,
    bit-identical to the serial loop, the range always completed — over
    helpers created once and parked between runs, so a fan costs a hand-off,
    not a thread creation per worker. A pool of one, or `NULL`, runs on the
    caller. The searcher's roll per thread (design §2.3) stands on it.
