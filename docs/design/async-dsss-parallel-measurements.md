# Parallel Asynchronous-Data DSSS Receivers — the measurement record

*The dated record behind [the design page](async-dsss-parallel.md): what each
number there was measured by, with the run. Section numbers are shared, so a
`§6.2` cited on the design page, in an issue or in a code comment is the same
entry here. Every entry comes from one function of
`src/doppler/dsss/tests/characterization/async_dsss_pool/characterize.py`,
named in it, and the entries below are one run of all of them:*

|         |                                                                                                                     |
| ------- | ------------------------------------------------------------------------------------------------------------------- |
| date    | 2026-10-17, 16:35 UTC                                                                                               |
| machine | `swiftgo-ai9465`: AMD Ryzen AI 9 465 (20 threads), Python 3.14.8                                                    |
| commit  | `29f50183f`, clean tree                                                                                             |
| command | `make characterize CHARACTERIZATIONS=src/doppler/dsss/tests/characterization/async_dsss_pool/characterize.py` (5 s) |

*A share of one CPU is the push time over the 333.3 µs of signal a push
covers: one push is one code epoch, 2,046 samples at 6.138 MSa/s. Timings
are wall-clock around the Python call, so a rerun on another machine, or
under load, moves them; what it must not move is which state costs what, and
what fits one CPU. The signal is the shipped `Synth`'s continuous DSSS:
Gold-1023 at 3.069 Mcps (so 1/T_E is 3 kHz), 2 samples a chip, 2,700 sym/s
asynchronous BPSK, 1,500 Hz. The search-only rows use noise.*

______________________________________________________________________

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
| 10 dB | data on   | 5,864           | 23.7 µs     | 0.072            |
| 5 dB  | data on   | 5,737           | 23.9 µs     | 0.077            |
| 10 dB | code only | 5,865           | 23.5 µs     | 0.071            |

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
| one tile | slow-time          | 16  | 1.2 ms  | 0.232            |
| one tile | slow-time          | 94  | 10.8 ms | 0.321            |
| ±1/T_E   | the receiver's own | 1   |         | 0.327            |
| ±1/T_E   | slow-time          | 16  | 0.8 ms  | 0.361            |
| ±1/T_E   | slow-time          | 94  | 14.0 ms | 0.636            |

### 7.1 The pool as built: where a push goes

`s7_1_split`: the pool (four slots, one thread), one receiver, and a
continuous `Acquisition` built as the pool builds its searcher, each pushed
the same epoch in turn on 1 s of one code-only emitter at 47 dB-Hz, searched
over ±1/T_E with 31 code-only epochs. A dwell decides every 16th push.

| object                    | ordinary push | slowest 1 in 16 | share of one CPU |
| ------------------------- | ------------- | --------------- | ---------------- |
| one receiver              | 25.5 µs       | 75.2 µs         | 0.086            |
| the pool                  | 98.7 µs       | 1,753 µs        | 0.615            |
| the pool's searcher alone | 74.2 µs       | 1,708 µs        | 0.536            |

The pool's cost above one receiver is its searcher's: 98.7 − 25.5 = 73.2 µs
against 74.2 µs on an ordinary push.

### 7.2 That searcher at the design point

`s7_2_design_point`: the same engine at ±50 kHz (35 tiles), SPEC's 500
code-only epochs and the 500 Hz/s rate, so D = 94; on noise, four dwells
timed.

| threads | ordinary push | dwell  | share of one CPU |
| ------- | ------------- | ------ | ---------------- |
| 1       | 700 µs        | 170 ms | 7.58             |
| 4       | 233 µs        | 52 ms  | 2.37             |
| 8       | 146 µs        | 31 ms  | 1.46             |

A dwell decides each 94-epoch block, 31.3 ms of signal; on eight threads the
dwell alone takes as long as the block.
