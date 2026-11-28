- **A timing loop's control can no longer put two symbols in one input.**
    `RateSync` and `MpskReceiver` hold the steered rate of their converter
    at `rate + ctrl <= m`, the most that keeps one on-time strobe per
    input. Before, a control past `m` (an acquisition transient) put two
    strobes in one input and `step()`'s single symbol overwrote one. The
    bound is a new `RateConverter` field, `ctrl_rate_max`, off (`+INFINITY`)
    unless an owner sets it, so no other converter changes. Tracking never
    reaches it (#3289).
