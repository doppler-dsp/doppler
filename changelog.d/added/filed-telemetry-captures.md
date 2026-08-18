- **A validation report now files the TRAJECTORIES behind its numbers, not
    just the numbers** ([#846](https://github.com/doppler-dsp/doppler/issues/846)).
    `Report.capture()` attaches telemetry, captures the run, and writes the
    capture and its self-describing sidecar into the object's own `data/`
    folder beside the CSVs — committed like every other artifact there, so the
    scene behind an EVM or a lock time is re-openable without re-running
    anything.

    **`Report` owns it because it already owns the folder.** It is the single
    writer of `results.md`, the plots and `data/*.csv`; a per-validator
    convention would let the layout drift between objects the way the report
    format would without `_validation_common`. `write=False` still captures —
    every measurement runs, as the limits gate expects — and files nothing,
    because a test must never write into the repo.

    What is encapsulated is the ORDER, for the same reason `dp_ber_measure()`
    exists on the C side: probes must be registered BEFORE the capture opens,
    because the ring is sized from the probe table. Attach, then `arm`, then
    run — and getting it wrong raises rather than silently truncating.

    `scripts/plot_capture.py` is the one utility that reads them, replacing
    one script per subject: it takes a filed capture and an optional probe
    selection, resolves names against the capture's own table, and computes
    nothing. `--list` prints the probe table, because the capture describes
    itself. `MpskReceiver` is the first object to file one — 14 probes, 1792
    records, byte-stable across runs.
