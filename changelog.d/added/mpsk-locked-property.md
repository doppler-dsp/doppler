- **`MpskReceiver.locked` — the binary carrier-lock indicator reaches Python.**
    `mpsk_receiver_get_locked()` has always existed in C, and `lock_time`'s own
    docstring referred to "polling `locked` in a loop", but the property was
    never declared, so the Python face had only the raw `lock` metric. Adding
    it also gives the Python tests the observable that replaces `tracking`.

    Its docstring states what the C header states and the API reference now
    repeats: it is an **indicator and nothing else**. It steers no loop and
    gates no output, so a wrong reading costs a caller their measurement window
    and costs the demodulator nothing. Its statistic's H1 mean is a function of
    Es/N0 alone, so the instant it declares carries no information about how
    converged the carrier estimate is — `lock_time` plus a settling budget is
    the question that asks about convergence.
