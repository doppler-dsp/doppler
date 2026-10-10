- **`dp_req_send`/`dp_rep_send` refuse a size above `INT_MAX` with
    `DP_ERR_TOO_LARGE`** instead of narrowing it to nats.c's `int` length
    and returning `DP_OK`: 2^32 + 5 bytes sent 5, and 2^31 sent an empty
    message (#2069). C only; the Python `Requester`/`Replier` send framed,
    already-bounded messages.
