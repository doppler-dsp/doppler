- **Each carrier and code loop reports its NCO's command, and
    `AsyncDsssReceiver` its own symbol lock.** `Costas` and `Dll` add `.nco`
    (the whole filter output) beside the integrator's `.freq`/`.rate`; the
    receiver adds `.sym.lock`/`.sym.locked`, so a 4-slot `AsyncDsssPool`
    registers 121 probes and the 512-probe table holds 18 slots. The lock
    validator's `--telemetry DIR` captures one receiver through arrival,
    three fades and departure.
