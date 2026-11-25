- **A timing loop on the rectangular pulse sees the whole timing error.**
    The matched converter's rectangle bank sampled a hard-edged rectangle,
    so its arms were identical across one input interval and `RateSync`
    (the DSSS and M-PSK receivers' timing loop) could not steer inside it:
    no lock at all at 2 input samples per symbol, twice the predicted
    timing error at 4. The edge taps now carry the fraction, and the loop
    matches the linear model at 2, 4 and 8
    ([#3362](https://github.com/doppler-dsp/doppler/issues/3362)).
