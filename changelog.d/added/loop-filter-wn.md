- **`loop_filter_wn(bn, zeta)` is public** — the natural frequency
    `8*zeta*bn / (4*zeta^2 + 1)`, which at `zeta = 0.707` is `1.8857*bn`.
    Every closed form about a second-order loop is written in `wn`, and the
    one that matters for measurement is the steady-state phase lag under a
    frequency **ramp**, `2*pi*r / wn^2` — the disturbance a type-2 loop does
    *not* null, and therefore the only one against which a gain can be
    checked. A frequency step is nulled regardless of gain and cannot.

    Extracted from `loop_filter_init()` and **deliberately unguarded**, so the
    behaviour of that function changes by exactly nothing — including the
    non-finite case its own docstring documents. `loop_filter_create()` is the
    boundary that rejects the domain, and the test pins it doing so.

    `native/validation/rx_nda_tap.c`'s private `rx_nda_wn` is gone.

    **Three of the five "copies" turned out not to be copies**, which is worth
    recording because the grep that found them looked convincing.
    `loop_filter_bandwidth_demo.py` and `validate.py` share only the
    `(4*zeta^2 + 1)` denominator — their `excess_law` is
    `16*zeta^2 / (4*zeta^2 + 1)^2`, a different closed form. And
    `test_loop_filter_core.c`'s `theta = 4*zeta*bn*t / (4*zeta^2 + 1)` is
    `wn*t/2` written out **independently on purpose**: its own comment says
    re-typing the implementation's formula beside it "would prove only that
    the file had been copied correctly". Repointing either would have deleted
    the independence that makes them evidence.

    The new test exploits that instead: it ties `loop_filter_wn()` to the
    test's existing independent derivation (`wn*t/2 == theta`) rather than to
    a copy of its own expression, and pins the quoted `1.8857*bn`, linearity
    in `bn`, and that the gains `loop_filter_init()` produces still match.
    Sabotage: `8.0 -> 8.1` takes six assertions red.
