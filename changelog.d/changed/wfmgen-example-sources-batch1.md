- **Four examples and the M-PSK gallery page build their stimulus with
    `wfmgen` instead of numpy.** Each one shows a different face of the
    generator rather than repeating one recipe, because the examples are where
    a reader learns which face their own problem wants:

    | example                    | what it now teaches                                                                                                                                                              |
    | -------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
    | `mpsk_receiver_demo`       | the `symbols` source — an arbitrary constellation, which is how ONE function serves BPSK, QPSK **and** 8PSK when `modulation` reaches only the first two; plus `snr_mode="esno"` |
    | `costas_demo`              | `snr_mode="fs"` — SNR per SAMPLE, the question a Costas loop actually asks, and a 12 dB different number from `esno` at sps=16                                                   |
    | `symsync_demo`             | `rc_h` and `wfm_awgn_amplitude` — the primitives for a stimulus the composer *cannot* build                                                                                      |
    | `dll_demo`                 | `PN` — a real maximal-length code                                                                                                                                                |
    | `gallery/mpsk-receiver.md` | `level` in dBFS, replacing a bare `* 0.5`                                                                                                                                        |

    Every conversion was verified numerically rather than assumed, because the
    conventions are the part that goes wrong silently: with noise off the
    `symbols` source is **byte-identical** to `np.repeat`, `snr_mode="esno"`
    and the hand-written `sqrt(sps / (2 * 10**(esn0/10)))` both measure
    10.0 dB at the matched-filter output, `snr_mode="fs"` reproduces its hand
    sigma to five digits, and `wfm_awgn_amplitude` matches to eight decimals.

    **`dll_demo` got a real fix, not just a refactor.** Its 127-chip code was
    `default_rng(1).integers(0, 2, 127)` — a coin flip. 127 is 2^7 - 1, so
    `PN(length=7)` fills the period exactly, and the difference is the
    property a delay lock loop runs on: an m-sequence's off-peak
    autocorrelation is a flat **-1** at every non-zero lag, while the random
    code wandered between **-13 and +3**. The discriminator reads that
    sidelobe structure directly, so the demo had been handing itself a worse
    S-curve than any real spreading code would produce.

    **`symsync_demo` is the one that stays hand-placed, and says why.**
    `Segment.sps` is an integer, so no scene can express "the symbol clock
    runs 1.004x fast and starts 1.7 samples late" — which is the question a
    `SymbolSync` exists to answer. Its pulse and its level are now library
    primitives even though its clock cannot be; `rc_h` is documented for
    exactly this, being the analytic raised cosine at arbitrary non-grid
    times, and the private `rc_pulse` it replaces was a transcription of that
    formula.
