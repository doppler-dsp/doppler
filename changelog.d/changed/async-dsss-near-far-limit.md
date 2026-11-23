- **`AsyncDsssReceiver` and `AsyncDsssPool` declare their near-far limit.**
    In practice, an emitter within 20 dB of the strongest one in range is
    handed over, held and decoded. The exact figures:

    - tracking tolerates 24.2 / 23.9 dB on a quarter of the code's lags and
        60.2 dB on the rest;
    - the fine hand-over tolerates 10 log10(N / K) on every lag (22.9 dB at
        the 415.1-B-1 point).

    A ghost on its parent's carrier is not separated from a co-located
    weak emitter today (doppler#3263)
    ([async-dsss-receiver §8.2](docs/design/async-dsss-receiver.md)).
