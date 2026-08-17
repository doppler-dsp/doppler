- **A retired identifier stays retired, and there is now a gate for it.**
    The `fec` → `ccsds_tm` rename went green with **eleven** occurrences of
    the retired prefix still in the tree: two macros in a public header (the
    ASM pattern and the inner code's constraint length, now `CCSDS_TM_ASM` and
    `CCSDS_TM_CONV_K`), the NAME five C tests print at the end of a run, an
    encoder type in a design page that had not existed since the codec was
    generalised to `conv_enc_t`, and three header references in a changelog
    entry and a validation report. A compiler cannot notice any of them —
    every one still compiled, because every one still existed.

    `scripts/check_retired_names.py` (wired as `make lint-retired-names` and a
    pre-commit hook) scans the hand-written trees for the patterns in
    `scripts/.retired-names`, a table whose rows are added by the commit that
    retires a name and which only ever hold at zero occurrences. Proven three
    ways by sabotage: the macro back in the header, the old name back in a
    test's printed label, and a stale symbol back in a design page.

    The patterns match the IDENTIFIER forms only, so FEC keeps its meaning as
    the general subject — `docs/design/fec-receive.md` spans a general Viterbi
    and a general Reed-Solomon, and that page's name is deliberate. One
    consequence worth stating, because it is the rule working rather than a
    gap: an entry like this one cannot spell the names it retires, so it names
    what replaced them instead.
