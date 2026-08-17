- **`mpsk.mpsk_soft_demap` — per-bit log-likelihood ratios from the M-PSK
    constellation.** The module docstring has promised *"hard and **soft**"*
    since the module shipped, and there was no soft anything in it. There is
    now, and the caller it was built for is the CCSDS inner decoder: a
    rate-1/2 K=7 Viterbi fed hard bits gives up roughly 2 dB of the coding
    gain it exists to deliver.

    The convention, because every consumer has to agree with it:
    `L_i = log( P(bit i = 0) / P(bit i = 1) )`, so **positive means bit 0**
    and the hard decision is `L < 0`. That is not a second decision rule — it
    is the same one seen differently, and *"the LLR's sign reproduces
    `mpsk_demap`'s label"* is asserted at every M across Es/N0 from −3 dB
    (where the decision is nearly a coin toss) to +20 dB. Sabotage: flipping
    the sign reddens it at every point.

    **One general path, no per-M fast paths.** BPSK and QPSK have exact
    closed forms — `4·Re{y}/N0`, and `4·(1/√2)·{Re,Im}{y}/N0` for QPSK, whose
    `phi0 = pi/4` grid is axis-separable — but shipping them beside the
    general path would be two implementations of one primitive, which is the
    thing that drifts. They are worth more as **test assertions**: they prove
    the general path right, to 2e-4, and pin the grid that makes the
    separability true rather than coincidental.

    Also pinned, each sabotage-proven: exact linearity in `1/N0` (so a caller
    without an SNR estimate may pass 1.0 and rescale, and a Viterbi may ignore
    it entirely); that the origin — equidistant from every point — reads
    exactly zero on every bit, and that confidence grows strictly along the
    ray to a constellation point, which is what a demapper returning the hard
    decision as ±1 would fail; and four refusals (short buffer, unsupported M,
    zero and negative `N0`) verified against a poisoned buffer, since silence
    is this function's whole contract.

    `llr` is a caller-provided out-param rather than a returned array because
    the output expands by `log2(M)` and jm sizes a function's output array 1:1
    with its input — `kaiser_window` is the existing precedent for that shape.
    What max-log costs at 8PSK **in dB** is deliberately not claimed: it is an
    Eb/N0 offset on a decoded BER curve and cannot be measured until the
    decoder exists (`docs/design/mpsk-soft.md` §5).
