- **just-makeit pin 0.63.3 → 0.64.1, and eight bindings gain a real `out=`.**
    jm gh-1079 gives an all-scalar `variable_output` method the caller-owned
    output buffer the shape had been carved out of, plus the `<m>_max_out()` a
    caller sizes it with. `DDC`/`Ddcr`/`MatchedDDC`/`MatchedDdcr`
    `execute_ctrl_push`, `RateConverter`/`MatchedRateConverter`
    `execute_ctrl_push`, `Telemetry.read` and `MemoryCapture.records` now
    accept `out=` in the binding as well as the stub, and
    `DelayCf64.push_ptr` comes off the `kwarg-parity` ratchet because the two
    faces agree on their own.

    `objects/delay.toml`'s `push_ptr_max_out` `manual_stub` is retired — jm
    generates that symbol now. `farrow.delay_max_out` and
    `Resampler.execute_ctrl_max_out` are NOT retired and each carries a
    comment saying why: both are an array beside other params, which
    `_outbuf.why_not` excludes (gh-412), so jm emits no bound for them and the
    hand-written stub is still the only one. `farrow_delay_max_out`'s
    deliberate `0` return — the "caller sizes it" sentinel — is documented in
    the header rather than in a hand-written `.pyi` symbol.

    0.64.0 was adopted, found defective and **held**: it broke
    `Telemetry.read()` on an empty ring (the steady state of a non-blocking
    drain) by refusing a zero bound, and silently deleted `DelayCf64.write`
    from `delay.pyi` while leaving it in the binding. Both are fixed in 0.64.1
    ([just-makeit#1091](https://github.com/just-buildit/just-makeit/issues/1091),
    [#1092](https://github.com/just-buildit/just-makeit/issues/1092)), the
    second as a hard error rather than a silent drop.

    Also carried: every enum refusal now names its valid choices
    (`invalid type 'nope' (choices: tone, noise, pn, …)`), from jm gh-1026.
