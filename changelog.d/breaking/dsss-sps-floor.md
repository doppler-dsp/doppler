- **`DsssReceiver` refuses `sps < 2` instead of aborting the process.**
    `DsssReceiver(sps=1)` used to **SIGABRT the interpreter** — exit 134, no
    exception, no traceback, nothing on stderr. `sps` is a documented
    constructor parameter and `1` is not obviously out of range from the
    Python face, so a caller had no way to see it coming and no way to
    recover.

    The guard validated `sps < 1`, so `sps = 1` reached
    `mpsk_receiver_create()`, which rejects it (that constructor requires
    `sps >= m_out`, and the smallest legal `m_out` is 2). Its argument-error
    NULL then went through `dp_xnn()`, an **abort-on-OOM** helper — and an
    argument error is not an allocation failure.

    Two is not arbitrary: below it there is no receiver to build, which is
    the range `mpsk_rx_derive_m_out()` already documented. The guard and the
    guarantee simply did not meet. The object now also declares
    `create_error = "ValueError"`, so the refusal names the parameter set
    instead of surfacing as a blanket `MemoryError`.

    **Behaviour change:** `sps` of 0 or 1 now raises `ValueError` where it
    previously killed the process. Nothing in the tree passed either.
