- **`Dll`, `Despreader` and the DSSS receivers refuse a bad argument instead
    of crashing.** A NaN `spacing` read out of bounds on the first sample, a
    NaN loop bandwidth gave a loop that never recovered, and
    `set_symbol_period(nan)` aborted the process allocating 2^63. They now
    raise `ValueError`, and `Dll.configure` refuses a bad `(bn, zeta)`. A
    1-chip code is refused, not aborted on. `set_symbol_period` refuses a
    period past 2^20 partials, and `AsyncDsssReceiver` refuses at create a
    symbol rate whose period would pass it; real links (1e3-6e4 baud) sit
    about five orders of magnitude below (#2103).
