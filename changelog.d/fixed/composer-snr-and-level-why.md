- **A scene that gives a source both `snr` and `level` beside a noise floor
    now says so.** The Composer refused it with only
    `dp_wfm_compose_create_why failed`; the reason now names the rule (the
    floor is shared, so a source sets its `snr` or its `level`, not both) on
    every face. See [#2834](https://github.com/doppler-dsp/doppler/issues/2834).
