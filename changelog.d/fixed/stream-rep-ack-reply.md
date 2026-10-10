- **Acking a request no longer sends the requester `+ACK`.** `dp_msg_ack()`
    on a REP request ran a JetStream ack on a plain subscription, which
    published `+ACK` to the requester's inbox, so a replier that acked
    unconditionally gave its requester `+ACK` as the first reply instead
    of its own. An ack on REP and SUB messages is now the no-op `stream.h`
    documents; on SUB it had returned an error (#2016).
