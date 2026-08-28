- **The wfmgen pages stopped contradicting each other.** Established by running
    the code, not by picking a side: `--snr-mode auto` sends `bits` to `fs`,
    not Es/N0 as the schema claimed (`--type bits` renders byte-identically
    under `auto` and `fs`); RRC is `2 * span * sps + 1` taps, not
    `span * sps + 1`; there are **nine** `--type`s, not eight; and `Plan` now
    accepts multi-segment, `repeats`, ranged *timing* and bundled noise —
    three of the five limits `plan.md` still advertised.
