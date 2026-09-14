- **`dropped` on a ring buffer documented as what it is: samples in REFUSED
    writes, not samples lost.** `write` is all-or-nothing — with no room it
    copies nothing and leaves the caller's array untouched — so a producer
    that spins on it until it succeeds inflates the counter while losing
    nothing (measured: 5,960,438 over a 60,000-sample run). The header, the
    binding, the stubs and both guide pages said "dropped due to buffer
    overrun"; all four now say the call was refused and the data is still
    yours to retry.
