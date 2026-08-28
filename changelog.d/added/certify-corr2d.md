- **`Corr2D` is certified**, and the certification found a silent failure mode
    — `dwell = 0` passed `create()` and built an object that accumulated every
    frame and emitted nothing until `count` wrapped at `SIZE_MAX`. Fixed at
    the primitive, so `CorrDetector2D` inherits the refusal rather than
    carrying a second copy of the rule. Two pins the object arrived with
    could not fail (a vacuous `reset`, an uncovered even-`n` Nyquist split),
    both found by breaking the code and watching the suite stay green.
    18 limits on every push; three claims are C-only and say so. Evidence:
    `src/doppler/spectral/tests/validation/corr2d/results.md`.
