- **The docstring ratchet now fails per SYMBOL, not just per module.** A
    symbol recorded as documented may never stop being documented, and that
    fails on its own however much improved elsewhere. The per-module
    incomplete count stays, because it is what catches *new* undocumented
    surface. Adopting just-makeit 0.70.0, `doppler.track` read 1 → 1 while
    `BpskReceiver` gained a description and `MpskReceiverR` lost one — a
    count is a container that cannot represent a pair that cancels, so the
    module was silent over a real loss and only a hand-written diff found it.
    Proven by replaying that exact `.pyi`: the new rule names
    `MpskReceiverR` where the count reports nothing.
