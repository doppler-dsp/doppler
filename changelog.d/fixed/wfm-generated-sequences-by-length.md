- **Generated sequences are no longer read as absent.** Checks that tested a
    sequence's array rather than its length refused `--data-code-gen` with
    `--symbol-rate`, let `--sync-gen` past the burst-frame refusal, refused a
    generated `bits` payload, and silently sent the PRBS default in place of a
    generated continuous-DSSS payload (#1592).
