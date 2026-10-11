- **The `wfm_synth` state blob is version 4.** Its embedded `AWGN` blob is
    version 2 (seed and amplitude, #2084), so a noisy synth blob changed size.
    A version 3 synth blob is refused by its version: a clean one (no AWGN
    child) has the same length, so the VERSION check alone refuses it. A noisy
    v3 blob is refused by its length as well, since its AWGN child is shorter.
