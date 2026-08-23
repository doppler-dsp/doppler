- **An end-of-stream frame on the work queue is acked, so it does not
    outlive the run that sent it.** PULL is an explicit-ack consumer on a
    JetStream WorkQueue stream, and an EOS frame is the one message a caller
    can never ack: it is reported as a state (`DP_ERR_EOF` / `EOFError`) with
    no `dp_msg_t` handed back, so there is nothing to call `dp_msg_ack()` on.
    Nothing acked it, and the consequences compounded quietly:

    - it redelivered every `AckWait` (5 s) **forever**, so a long-lived
        worker pool saw one ending arrive repeatedly rather than once;
    - a work-queue message is only removed once acked, so it stayed in the
        stream permanently;
    - and therefore the **next** run against that subject opened onto the
        previous run's ending — a stream told it had finished before it had
        begun.

    The receive path now acks it itself, which is the only place that can.
    Every other role is unaffected: they have no ack to give.

    Missed originally because both the C test and the Python tests exercised
    the PUB/SUB tier, where there is no ack at all, and each ran against a
    freshly-named subject — the two conditions that hide this. The pin is
    `test_eos_is_acked_on_the_work_queue` in `test_stream_nats_core.c`, and
    it costs one `AckWait` to run: a redelivery is only scheduled when that
    timer expires, so nothing faster can tell an acked ending from an
    unacked one.

- **`dp_frame_parse` checks an EOS frame's `format`.** The header states
    that an ending carries no sample type, and the parser checked
    `payload_bytes` and `num_samples` but not that. Since end-of-stream is
    the one kind that skips the element-size arithmetic, it would otherwise
    have been the one kind whose `format` nothing ever looked at.

- **`F64Buffer.close()` and `I16Buffer.close()` documented themselves with
    an `F32Buffer` example.** Copy-paste, and invisible to the doctest gate
    precisely because `F32Buffer` exists and the example runs. `wait()` on
    all three now also documents the two ways it ends — `EOFError` and
    `KeyboardInterrupt` — which a caller has to handle and which its
    docstring still described as an unconditional block.
