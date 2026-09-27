- **`example-projects/uno-q` over NATS, with every frame accounted for.**
    `uno_q_pub` puts an RTL-SDR on doppler's wire as `ci8`, and
    `uno_q --nats` counts lost and repeated frames from the header's sequence,
    over pub/sub (the default) or JetStream push/pull. Gated as
    `make uno-q-nats-check`. On an Arduino UNO Q, live FM arrived with 0 of 733
    frames lost, at 46% of one core.
