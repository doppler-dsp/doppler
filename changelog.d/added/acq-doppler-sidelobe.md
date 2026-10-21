- **The acquisition engine reports its Doppler-offset sidelobe level and
    its gate.** `dp_acq_dsl_db()` is the code's ambiguity at the worst
    offset the search's tiles can put between an emitter and a tile,
    relative to the emitter's own peak: -16.95 dB for the SPEC Gold-1023
    search over +-5 kHz, where the zero-offset `dp_acq_psl_db()` reads
    -23.9 dB. It is computed on its first read, so an engine that never
    asks does not pay for the sweep. `dp_acq_gate_stat()` is the gate a
    dwell's `test_stat` must clear, on either detection path.
