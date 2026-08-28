- **One unparseable frame no longer ends a PULL work queue for good.** The
    JetStream tier acks explicitly, so a frame destroyed without an ack stayed
    pending, redelivered every `AckWait`, and held one of the consumer's 1000
    `MaxAckPending` slots — measured on a queue carrying an older wire version
    (`SIGS`/v1 vs today's `DPST`/v2): exactly **1000** failures, then
    `DP_ERR_TIMEOUT` permanently, with 997k frames stacked behind it. Such a
    frame is now `natsMsg_Term`'d — not acked, it was never processed — so the
    queue moves past it while `recv` still reports the error rather than
    silently skipping corruption.
