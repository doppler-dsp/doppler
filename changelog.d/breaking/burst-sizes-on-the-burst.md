- **A burst engine sizes on the burst, not one dwell** (#1498). Its dwells
    are aligned to the stream, so a preamble of `reps` repetitions spans about
    `reps/D` of them. New `pd_burst` on `BurstAcquisition` and `BurstCapture`
    averages over that alignment and credits every dwell; the sizer meets
    `pd` with it and `underpowered` reads it. One dwell's Pd ranged 0.18–0.93
    across depths that all delivered about 0.6. **Breaking:** auto-sized
    depths change, usually shallower (reps=16 at 53 dB-Hz: 6 → 4), and a
    design point can newly read as under-powered. `pd_predicted` still means
    one aligned dwell; check `pd_burst` against `pd`.
