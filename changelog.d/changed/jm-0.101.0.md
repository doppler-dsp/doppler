- **just-makeit pin 0.100.1 → 0.101.0.** It carries gh-2059: a numpy
    `Parameters`/`Examples` section in an `[[<obj>.extra_methods]]` row's
    `doc` no longer fails `jm status --check`, which is what held the last
    hand-owned bindings (#1886). `jm apply` re-rendered two files: the
    example in `help(wfm.Writer)` now imports from `doppler.wfm` instead
    of `doppler.wfm_writer`, which does not exist, and the root
    `CMakeLists.txt` reorders its `target_sources` lines (the same 250).
