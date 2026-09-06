- **The async-DSSS receiver's status reports the whole carrier estimate,
    and the pool's table keys on locked loops only.** A hand-over past
    loop 1's pull-in leaves the receiver code-locked with the carrier
    unlocked and loop 1 free-running 800 Hz in a second; a row keyed on
    that would let the searcher's next hit on the same emitter look new
    at the pool's 31.7 Hz rows. `status().doppler_hz` is now loop 1 plus
    loop 2's integrator; a row refreshes its Doppler under `locked` and
    its chip phase under `code_locked`, holding the last locked value
    otherwise (design §12.13).
    [#1261](https://github.com/doppler-dsp/doppler/issues/1261).
