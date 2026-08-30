- **The UBSan alignment ratchet is a gate now, and it had grown again.** The
    class was *excluded* with `-fno-sanitize=alignment` and the words "may only
    ever shrink" in a comment nothing read; it went 821 → 853 → **934**.
    `make test-ubsan` instruments it and counts: any non-alignment report
    fails, and the alignment count is held to a committed ceiling. Excluding
    was also strictly weaker — an exclusion cannot see a *new* misalignment at
    all, only reports outside the excluded class.
    [#1028](https://github.com/doppler-dsp/doppler/issues/1028).
