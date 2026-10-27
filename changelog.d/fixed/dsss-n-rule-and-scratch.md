- **A DSSS receiver's own grid re-applies, and tracking allocates nothing
    once warm.** `DsssReceiver` and `AsyncDsssReceiver` `configure_chain_raw`
    now accept any `n` MpskReceiver accepts (even, 2..8, at most `sps`;
    `mpsk_rx_m_out_ok`, beside the derivation `create` uses), so the `n` that
    `create` picks at `sps` 3, 5 or 10 is no longer refused. `DsssReceiver`
    tracking and `Ddcr.execute`/`execute_ctrl` keep their scratch in the state
    instead of allocating it on every call (#2650).
