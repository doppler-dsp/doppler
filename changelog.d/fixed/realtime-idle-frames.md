- **`wfmgen --realtime` sends idle frames again while stdin pauses.**
    `dp_wfm_compose_set_data_pacing` now also paces the synths `create`
    already built, so a paced stream with nothing yet sends a fill frame
    instead of stalling the output
    ([#1782](https://github.com/doppler-dsp/doppler/issues/1782)).
