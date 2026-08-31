- **`Gold` is certified** — the 32nd object and the wfm module's last: 17
    limits, 4 findings. The subject is the family, and the header's count of it
    was wrong in both directions — it said 1024, where only **1023** codes are
    reachable (a zero seed is refused) out of a true family of **1025** (the
    two constituent m-sequences are unreachable, since the generator always
    XORs both registers). Corrected and pinned. Also quantified: the
    three-valued set `{-1, -65, 63}` is the theoretical `{-1, -t, t-2}` at
    `t = 2^((n+2)/2)+1 = 65`, costing **23.9 dB** of peak-to-sidelobe margin
    against a plain m-sequence's 60.2 dB — the price of having a family at
    all. [Evidence][gold-cert].

[gold-cert]: https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/gold/results.md
