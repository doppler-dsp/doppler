- **A pooled receiver with nothing to hold reads state 3, `"idle"`, not 0,
    `"searching"`.** The pool's controller owns the search, so a pooled
    receiver never searches: `PoolReceiverStatus.state` and the `.state`
    telemetry probe read 3 (`ASYNC_DSSS_RX_IDLE`, formerly reserved) while
    it waits for a seed. Code that took `state == 0` as "free" should test
    `state == 3`.
