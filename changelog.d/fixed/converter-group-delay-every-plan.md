- **`dp_RateConverter_get_group_delay()` answers for every plan.** It
    returned NaN for any plan with a halfband or a CIC, and for a matched
    plan of one stage it returned the plain resampler's figure, which is
    not the matched bank's (50.0 input samples where a tone measures 41.0
    at rate 0.8 on a root-raised cosine). It is now the sum of each
    stage's own delay, with the matched bank's read from its taps, and
    `dp_hbdecim_get_group_delay()` and `dp_cic_get_group_delay()` are the
    stages' own
    ([#3119](https://github.com/doppler-dsp/doppler/issues/3119)).
