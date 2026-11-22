- **`CarrierNda`'s `bn` and `MpskReceiver`'s `bn_carrier` are now the
    carrier loop's real noise bandwidth.** The discriminator's slope (2)
    went undivided, so both loops ran about 1.67 times wider than `bn`
    said, at a damping near 1.0. A loop at `bn = 0.01` is now 0.01. Narrower
    means less jitter and a smaller pull-in window: a carrier residual now
    locks promptly from inside `bn/(4M)` (it was `bn/M`), so a coarse
    acquisition in front needs a bin a quarter as wide, or a wider `bn`
    (#3224).
