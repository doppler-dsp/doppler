- **Batch 2 of the numpy-to-`wfmgen` sweep**, four more examples, again a
    different face each:

    | example                | what it now teaches                                                                               |
    | ---------------------- | ------------------------------------------------------------------------------------------------- |
    | `telemetry_fanin_demo` | a **multi-segment scene** — clean, outage, clean as one `Composer`, not three arrays concatenated |
    | `receiver_lock_demo`   | **continuous async DSSS** (`symbol_rate` > 0), the waveform type this demo was hand-indexing      |
    | `rate_converter_demo`  | **`Composer.from_json`** — the face a `--record` document has                                     |
    | `ber_awgn_demo`        | `wfm_awgn_amplitude` at sps = 1, where Es/N0 and per-sample SNR are the same number               |

    `receiver_lock_demo` is the one worth reading. wfmgen has its waveform as
    a first-class type: `--type dsss` with a `symbol_rate` selects the
    continuous form, where the code repeats forever and data rides it at a
    rate that is deliberately not a whole number of chips. That
    asynchronicity is the entire subject of the demo, and it had been
    expressed as index arithmetic
    (`si = floor((idx - 0.37*TE) / tsym)`). All three loops — DLL, Costas
    and SymbolSync — still acquire from a cold start on the generated
    stimulus, which is the assertion that file already carried.

    Two codes stopped being coin flips. `receiver_lock_demo`'s 127-chip code
    and `telemetry_fanin_demo`'s 31-chip one are both `2^n - 1`, so
    `PN(length=7)` and `PN(length=5)` fill their periods exactly. The second
    replaces a hand-written five-stage LFSR whose own docstring explained why
    low autocorrelation sidelobes matter to a CFAR reference — and then
    generated the sequence by hand rather than asking the library.

    **A trap found and documented**: the CLI's `--fs` default of 1.0 does
    **not** reach a JSON scene. `{"type": "tone", "freq": 0.08}` renders at
    DC with no error; `{"type": "tone", "freq": 0.08, "fs": 1.0}` renders at
    0.08. Found because `rate_converter_demo` failed its own frequency check
    with the tone 1245 bins off, which is the gate doing its job.
