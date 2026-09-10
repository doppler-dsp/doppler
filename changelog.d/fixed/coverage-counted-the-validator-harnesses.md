- **Coverage counted the validator harnesses as library code.** `COV_IGNORE`
    has always dropped `native/tests/` and `native/benchmarks/` — the recipe's
    own rule is that test sources stay out of the report and only what they
    exercise in library headers is added — but it never named
    `native/validation/`. So 9310 lines of `validate_*` harness, about a
    quarter of the reported denominator, were scored as first-party source at
    78.97%. Found while measuring what excluding those validators from the
    instrumented run would cost: the headline drop looked like 9.4 points and
    was 0.03, the rest being the harnesses reporting on themselves. The
    reported total moves from 84.36% to **86.05%**, which is the number for
    doppler's own code.
    [#1292](https://github.com/doppler-dsp/doppler/issues/1292).
