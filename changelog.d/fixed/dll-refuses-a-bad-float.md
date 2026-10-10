- **`Dll` refuses a bad float instead of crashing.** A NaN `spacing` read
    out of bounds on the first sample (x86-64), a NaN or out-of-domain
    `bn`/`zeta` gave a loop that never recovered, and
    `set_symbol_period(nan)` aborted the process allocating 2^63. `Dll(...)`
    now raises `ValueError` for a non-finite seed phase, loop parameters
    outside the loop filter's domain, or a spacing outside (0, len(code)/2).
    `set_symbol_period` raises for a non-finite period or one past 2^20
    partials (#2103).
