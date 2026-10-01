- **A refused scene says why, on every face** (#1696, #1706). A dsss
    source missing a code -- a burst with neither a preamble nor a frame, a
    continuous stream with no `data_code` -- and a continuous stream below
    one chip per data symbol (`fs / sps < symbol_rate`, the default
    `fs = 1.0` with a rate in Hz) are refused naming the fix. `Composer([...])`
    raises `ValueError(<the reason>)` for any refused scene, through the new
    `dp_wfm_compose_create_why`, instead of `dp_wfm_compose_create failed`.
    A preamble alone stays a valid burst, and `waveforms.md` says so.
