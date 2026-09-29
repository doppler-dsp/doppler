- **wfmgen's numeric flags read the whole value or refuse it.** They used to
    stop at the first character that was not a digit, exit 0: `--seed 0x10`
    recorded 0, `--pn-poly 0x6000` selected auto and `--sps 4x` recorded 4.
    Now a bad value exits 2 with a sentence naming the flag. Integers take
    decimal or `0x` hex, read by the Field grammar's own reader, so a leading
    0 is decimal and never octal (#1611).
