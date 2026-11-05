- **A C test that `Corr` is `Corr2D` at one row, byte for byte**
    (`test_corr_equiv`, #2752 step 3.2). It drives both through the same
    calls and compares every return, every output buffer, the count and the
    state blob's payload, including a short `out`, a new reference and a
    reset mid-dwell. It is the evidence step 3.2 rests on to make `Corr` the
    one correlator's single-row flavor (its design, #2863, section 2.1).
