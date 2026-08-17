- **Reed-Solomon symbol interleaving** (131.0-B-3 sections 4.3.5 and 4.4.1),
    depths `I = 1, 2, 3, 4, 5, 8`, plus `fec_rs_codeword_ok` — a syndrome check
    that says whether 255 symbols form a codeword without decoding them. The
    interleaver is what makes the outer code burst-tolerant: a contiguous burst
    of `B` symbols lands as `ceil(B/I)` errors per codeword, so depth buys a
    `I`-fold longer correctable burst at no cost in rate. Tested in both
    directions — a 40-symbol burst stays inside `E=16` at depth 5 and exceeds
    it at depth 1 — because an interleaver that merely copied would pass the
    first half alone.
