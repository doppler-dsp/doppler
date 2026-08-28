- **Three Doxygen defects, all comments that said something other than what
    they meant.** An orphaned `/** */` block left behind when the FFT-bin fold
    moved to `clib_common.h` bound itself to the *next* declaration, so
    `acq_build_handoff()` rendered with two `@param`s it does not take and a
    `@return` while returning `void`. `objects/*.toml` inside a block comment
    contains the literal `/` `*` and opens a nested comment, reported 940 lines
    below its cause. And `DOT_GRAPH_MAX_NODES` was a count of our own modules,
    so it is crossed by growth rather than by a defect — already raised
    50 → 100 once, crossed again at 103, now Doxygen's maximum. Fixed in
    `Doxyfile.base` upstream too.
