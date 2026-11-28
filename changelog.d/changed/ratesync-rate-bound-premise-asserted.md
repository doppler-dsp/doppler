- **A Debug build asserts the steered-rate bound's premise.** RateSync and
    MpskReceiver bind their loop to a converter whose steered rate is held
    at `ratesync_rate_bound()`; the nominal terminal rate must sit under
    it, so only a loop transient meets the clamp. A Debug build now aborts
    at create on a geometry that breaks that, instead of tracking at the
    bound; every geometry either object accepts holds it (measured: at most
    0.71 of the bound)
    ([#3408](https://github.com/doppler-dsp/doppler/issues/3408)).
