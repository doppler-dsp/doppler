- **`docs/design/mpsk-soft.md` — the design for LLR output from the M-PSK
    constellation.** Planned, not built. `doppler.mpsk`'s module docstring has
    said *"M-ary PSK mapping: hard and **soft**"* since the module shipped, and
    there is no soft anything in it — five functions, all hard-decision. The
    caller that makes this urgent is the CCSDS inner decoder: a rate-1/2 K=7
    Viterbi fed hard bits gives up roughly **2 dB** of the coding gain it
    exists to deliver, which is more than the difference between having the
    convolutional code and not having it.

    Three things the throwaway prototype settled before any C was written, all
    measured against the shipped `mpsk_map`/`mpsk_demap` rather than a numpy
    re-derivation:

    - **The sign convention reproduces `mpsk_demap` exactly** — zero
        mismatches over 20 000 symbols at each of M ∈ {2, 4, 8} × Es/N0 ∈
        {−3, 0, +6, +20} dB, including where the decision is nearly a coin
        toss. That matters because the repository has exactly one decision
        rule and a soft demapper that disagreed anywhere would be a second.
    - **BPSK and QPSK have exact closed forms and need no search.** With
        `phi0 = pi/4` the QPSK grid is axis-separable, so its two bits are
        independent BPSK decisions: `4·Re{y}/N0` and `4·(1/√2)·{Re,Im}{y}/N0`,
        agreeing with the general path to 1e-14. Two of the three
        constellations are one multiply per bit.
    - **max-log is free at M = 2 and M = 4 and costs something at M = 8** —
        identical for the first two (with one point per bit subset per axis,
        the maximum *is* the sum), and 3–14 % median LLR error at 8PSK.

    What that costs **in dB** is named as an unknown rather than quoted from
    literature: it is an Eb/N0 offset on a decoded BER curve and cannot be
    measured until the decoder exists, so the header will not claim a figure
    until phase 7 does.
