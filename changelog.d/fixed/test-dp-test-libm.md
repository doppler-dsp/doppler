- **`test_dp_test` did not link libm, and only the coverage build noticed.**
    The self-test calls `cabs` once, to show that `dp_cnear` accepts a diagonal
    error a magnitude bound would reject. gcc at `-O2` folds that call away and
    needs no libm; the coverage build is clang at `-O0` and emits it, so the
    missing `target_link_libraries(... m)` was invisible to `make test` and an
    `undefined reference` in `make coverage`. Its sibling `test_dp_rng` already
    linked `m`.
