- **`wait(n)` on a ring buffer no longer hangs when `n` exceeds the ring's
    capacity.** It could never be satisfied — the ring holds at most
    `capacity`, so no producer can supply `n` — and the spin loop, which has
    exits for end-of-stream and for an interrupt, had none for this: it span
    forever at 100% CPU with no diagnostic
    ([#1335](https://github.com/doppler-dsp/doppler/issues/1335)). It now
    raises `ValueError` naming both `n` and `capacity`, which matters because
    `capacity` is rounded **up** from the constructor argument.
