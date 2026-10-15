- **The AWGN and LO sine tables and the CCSDS marker's bits are built once,
    safely, however many threads make the first object.** Each sat behind a
    plain `ready` flag, so two threads making their first AWGN or LO at once
    (a plan's first parallel `prepare`, any GIL-free caller) raced, and one
    could read a half-written table. All four lazy tables, rs.c's included,
    now go through one guard, `dp_once()` in `doppler/dp_once.h`, which
    `test_dp_once` checks from eight threads under `make test-tsan`
    ([#2328](https://github.com/doppler-dsp/doppler/issues/2328)).
