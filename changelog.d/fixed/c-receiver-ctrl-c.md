- **The C receiver example ignored Ctrl+C once the transmitter stopped.**
    `native/examples/receiver.c` never set a receive timeout, so `dp_sub_recv`
    parked inside the NATS client — for an hour at a time — and the SIGINT
    handler's `keep_running` flag was never re-read. With traffic flowing
    the bug is invisible: every packet returns control to the loop and the
    interrupt is seen at once. The moment the sender stops, Ctrl+C stops
    working. Measured before and after: 0.00 s to exit with the transmitter
    running, indefinite with it stopped, 0.26 s with the fix.

    `spectrum_analyzer.c` had the same shape and is fixed the same way — a
    bounded `dp_sub_set_timeout` plus treating `DP_ERR_TIMEOUT` as "loop
    round and re-read the flag", which is what the Python receiver was
    already doing by passing `timeout_ms=500`.

    Nothing ran these: `native/examples/.examples-skip` excuses the dashboards
    from the smoke gate because they never self-terminate, and the reason
    it gives — "runs until Ctrl+C" — quietly assumed the interrupt worked.
    `src/doppler/tests/test_c_example_pairs.py` now runs receiver and
    transmitter as a pair, stops the sender FIRST, and requires the
    receiver to exit on SIGINT within five seconds. Interrupting a *busy*
    receiver passes against the defect, so the stopped-sender ordering is
    the whole test. Sabotage-checked: removing the timeout turns it red.

    While placing that gate: CI ran `make nats-down` **before** the
    examples step, so every broker-dependent example — the conditional
    `broker:` registry entries, and any two-process pair — self-skipped
    there. The step order is swapped, which is the difference between that
    gate running in CI and looking like it does.
