- **The `wfm_synth` state blob is version 4.** Its embedded `AWGN` blob is
    version 2 (seed and amplitude, #2084), so the synth blob changed size. A
    version 3 synth blob is refused by its version, not by its length.
