- **`HalfbandDecimatorDp` is gone; use `HalfbandDecimator`.** It was a
    hand-written second binding of the same CF32 `dp_hbdecim` core, and
    not double precision despite its name. One behavior differs:
    `HalfbandDecimator` refuses float64 taps with `TypeError` where the
    old class narrowed them silently, so cast them with
    `.astype(np.float32)` (#3002).
