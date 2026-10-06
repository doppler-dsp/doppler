- **`FrameMeter` is jm's, and `fer()` and `sync_miss()` refuse an argument**
    (#1446). Its fragment is re-rendered by jm and leaves the `-Wall -Wextra`
    exempt list, now 34 files. The two rates are `METH_NOARGS`: `meter.fer(1)`
    used to return the rate and now raises `TypeError`. `FrameMeter` also
    reports `doppler.ber` as its module.
