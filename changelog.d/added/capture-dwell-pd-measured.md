- **A burst capture's Pd is measured against its coherent depth** (#1470).
    `native/validation/capture_dwell_pd.c` shows `pd_predicted` is pessimistic
    at small D, where a burst offers several dwells, and optimistic past
    D = (R + 1)/2, where a dwell can straddle the preamble's edge: 0.26
    delivered against 0.61 predicted at D = R = 8. Each burst lands at a
    continuous delay; a whole-sample delay read up to 0.23 high. The model
    fix is [#1498](https://github.com/doppler-dsp/doppler/issues/1498).
