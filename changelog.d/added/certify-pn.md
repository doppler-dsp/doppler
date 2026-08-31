- **`PN` is certified** — the 31st object: 16 limits, 4 findings, none open.
    The subject is the table. `pn_mls_poly()` declares a primitive polynomial
    for every width 2..64 and the header calls them verified, but across both
    suites that was pinned at **six of sixty-three** — a wrong entry is not a
    crash but a short-period spreading code. All 63 now checked in both
    realizations, by an order test whose transition matrix is probed out of
    the shipped library and cross-checked against brute-force stepping.
    Also pinned: the two-valued autocorrelation, shift-and-add closure, and
    `fib[i] == gal[(P-i) % P]`. [Evidence][pn-cert].

[pn-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/pn/results.md
