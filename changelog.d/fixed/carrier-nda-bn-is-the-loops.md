- **`CarrierNda`'s `bn` and `MpskReceiver`'s `bn_carrier` were the loop
    filter's design value, not the carrier loop's bandwidth.** The
    discriminator's slope (2) went undivided, so both loops ran about 1.67
    times wider than `bn` said. `bn` is now the loop's noise bandwidth, so
    the same setting runs narrower, with less jitter and a smaller pull-in
    window. A residual now locks promptly from inside `bn/(4M)` (it was
    `bn/M`); size a coarse acquisition's bin to a quarter of what it was,
    or widen `bn` (#3224).
