- **`hbdecim_q15` no longer reads outside its ring on AVX2 builds with few
    taps.** The AVX2 kernel loads `a[N-16-k]` for every padded tap block, so a
    fold shorter than the padded tap count read before the window (native
    `-march=native` builds, `num_taps < 16`, for any head). The kernel now
    runs only when the fold covers the padded taps, and the scalar reference
    otherwise. The sum is identical, so the output does not change.
    a native clang ASan build (#2142).
