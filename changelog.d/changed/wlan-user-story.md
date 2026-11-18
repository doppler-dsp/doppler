- **A PPDU example, and `wlan`'s stage-by-stage benchmark rows.** `wlan_ppdu_demo.c`
    renders a whole PPDU through `dp_wlan_ppdu()` and checks its length, the
    L-STF's period, the L-LTF's repeat and unit power; the guide's one
    sentence naming three functions is now the steps with the example. The
    interleaver, mapper, points, layout and PPDU have bench rows at 64-QAM
    rate 3/4 (#3147).
