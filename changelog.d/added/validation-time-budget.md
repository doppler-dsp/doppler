- **A validator spot check's cost is now recorded and ratcheted**
    ([#1328](https://github.com/doppler-dsp/doppler/issues/1328)). The
    spot-check/full-sweep split had no gate, so three validators landed in six
    days at 87.0 s, 58.7 s and 53.5 s — the largest longer than the full sweep
    that motivated the split. An unrecorded, outgrown or stale entry now fails
    `make test-sweep`. Budget is in CI seconds and enforced only there: a
    workstation runs the same 34 checks **2.35x** faster.
