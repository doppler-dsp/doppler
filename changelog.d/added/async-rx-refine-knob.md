- **`AsyncDsssReceiver` and `AsyncDsssPool` take `refine`** (in C, a new
    trailing create parameter: see the breaking entry). False hands the
    search's seed straight to tracking, at its own sample, with no refining
    state; it needs a search at least 1/bn code periods deep, bn the carrier
    loop's (about 101 / 135 at the 415.1-B-1 floor), and is refused below
    that. At the 415.1-B-1 point on a 400 Hz/s ramp its
    hand-over is within 13.6 Hz of the truth on 60 of 60 draws, and 52 of
    60 lock by 0.5 s against 42 of 60 as built. Default true, the
    receiver as built; the name is a placeholder.
