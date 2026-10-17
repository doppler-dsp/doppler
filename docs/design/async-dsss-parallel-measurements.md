# Parallel Asynchronous-Data DSSS Receivers — the measurement record

*The dated record behind [the design page](async-dsss-parallel.md): what each
number there was measured by, with the run. Section numbers are shared, so a
`§6.2` cited on the design page, in an issue or in a code comment is the same
entry here. Every entry comes from one function of
`src/doppler/dsss/tests/characterization/async_dsss_pool/characterize.py`,
named in it, and the entries below are one run of all of them:*

|         |                                                                                                                     |
| ------- | ------------------------------------------------------------------------------------------------------------------- |
| date    | 2026-10-17, 16:42 UTC (load average 3.4)                                                                            |
| machine | `swiftgo-ai9465`: AMD Ryzen AI 9 465 (20 threads), Python 3.14.8                                                    |
| commit  | `730af9f2c`, clean tree                                                                                             |
| command | `make characterize CHARACTERIZATIONS=src/doppler/dsss/tests/characterization/async_dsss_pool/characterize.py` (8 s) |

*A share of one CPU is the push time over the 333.3 µs of signal a push
covers: one push is one code epoch, 2,046 samples at 6.138 MSa/s. Timings
are wall-clock around the Python call, so a rerun on another machine, or
under load, moves them; what it must not move is which state costs what, and
what fits one CPU. The signal is the shipped `Synth`'s continuous DSSS:
Gold-1023 at 3.069 Mcps (so 1/T_E is 3 kHz), 2 samples a chip, 2,700 sym/s
asynchronous BPSK, 1,500 Hz. The search-only rows use noise.*

______________________________________________________________________

### 3.1 Symbol lock through a dropout

`s3_1_lock_hysteresis`: a receiver searching ±1/T_E, locked on a data-on
signal for 0.6 s; then the SIGNAL is cut (the noise runs on, the library's
`wfm_awgn_amplitude` over a unit-noise `Synth`) for the time shown, then
restored for 0.5 s. The receiver's symbol lock (`locked`) is read every
epoch. The detector behind it is a power-weighted average over 30 symbols,
declared after 30 symbols at or above 0.5 and dropped after 15 below 0.3
(`ASYNC_DSSS_RX_LOCK_*`, `async_dsss_receiver_core.h`).

| Es/N0 | dropout      | held | lock fell, after the cut began | lock back, after the signal returned |
| ----- | ------------ | ---- | ------------------------------ | ------------------------------------ |
| 10 dB | 5, 10, 20 ms | yes  |                                |                                      |
| 10 dB | 30 ms        | no   | 24.0 ms                        | 18.0 ms                              |
| 10 dB | 50 ms        | no   | 24.0 ms                        | 19.3 ms                              |
| 10 dB | 100 ms       | no   | 24.0 ms                        | 19.3 ms                              |
| 10 dB | 200 ms       | no   | 24.0 ms                        | 28.7 ms                              |
| 10 dB | for good     | no   | 24.0 ms                        |                                      |
| 5 dB  | 5, 10 ms     | yes  |                                |                                      |
| 5 dB  | 20 ms        | no   | 19.7 ms                        | 57.3 ms                              |
| 5 dB  | 30 ms        | no   | 19.7 ms                        | 99.0 ms                              |
| 5 dB  | 50 ms        | no   | 19.7 ms                        | 76.3 ms                              |
| 5 dB  | 100 ms       | no   | 19.7 ms                        | 61.0 ms                              |
| 5 dB  | 200 ms       | no   | 19.7 ms                        | 70.7 ms                              |
| 5 dB  | for good     | no   | 19.7 ms                        |                                      |

"Lock back" is the receiver as built, still tracking with its loops running
through the dropout; it does not leave tracking (§3). One seed per point.

### 5.1 Arrival to tracking

`s5_1_acquisition`: one receiver, its own search over ±1/T_E, on 1 s of
signal with data on, counting the pushes it spent searching and refining
before it tracked.

| Es/N0 | searching | refining | arrival to tracking | tracked |
| ----- | --------- | -------- | ------------------- | ------- |
| 10 dB | 9         | 127      | 45.3 ms             | yes     |
| 5 dB  | 45        | 218      | 87.7 ms             | yes     |

One seed per point (the synth's seed 7), so these are single draws, not a
distribution.

### 6.1 One tracking receiver

`s6_1_tracking`: the same receiver over 2 s of signal, timed per push while
it tracked.

| Es/N0 | signal    | tracking pushes | median push | share of one CPU |
| ----- | --------- | --------------- | ----------- | ---------------- |
| 10 dB | data on   | 5,864           | 23.8 µs     | 0.074            |
| 5 dB  | data on   | 5,737           | 24.2 µs     | 0.081            |
| 10 dB | code only | 5,865           | 23.7 µs     | 0.072            |

### 6.2 One receiver searching

`s6_2_search_depth`: on noise, so the search never stops. At one tile
(±0.49/T_E about the receiver's offset) and over the whole carrier range
(±1/T_E). "The receiver's own" is `AsyncDsssReceiver` with that
`doppler_uncertainty`, its one-epoch looks. The slow-time rows are the
continuous `Acquisition` engine the receiver carries, with a coherent block
of D epochs: D = 16 from a 31-epoch data-free window, D = 94 from SPEC's 500
with the 500 Hz/s Doppler rate bounding it. At least 1,024 pushes and four
dwells are timed after a warm-up block.

| range    | search             | D   | dwell   | share of one CPU |
| -------- | ------------------ | --- | ------- | ---------------- |
| one tile | the receiver's own | 1   |         | 0.169            |
| one tile | slow-time          | 16  | 1.2 ms  | 0.235            |
| one tile | slow-time          | 94  | 11.0 ms | 0.328            |
| ±1/T_E   | the receiver's own | 1   |         | 0.340            |
| ±1/T_E   | slow-time          | 16  | 0.8 ms  | 0.386            |
| ±1/T_E   | slow-time          | 94  | 14.3 ms | 0.644            |

### 7.1 The pool as built: where a push goes

`s7_1_split`: the pool (four slots, one thread), one receiver, and a
continuous `Acquisition` built as the pool builds its searcher, each pushed
the same epoch in turn on 1 s of one code-only emitter at 47 dB-Hz, searched
over ±1/T_E with 31 code-only epochs. A dwell decides every 16th push.

| object                    | ordinary push | slowest 1 in 16 | share of one CPU |
| ------------------------- | ------------- | --------------- | ---------------- |
| one receiver              | 25.3 µs       | 84.0 µs         | 0.090            |
| the pool                  | 98.5 µs       | 1,824 µs        | 0.639            |
| the pool's searcher alone | 74.5 µs       | 1,776 µs        | 0.556            |

The pool's cost above one receiver is its searcher's: 98.5 − 25.3 = 73.2 µs
against 74.5 µs on an ordinary push.

### 7.2 That searcher at the design point

`s7_2_design_point`: the same engine at ±50 kHz (35 tiles), SPEC's 500
code-only epochs and the 500 Hz/s rate, so D = 94; on noise, four dwells
timed.

| threads | ordinary push | dwell  | share of one CPU |
| ------- | ------------- | ------ | ---------------- |
| 1       | 703 µs        | 165 ms | 7.37             |
| 4       | 216 µs        | 50 ms  | 2.25             |
| 8       | 147 µs        | 33 ms  | 1.51             |

A dwell decides each 94-epoch block, 31.3 ms of signal; on eight threads the
dwell alone takes longer than the block.
