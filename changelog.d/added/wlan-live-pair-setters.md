- **A live `Synth`'s ofdm rate stays whole.** Setting `constellation` or
    `code_rate` beside an `auto` other fixes the other at its read; either
    set to `auto` returns both; a half beside `wlan`, and `wlan` beside a
    given pair, are refused. C: `dp_wfm_source_set_constellation`,
    `_set_code_rate`, `_set_wlan`
    ([#3287](https://github.com/doppler-dsp/doppler/issues/3287)).
