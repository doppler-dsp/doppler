- **The correlator's two search flavors, and a C test that they are acq,
    byte for byte** (`test_search_equiv`, #2752 step 3.4b).
    `dp_correlator_create_continuous` and `dp_correlator_create_burst` are
    the acquisition engine as the correlator's sixth and seventh flavors,
    served by its one `push`, `reset` and state triplet; the hit record
    widens to acq's 64 bytes. The test drives both through 14 scenes (both
    doors, a peak list, rate hypotheses, the carrier, a tiled search, four
    threads, odd chunks, a mid-stream restore) and compares every hit and
    every dwell's surface under strict floating point. It is the evidence
    the step's move rests on.
