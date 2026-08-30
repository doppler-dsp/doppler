- **`refine_span` is start-to-start separation, not dead air.** Both sides of
    the coalescing test are burst STARTS, so the two readings differ by a whole
    burst — and reading it as the gap between bursts made a caller reserve ~9%
    airtime for a constraint that was never there. The gap actually required is
    `max(0, refine_span - burst_len)`, which is 0 for any realistic payload.
    Behaviour is unchanged and was already correct; the docstring was not, on
    all three faces. Reported with a reproducer in
    [#1085](https://github.com/doppler-dsp/doppler/issues/1085).
