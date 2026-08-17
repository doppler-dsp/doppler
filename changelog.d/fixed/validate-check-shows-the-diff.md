- **`make validate-check` no longer discards the diagnosis it asks for.** It ran
    `validate.py --check > /dev/null`, so the one caller anyone actually uses
    threw away the output and printed a filename. `--check` now prints a unified
    diff when a report is stale, and the target captures and replays it (from
    `STALE:` onward, so the per-limit PASS lines stay out of the way).

    Measured consequence of not having it: wiring the target into CI for
    [#816](https://github.com/doppler-dsp/doppler/issues/816) reported four of
    eleven reports stale on the runner and the log said only *which files* — the
    half that cannot distinguish an edited validator, where `make validate` is
    the fix, from a machine difference, where re-running fixes nothing. Those
    have opposite responses, so a filename alone is the least useful thing to
    print.

    With the diff in hand the cause took one container run: the reports differ
    because the measured **error counts** differ (204 against 198 on one BPSK
    cell), which moves SER, implementation loss and EVM with them — all well
    inside the measurement's own ~7% standard error. So the reports print three
    and four significant figures where they carry about one, and byte-comparison
    is the wrong contract for the numeric half. Diagnosis, evidence and options:
    [#820](https://github.com/doppler-dsp/doppler/issues/820).
