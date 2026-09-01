- **`reset()` no longer leaves the history ring behind the stream.** The ring's
    `head`/`tail` are monotonic ABSOLUTE counters, so emptying it by consuming
    everything available left them at the stream's last position while
    `samples_fed` restarted at 0 — every position computed afterwards was
    0-based against a ring that was not, so nothing was reachable, refine never
    ran, and `dp_f32_write` refused. Measured on `DsssBurstReceiver`, from
    which this code was moved: a second pass over the SAME capture returned
    nothing, with `dropped=67992`. `set_state()` already rewound for exactly
    this reason; `reset()` now agrees with it. The receiver's own copy is
    [#1169](https://github.com/doppler-dsp/doppler/issues/1169).
