- **The UBSan alignment ratchet is a gate now, and it had grown again.** The
    class was *excluded* with `-fno-sanitize=alignment` and the words "may only
    ever shrink" in a comment nothing read; it went 821 → 853 → **934**.
    `make test-ubsan` instruments it and ratchets the distinct source SITES
    per file — not the report count, which is how often the suite happened to
    execute one and moved 933→1058 between this machine and CI. Excluding was
    also strictly weaker: an exclusion cannot see a *new* misalignment at all,
    only reports outside the excluded class.
    [#1028](https://github.com/doppler-dsp/doppler/issues/1028).
