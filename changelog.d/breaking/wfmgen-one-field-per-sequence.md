- **wfmgen takes ONE Field per sequence field.** `--acq-code`, `--sync`,
    `--data-code` and `--bits` each take a Field: literal bits, `0x…` hex, or
    a generated `pn`/`gold`/`dotted` sequence, with `*REPS` on the preamble.
    `--X-hex`, `--X-gen`, `--acq-reps`, `--payload-gen` and `--payload-len`
    are refused, each naming its replacement, which produces the same samples.
    In a scene, `pattern`, `*_gen` and `acq_reps` give way to one Field string
    per key, and a carried frame's `lit`/`gen` to `spec` (#853).
