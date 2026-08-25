- **`Corr2D` is certified**, and the certification found a silent failure
    mode in it — `src/doppler/spectral/tests/validation/corr2d/results.md`,
    18 limits asserted on every push, plus five C sections the suite was
    missing.

    **`dwell = 0` built an object that swallowed everything and emitted
    nothing.** The header documents `dwell` as *"must be >= 1"*;
    `corr2d_create` enforced only its output-grid rule. The dump test is
    `++count == dwell`, which a zero dwell never satisfies, so such an
    object accumulated every frame it was handed and returned nothing until
    `count` wrapped at `SIZE_MAX` — a caller whose dwell came from a
    computed value that underflowed got silence and unbounded growth rather
    than a refusal. Fixed at the primitive, not per caller:
    `detector2d_create` forwards its own `dwell` straight into that call, so
    `CorrDetector2D` inherits the refusal and no second copy of the rule can
    drift. Both objects now pin it, and both pins are sabotage-proven.

    **Two of the pins the object arrived with could not fail.** Both were
    found by breaking the code and watching the suite stay green, which is
    the only way that class of gap surfaces:

    - **`corr2d_reset` was vacuous** — the C test called it on a freshly
        created object and asserted `count == 0`, which was already true
        before the call. A `reset()` with an empty body passed it, and the
        header's other promise, that it zeroes the accumulator, was asserted
        by nothing. Now measured the way the header states it: a partial
        dwell, a reset, then the same full cycle as an untouched object,
        requiring bit-identical dumps. Dropping the `memset` takes the new
        check red and leaves the old one **green**.
    - **The even-`n` Nyquist-bin split was covered by nothing** — including
        by the new sub-bin interpolation section, because the split changes
        the interpolated shape and barely moves its argmax. The invariant
        that sees it needs no reference implementation: a real input against
        a real reference gives a real correlation, and the band-limited
        interpolation of a real sequence is real. An unsplit Nyquist bin
        turns `cos(pi t)` into `exp(i pi t)` — identical on the sample grid,
        complex between. 5.5e-08 with the split, 6.3e-03 without.

    **Sub-bin interpolation was pinned only at integer shifts**, where the
    native grid already lands on the answer and interpolation proves
    nothing. Now swept at quarter-bin offsets on both parities of `nx`: the
    interpolated peak lands within 0.001 of a bin where the native grid is
    off by 0.5.

    Worth recording how that test failed first, because it reads exactly
    like an implementation defect: built with the phase ramp swept over
    `u = 0..nx-1`, the fine grid shows **two equal peaks straddling a null
    at the true position**. That sweep treats the upper bins as high
    *positive* frequencies rather than the negative ones they are, and no
    correlation surface has such a spectrum. With the frequencies signed the
    peak lands correctly everywhere — the correlator was right and the
    stimulus was wrong.

    **Three claims are C-ONLY and say so**: `corr2d_set_ref` has no binding
    at all, including its contract that a fast-path object *refuses* a
    reference that is no longer single-row; the `fast_path` selection flag;
    and the native path's promise to allocate no interpolation scratch,
    which is observable only as NULL pointers. A Python-only audit would
    have reported a clean bill of health for the surface that matters least.

    `nthreads` is the one claim that **cannot** be sabotaged, and the test
    says so rather than implying proof: `corr2d_state_t` has no such member,
    `create()` forwards it to `fft_create`/`fft2d_create`, and both open
    with `(void)nthreads;`. The bit-identical-output check is a forward
    guard for the day someone wires it to a threaded reduction whose
    summation order differs.
