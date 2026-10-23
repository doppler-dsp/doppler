- **`AsyncDsssPool` releases sidelobe ghosts.** A receiver that took a
    strong emitter's Doppler-offset sidelobe, hundreds of chips from it, is
    reset when its carrier tracks a stronger holder's a whole lattice step
    away (`chip_rate / (2 code_len)`) while its Dll rarely holds code lock,
    both over the last 0.2 s (doppler#2529). `released_ghost` counts them.
