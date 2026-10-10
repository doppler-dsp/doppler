- **The stream receiver examples ride out an end-of-stream and a one-off
    receive failure.** `receiver.py` caught only Ctrl+C, so the
    `EOFError` of a graceful publisher restart, or the `RuntimeError` of
    the broker's slow-consumer signal, ended the dashboard. Both receivers
    now print the end-of-stream and keep receiving, skip a failed receive
    (the lost frames count as the next gap), and stop after three
    failures with no frame between. `streaming.md`'s "Dropped" causes lose
    "slow joiner", which a first-frame anchor cannot count (#2096).
