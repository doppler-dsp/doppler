- **`ddc_fn_scaling.py` read one point of its scaling curve and called an
    ordinary cloud VM a GIL regression
    ([#990](https://github.com/doppler-dsp/doppler/issues/990)).** The demo's
    whole claim is that `Ddcr.execute` releases the GIL, and it tested that
    with `assert su2 > 1.25` — the 2-thread speedup alone. A CI runner
    measured 1.02x at 2 threads and **1.90x at 4**, which is not a held GIL:
    a held GIL caps every count near 1x.

    Both points imply the same per-thread rate — 0.510 and 0.475 of the
    single-thread rate — and a machine whose single-thread boost clock is
    about twice its all-core clock produces exactly that. On such a box
    `su2 ≈ 1.0` is the CORRECT result for a fully GIL-free kernel, so the
    assertion's premise was wrong rather than merely fragile.

    The first guess, filed with the issue, was transient runner contention.
    That did not survive measurement: 24 local runs across four topologies and
    load regimes — four physical cores, two physical cores with their SMT
    siblings, constant competing load, and load applied only during the
    2-thread window — never moved `su2` below **1.92x**. It is a stable
    quantity on a healthy machine, and nothing available locally reproduced
    1.02x.

    So the assertion now reads the CURVE: the best speedup over every measured
    thread count. Frequency scaling flattens a curve without stopping it
    climbing; a held GIL stops it climbing everywhere. Proven by sabotage —
    swapping the kernel for a pure-Python loop scores 1.00x at 1, 2 and 4
    threads and fails the new assertion, while `execute` scores 4.23x. All
    four cases are exercised through the real `main()` with a stubbed
    `_throughput`: CI's actual failing curve now passes, a genuinely held GIL
    fails, a healthy 20-core box passes.

    Below four usable cores the assertion is **skipped with a printed
    reason**, matching the existing `LLVM_PROFILE_FILE` guard: with one or two
    points the frequency effect and a held GIL are indistinguishable at any
    threshold, and the honest move is to say the measurement cannot support
    the claim rather than pick a number. The plot is still produced.

    `.examples-serial` keeps its entry — xdist starves every thread count at
    once, so reading the best point does not help there and the serial pass is
    still what makes that case sound.
