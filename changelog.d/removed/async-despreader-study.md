- **`async_despreader_study.py` is retired, and its BER table and figure
    with it.** It modelled a despreader the library does not ship (numpy
    partials, a boxcar and `SymbolSync`, a non-coherent combine).
    [The design](../../docs/design/async-dsss-receiver.md)'s §3.3 now
    describes the shipped chain, the carrier wipe → `Dll` → `RateConverter`
    → `MpskReceiver`, and cites the receiver's gated 5 dB Es/N0 floor test.
