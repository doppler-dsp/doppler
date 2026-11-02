- **`AsyncDsssPool`'s search runs on the pool's thread count again,** capped
    at one worker a Doppler tile. Its once-an-epoch stages run inline and its
    once-a-block ones fan when a block is more than one epoch, so a ±50 kHz
    dwell takes 0.47 to 0.68 of its serial time at 2 threads to all cores on
    both bare metal and WSL2, and one-epoch pushes tie serial. A standalone
    `Acquisition` keeps fanning every stage (#2773, #2775).
