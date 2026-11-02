- **`AsyncDsssPool`'s search runs on the pool's thread count again,** capped
    at one worker a Doppler tile. On bare metal a ±50 kHz dwell takes 0.35 to
    0.58 of its serial time at 2–8 threads. On WSL2 the scene example's
    one-epoch pushes run about 1.8x slower, and an all-cores pool 1.27x;
    #2775 owns that (#2773).
