- **`DsssBurstReceiver.pending` reports detections being held, instead of
    three contradictory things.** The manifest called it "bursts demodulated
    and waiting", the header said "Always 0", and the code assigned it the
    queue length on one path only — so a caller ending a capture mid-burst
    saw `pending=0 dropped=0 n_bursts=0`, identical to an empty capture,
    while the receiver held a burst that would have decoded. There is now one
    live field and the three agree. Holding is correct and unchanged: a burst
    split across two `push()` calls comes out of the second one bit-exact,
    now pinned by `test_a_burst_split_across_two_pushes_survives` — the
    contract nothing had tested.
