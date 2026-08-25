- **`dp_fftfreq()` and `dp_fftfreq_index()`** — the FFT bin→frequency mapping,
    named for what it is and living in `clib_common.h` where everything can
    inline it. `doppler.dsss.bin_to_signed` is a thin wrapper over the same
    inline, so C and Python read an FFT grid with one implementation instead
    of several.

    **It arrived as `acq_bin_to_signed`, an acquisition-private helper**, and
    that framing is how it came to disagree with the rest of the world. It is
    `numpy.fft.fftfreq(n) * n` — identical for odd `n` — except at one index:
    an even grid's Nyquist bin, which it reported as `+n/2` where numpy (and
    every other FFT library) reports `-n/2`.

    Not wrong on its own: `+n/2` and `-n/2` are the same frequency and a
    search on that grid cannot separate them. But it meant **every formula
    ported in from numpy disagreed with the engine at exactly the bin the
    engine was most careful about** — its own header carries a warning about
    a past full-span sign inversion at that index, which surfaced as a
    receiver reporting `tracking == 1` while decoding noise. It now follows
    the universal convention, which deletes that class of surprise instead of
    documenting it. Changing it broke no existing test, which is a fair
    measure of how untouched that index was.

    **Four Python call sites had restated the fold, in three mutually
    inconsistent ways** — one of them spelled verbatim as
    `((bin + n/2) % n) - n/2`, the form the header names as the historical
    bug. All four now call `bin_to_signed`.

    **`dp_fftfreq()` takes the sample RATE where numpy takes the sample
    spacing.** That is the one deliberate difference from the numpy
    signature and it is the right way round here: every caller has `fs` in
    hand and would otherwise write `1.0 / fs` at the call site — a
    reciprocal to get wrong for no benefit. Pass `fs = 1.0` for normalised
    cycles/sample, numpy's default.

    The C test states the contract as numpy's own output written out, rather
    than re-deriving it from the implementation under test, and is
    sabotage-proven by restoring the `+n/2` reading. The benchmark answers
    the question the two-form design actually raises: the wrapper cannot be
    inlined across its translation unit and costs **3.3x** the inline (0.65
    against 0.20 ns/call), so hot C paths should stay on `dp_fftfreq_index`
    and everything else may use either.

    One upstream limit found on the way, and worked around rather than
    accepted: `jm apply` cannot round-trip a `[[module.X.functions]]` `doc`
    containing a newline — it re-serialises the string into a single-quoted
    TOML scalar, which is illegal, and fails on its own output
    ([just-makeit#1153](https://github.com/just-buildit/just-makeit/issues/1153)).
    That caps a manifest-documented module function at one paragraph, so it
    can never carry an `Examples` block and can never satisfy the
    docstring-coverage ratchet. The prose and the doctest therefore live in
    `dsss_core.h` above the injected declaration, which jm transplants into
    the stub and preserves across `apply`.
