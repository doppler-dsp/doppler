- **A stalled stream test says what the broker thinks.** When a `nats://`
    work-queue readiness probe times out, the failure now quotes the
    broker's own view of that queue — messages held, and each consumer's
    pending, ack-pending, waiting and redelivered counts — so a rare
    stall ([#1463](https://github.com/doppler-dsp/doppler/issues/1463))
    says whether the frame was never delivered, delivered and unacked, or
    gone. Test-side only: the broker's loopback monitoring port, read
    after a failure.
