- **`doppler.correlation.correlator_same_code_phase`, the one
    same-signal key.** `dp_correlator_same_code_phase()` says whether two
    code phases are one signal: within `DP_CORRELATOR_SAME_PHASE_CHIPS` (one
    chip), circularly. The receivers pick and the pools duplicate test call
    it, and now so can a caller
    ([#3401](https://github.com/doppler-dsp/doppler/issues/3401)).
