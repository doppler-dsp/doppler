- **`AsyncDsssReceiver`'s carrier loop is sized by the loop-SNR rule**: its
    bandwidth is `C/N0 T / 200` at `cn0_dbhz`, no wider than the old 0.04
    (`dp_async_dsss_receiver_carrier_bn()`), and the pull-in bound and
    `refine`-false search depth follow it
    ([async-dsss-receiver §1.3](docs/design/async-dsss-receiver.md)).
