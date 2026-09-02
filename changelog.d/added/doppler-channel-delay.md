- **`DopplerChannel.delay_samples`, `Resampler.delay`.** The resampler's
    group delay, closed-form from the bank (10.5 samples for the built-in
    bank): output `k` carries the input at `t + excess(t) - delay/fs`. A
    loop started at the input's phase through the channel was five chips
    from the peak and locked on a Gold sidelobe with nothing to say so
    ([#1189](https://github.com/doppler-dsp/doppler/issues/1189)); the
    resampler test's group-delay pin now holds the accessor instead of a
    literal, on both entry points.
