- **`AsyncDsssReceiver`'s carrier loop is sized by the loop-SNR rule.**
    Its bandwidth was a fixed 0.04. It is now `C/N0 T / 200` at the
    receiver's `cn0_dbhz`, 20 dB of loop SNR there, and no wider than 0.04:
    0.0120 at 2.2 GHz and 0.0094 at 2.8 GHz at the 415.1-B-1 floor.
    `dp_async_dsss_receiver_carrier_bn()` gives it. A receiver at a design
    C/N0 the rule allows wider keeps 0.04. The pull-in bound
    (`ASYNC_DSSS_RX_CARRIER_PULLIN_HZ`) and the least search depth with
    `refine` false (`ASYNC_DSSS_RX_NO_REFINE_MIN_BINS`) now take the
    bandwidth as an argument. At the floor that bound narrows to 37.7 Hz,
    and a `refine` false receiver needs about 1 / bn code periods of search.
