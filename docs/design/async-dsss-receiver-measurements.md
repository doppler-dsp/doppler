# AsyncDsssReceiver — the measurement record

*The dated record behind [the design page](async-dsss-receiver.md): what
each step of its §12 measured, in the order it was measured, with the
numbers, the wrong guesses and what corrected them. The design page
states what is; this page is why. Section numbers are shared — a `§12.17`
cited in an issue, a harness or a code comment is the same entry here and
there — and nothing is rewritten after the fact: a later entry corrects an
earlier one in its own words (§12.18 corrects §12.17's attribution of the
40 dB-Hz doubles, for example). Every number comes from a harness under
`native/validation/`, `native/benchmarks/` or a test under `native/tests/`;
the entry names it.*

______________________________________________________________________

### 12.1 What was measured (2026-09-02) — step 8, the budget

Three component benches gained operating-point rows and were run on an
8-core build box, one core, minimum of rounds (`bench_ddc_core`,
`bench_acq_core`, `bench_async_dsss_receiver_core`; `make bench` runs all
three; the rows are `rate=0.77`, `op5M_*`/`op2M_*` and `*,op5M`):

| stage                                   | ns per sample      | of one core, operating | of one core, 30 MSa/s floor |
| --------------------------------------- | ------------------ | ---------------------- | --------------------------- |
| front-end DDC, 13 → 10 MSa/s            | **13.6** per input | 0.18                   | 0.41                        |
| searcher, 5 Mcps, ±50 kHz, **21** tiles | **214** per output | **2.14**               | 4.9                         |
| searcher, 2 Mcps, ±50 kHz, **53** tiles | **523** per output | **2.09**               | 4.8                         |
| searcher, 5 Mcps, ±5 kHz, 3 tiles       | 36 per output      | 0.36                   | 0.83                        |
| searcher, 2 Mcps, ±5 kHz, 7 tiles       | 75 per output      | 0.30                   | 0.69                        |
| one receiver, tracking (warm), 5 Mcps   | **44** per output  | 0.44                   | 1.0                         |
| one receiver, cold (search + refine)    | 89 per output      | —                      | —                           |

Seven things this settles, and one it corrects:

- **The arbitrary-ratio front end is 4.2× the integer cascades** — 13.6 ns
    against 2.4–4.0 for rates 0.05–0.5 — so choosing 13 MSa/s to force it
    (§6.4) priced the front end at its real cost; it is still under a fifth
    of a core at the operating point.
- **The searcher over ±50 kHz does not fit on one core at any chip rate:
    2.1× real time at both ends of the range.** Its cost is ~10 ns per tile
    per output sample, and the tile count rises exactly as the rate falls,
    so the tile-samples per second — and the core count — are the same at 2
    and 5 Mcps. That confirms §6.4's arithmetic and turns its third bullet
    into a requirement: **the tiles must be partitioned across cores**,
    three at the operating point, five at the floor, before any margin.
    The tiles are independent inverse FFTs off one shared forward FFT, so
    the split is either inside the engine (a parallel-for over tiles per
    epoch, which keeps one forward FFT) or across engines each given a slice
    of the uncertainty (which repeats the forward FFT per slice but needs
    no threading inside the engine and matches the "processes as needed"
    shape of §1.1). **Measured, the same day:** one engine over a third of
    ±50 kHz (`op5M_U17k`, 7 tiles; `op2M_U17k`, 19) costs 75 and 192 ns
    per output sample, so three of them are 225 and 576 against the single
    engine's 213 and 520 — **6% and 11% for the slice**, the forward FFT
    repeated per slice being worth about one tile. **Superseded
    (2026-09-03): the split is a roll per thread inside the engine, on
    persistent workers** — see the bullet below §2.3 and §8.2. The slice
    was the shape to take while the only parallel-for created its workers
    per call at ~15 µs each (`burst-bank.md` §10.4) against a 205–512 µs
    epoch — 25–60% of the work for eight workers. A persistent pool pays
    that once.
- **Doppler pre-compensation is worth 6–7× on the searcher** — 0.36 and
    0.30 of a core over ±5 kHz — and nothing on anyone else. With it the
    searcher fits on one core with room; without it the partition above is
    mandatory.
- **One tracking receiver is 0.44 of a core, and receivers add.** The
    pool of twelve is 5.3 cores at the operating point and 12 at the floor
    — the largest single line in the budget. Run as concurrent processes on
    the 8-core box, the warm row went from 43 ns alone to 46 with four
    running and 46–55 with eight (the top of that spread is the core the
    operating system was also using), so **cores add nearly linearly** for
    the receivers and the memory system is not the limit at this scale.
- **The population, one process, ±50 kHz, 5 Mcps: about 7.6 cores** at
    the operating point (0.18 + 2.14 + 12 × 0.44), **17.5 at the floor**;
    with pre-compensation 5.8 and 13.4. At the 2× margin §6.4 asks for,
    that is 15 and 35 cores without pre-compensation, 12 and 27 with. The
    server "will have a lot"; this is what a lot means.
- **Correction:** the engine's tile rule gives **21** tiles at 5 Mcps over
    ±50 kHz, not the 23 this page derived from `burst-bank.md`'s channel
    formula; the table in §6.1 now carries the engine's number. Nothing
    else moved.

Not measured here: the searcher with the peak list (it does not exist;
the pick is one pass over the surface and will not move the ~10 ns per
tile), a replica subtraction (no replica output yet), and the whole
population as one run with its detection count beside the rate — that
needs the orchestrator, and is what step 8 still owes.

### 12.2 What was measured (2026-09-02) — step 1, the floor

`native/validation/acq_emitter_floor.c` (`make validate-c`; its `--check`
is in the C suite): one emitter rendered by the shipped continuous-DSSS
synth (`wfm_synth`, the generator wfmgen uses) on the engine's own
single-look surface at the operating point, ±50 kHz, Gold-1023 (CCSDS
#365), read back from `mag_buf` after the dwell and binned outside the
one-tile × one-chip exclusion zone. Everything in dB below the emitter's
peak:

| emitter                            | same tile, other lags               | worst cell, any tile                                                 | **worst cell at another code phase**   | CFAR reference |
| ---------------------------------- | ----------------------------------- | -------------------------------------------------------------------- | -------------------------------------- | -------------- |
| tile-centred, no data transition   | **−23.9** (the Gold bound, exactly) | −21.0 (far tiles)                                                    | **−21.0**                              | −32.9          |
| centred, a transition in the epoch | −18.7                               | **0.0** — an equal twin two tiles away; the reported tile is one off | **−16.0**                              | −28.8          |
| half a tile off centre, no data    | −18.2                               | −9.5 (two tiles away)                                                | **−16.1**                              | −28.8          |
| half a tile off, a transition      | −14.6                               | **0.0** — twins two *and* three-plus tiles away                      | **−12.8** (5 Mcps), **−11.9** (2 Mcps) | −25.2          |

The two chip rates agree to 0.1 dB except in the last row, where the
lower rate's narrower tiles spread the split emitter further. With noise,
one strong emitter moves the CFAR reference by 0.18 dB at 55 dB-Hz and by
nothing measurable at 45 and 40.

Four things this settles:

- **The design number is −13 dB, not −24.** The Gold bound holds exactly
    where it applies — full period, zero Doppler, no data — and that is the
    spot check. But the searcher looks at every epoch, an emitter's data
    puts a transition in 55% of them at 1.8 epochs per symbol, and it sits
    anywhere in its tile; in those cases the worst cell at *another code
    phase* is 16 dB down, and with both at once 12 to 13 dB. So a second
    emitter more than about 13 dB weaker than the strongest, less the
    detection margin, is under the strong one's floor and is the
    cancellation branch's (§9); §6.3's fork is at −13 dB.
- **One emitter can make more than one peak, and tile distance does not
    bound it.** A transition in the epoch splits an emitter into equal
    twins, two tiles apart when centred and three or more when it is also
    off centre; a half-tile offset alone puts a −9.5 dB sidelobe two tiles
    away. Every one of them is at the emitter's own code phase. The peak
    list therefore needs a rule beside the zone that keys on **code
    phase**, not tile distance: a peak within one chip of an already-listed
    peak's code phase is a candidate twin. Two real emitters *can* share a
    code phase at different Dopplers, so the twin is not dropped on one
    epoch — it is held, and the next epochs decide: a twin moves with the
    transition's position and vanishes in the emitter's data-free window,
    a real emitter stays put. That is a two-epoch rule, and it belongs in
    §7.1.
- **The reference does not hide the weak emitter; the sidelobes do.** A
    strong emitter leaves the CFAR reference where the noise put it, so a
    weak emitter's gate is unchanged; what stops it being a peak is the
    strong one's cells standing over it. That is why removing the strong
    emitter (cancellation) is the only fix on that branch, as §6.3 argued.
- **Where the peak list is taken matters.** In an emitter's own data-free
    window the *other* emitters are still carrying data, so −13 to −16 dB
    is the operating floor everywhere; the data-free window buys the
    emitter its own clean, single peak, not a clean surface.

### 12.3 What was measured (2026-09-02) — step 6, the release

`native/validation/async_dsss_receiver_release.c` (`make validate-c`; its
`--check` is in the C suite): the receiver as built, tracking one emitter
from the shipped continuous-DSSS synth at the operating point (5 Mcps,
2700 sym/s asynchronous BPSK, PRBS data) with the shipped `awgn` at two
C/N0s, fed one epoch (0.2 ms) at a time with both lock flags read after
every block. Once tracking with symbol lock held for 200 blocks, one event
per trial; 30 trials per event, 10 of 3 s for the on-time.

| C/N0 (Es/N0)       | event           | code lock off                   | symbol lock off            | both off                    | back by 1.5 s (code / symbol) |
| ------------------ | --------------- | ------------------------------- | -------------------------- | --------------------------- | ----------------------------- |
| 45 dB-Hz (10.7 dB) | switch-off      | 1.8 ms                          | 25 ms (max 38)             | 25 ms, stays off            | 0 / 0 of 30                   |
|                    | 10 dB fade, 1 s | 1.8 ms                          | 44 ms (max 83)             | for 0.99 s                  | 30 / 29                       |
|                    | 20 dB fade, 1 s | 1.8 ms                          | 26 ms                      | for 1.47 s                  | 29 / 27                       |
|                    | π/2 phase step  | 164 ms median, 783 max          | held in 29 of 30           | never                       | 29 / 30                       |
|                    | nothing, 30 s   | off 0.6% of blocks, 79 dips     | never                      | never                       |                               |
| 40 dB-Hz (5.7 dB)  | switch-off      | already off                     | 20 ms (max 34)             | 20 ms, stays off            | 0 / 0                         |
|                    | 10 dB fade, 1 s | already off                     | 22 ms                      | for 1.09 s                  | 0 / 23                        |
|                    | 20 dB fade, 1 s | already off                     | 20 ms                      | for 1.48 s                  | 2 / 17                        |
|                    | π/2 phase step  | already off                     | 9 ms, held in 17 of 30     | ≤ 52 ms                     | 2 / 30                        |
|                    | nothing, 27 s   | off **96%** of blocks, 466 dips | 0.5% of blocks, 4 episodes | 0.5%, longest run **36 ms** |                               |

What it settles, and what it overturned:

- **Code lock is not a presence flag.** At Es/N0 10.7 dB it dips for a
    block or two three times a second on a healthy signal; at 5.7 dB it is
    off 96% of the time while the receiver is tracking and decoding. It
    drops on a 10 dB fade in the same 2 ms as on a switch-off, and a phase
    step that symbol lock rides takes it down 160 ms later. It is the
    `Dll`'s per-decision CFAR flag on prompt power, and it does exactly
    that. The page's original rule — release on code lock — would have
    released on every fade and, near the floor, continuously.
- **Symbol lock is the stable one.** Never a dip in 30 s at 10.7 dB; four
    episodes in 27 s at 5.7 dB, the longest 36 ms. It drops 20–45 ms after
    a switch-off or the start of a fade, and stays down for the fade's
    length.
- **The rule is both flags down for longer than the fade.** A switch-off
    holds both down indefinitely; a 1 s fade holds both down for 1.0–1.5 s
    and then brings them back at 10.7 dB (less reliably at 5.7 dB, where
    symbol lock returned in 17–23 of 30 within the watch); a healthy
    signal's longest both-down run is 36–52 ms. Two seconds separates
    those with a margin of forty on the healthy side and two on the fade
    side, and costs under 1% of the shortest on-time. The confirm
    interval is a time, and the fade sets it — not a verify count.
- **The replica's gate is symbol lock.** A gate on code lock would drop
    the replica, and raise the searcher's floor, three times a second.
- **A recovered receiver is the same assignment.** After a fade both
    flags return on the same receiver with the same code phase — the
    emitter never restarted — so a release that fires during a fade would
    hand a fresh receiver an emitter one is already tracking. That is the
    false release the interval is sized against.

**After the fix (§3.7, §12.4) — the same sweep, the receiver's detector
sized and symbol-aided:**

| C/N0 (Es/N0)       | event           | code lock off        | symbol lock off            | both off         | back by 1.5 s (code / symbol) |
| ------------------ | --------------- | -------------------- | -------------------------- | ---------------- | ----------------------------- |
| 45 dB-Hz (10.7 dB) | switch-off      | **3.5 ms** (max 4.3) | 25 ms                      | 25 ms, stays off | 0 / 0 of 30                   |
|                    | 10 dB fade, 1 s | 3.7 ms               | 44 ms                      | for 0.56 s       | **30** / 29                   |
|                    | 20 dB fade, 1 s | 3.5 ms               | 26 ms                      | for 1.12 s       | 29 / 27                       |
|                    | π/2 phase step  | **held, 30 of 30**   | held in 29 of 30           | never            | 30 / 30                       |
|                    | nothing, 30 s   | **never**            | never                      | never            |                               |
| 40 dB-Hz (5.7 dB)  | switch-off      | **11.5 ms** (max 15) | 20 ms                      | 20 ms, stays off | 0 / 0                         |
|                    | 10 dB fade, 1 s | 12 ms                | 22 ms                      | for 0.99 s       | **30** / 23                   |
|                    | 20 dB fade, 1 s | 11.5 ms              | 20 ms                      | for 1.48 s       | 24 / 17                       |
|                    | π/2 phase step  | **held, 30 of 30**   | 9 ms, held in 17 of 30     | never            | 30 / 30                       |
|                    | nothing, 27 s   | **never**            | 0.5% of blocks, 4 episodes | never            |                               |

Code lock is the presence flag the page first wanted, once its looks are
sized and symbol-aligned: off within 4 ms of a switch-off at 10.7 dB and
12 ms at the floor, held through a phase step in every trial, back after
every fade at 10.7 dB and after 24 of 30 deep fades at the floor, and not
one dip in 57 s of on-time across both C/N0s. Symbol lock is now the one
that moves on a carrier disturbance. The rule of §10 keeps its shape —
both flags down for longer than the fade — because a fade still takes both
down for its duration; what the fix buys is a clock that starts within
milliseconds of a real loss and never starts on a healthy signal.

Not measured yet: the false-release
rate over a whole on-time — the 15-minute maximum of §6.1 — rather than
half a minute (the both-down rate at 5.7 dB is 0.5% of blocks in runs of
tens of milliseconds; whether a run ever reaches seconds is what fifteen
minutes would say), and any of this on the hand-off-mode receiver, which
does not exist.

### 12.4 What was measured (2026-09-02) — the DLL's telemetry, and the aid

The receiver's DLL alone (`bn 0.002, segments 4`), fed the shipped synth's
continuous DSSS at the operating point with a `Telemetry` context attached
(`receiver_lock_demo.py`'s pattern), 2 s per run; `code.lock` against its
threshold, `code.locked`, the discriminator and the tracked rate, for three
detectors on the same signal
(`src/doppler/dsss/tests/characterization/dll_lock/`):

| Es/N0   | detector                        | per-look Es/N0 | looks  | R vs eta       | miss per decision | off     | drops per s | code rate |
| ------- | ------------------------------- | -------------- | ------ | -------------- | ----------------- | ------- | ----------- | --------- |
| 10.7 dB | 20 partials (default)           | 2.1 dB         | 20     | 9.5 vs 8.7     | 5.1%              | 1.4%    | 4.5         | 1.000000  |
|         | partials sized (`det_n_noncoh`) | 2.1 dB         | 25     | above          | 1.7%              | 0.4%    | 1.0         | 1.000000  |
|         | **symbol-aided, sized**         | 10.7 dB        | **3**  | well above     | **0.0%**          | 0.1%    | **0**       | 1.000000  |
| 5.7 dB  | 20 partials (default)           | −2.9 dB        | 20     | **7.5 vs 8.7** | 86%               | **97%** | 11          | 1.000000  |
|         | partials sized                  | −2.9 dB        | 161    | 22 vs 20       | 2.2%              | 1.6%    | 0.5         | 1.000000  |
|         | **symbol-aided, sized**         | 5.7 dB         | **10** | well above     | **0.4%**          | 0.2%    | **0**       | 1.000000  |

What it settles:

- **The loop was never the problem.** In every run the tracked code rate
    is 1.000000 within 3 ppm and the discriminator is zero-mean with no
    drift, including the run where the flag read "unlocked" 97% of the
    time. §12.3's chatter and 96% were the detector's default integration
    — 20 quarter-epoch partials, 1 ms, sized for nothing — sitting under
    its own threshold at the floor and grazing it at 10.7 dB.
- **Sizing alone fixes the 96%; the aid fixes the margin.** Sized
    partials need 161 looks at the floor and still miss 2% of decisions;
    the symbol-aided look needs 10 and misses 0.4%, with the statistic
    well clear of its threshold at both C/N0s and no drop in 2 s.
- **The hysteresis is now a budget.** At the aided miss rate,
    `det_verify_count(0.01, 1e-6)` gives three consecutive misses to drop,
    which the receiver sets; at two, the floor's 0.4% would have produced
    a false drop about every four minutes of decisions.
- **What this does to the release rule (§10).** Code lock is a usable
    presence flag again — §12.3's post-fix sweep shows it off within 4–12
    ms of a switch-off, held through a phase step in every trial, and not
    dipping once in 57 s of on-time. The both-flags-down rule stands
    because a fade still takes any CFAR flag down for its duration; what
    changes is that the "both down" clock now starts within milliseconds
    of a real loss and never on a healthy signal.

### 12.5 What was measured (2026-09-02) — the discriminator on the aided window

`native/validation/dll_aid_jitter.c` (`make validate-c`; its `--check` is
in the C suite): the receiver's DLL alone (`bn 0.002`, half-chip spacing,
four partials per epoch) fed the shipped synth's continuous DSSS at the
operating point with the shipped `awgn`, one epoch per call, its tracked
code phase against the generator's after every block; the per-epoch
look-back and the symbol-aided window on the same stream. One seed per
cell, 12 000 epochs measured after 3 000 settling, so a ratio is good to
about 8%.

| C/N0 (Es/N0) | jitter, per-epoch | jitter, aided | ratio | pull-in from 0.25 / 0.5 / 0.75 chip, per-epoch | aided           |
| ------------ | ----------------- | ------------- | ----- | ---------------------------------------------- | --------------- |
| 50 (15.7 dB) | 0.0078 chips      | 0.0062        | 0.79  |                                                |                 |
| 45 (10.7 dB) | 0.0132            | 0.0136        | 1.03  | 179 / 234 / 251 ms, 10 of 10 each              | 145 / 202 / 222 |
| 42 (7.7 dB)  | 0.0173            | 0.0225        | 1.30  |                                                |                 |
| 40 (5.7 dB)  | 0.0208            | 0.0289        | 1.39  | 249 / 298 / 328 ms, 10 of 10 each              | 204 / 238 / 258 |
| 38 (3.7 dB)  | 0.0341            | 0.0371        | 1.09  |                                                |                 |
| 36 (1.7 dB)  | 0.0421            | 0.0524        | 1.25  |                                                |                 |
| 34 (−0.3 dB) | 0.0606            | 0.0650        | 1.07  |                                                |                 |

Neither loop lost the code in any cell, and both read a code rate of
1.000000. Both discriminators zero at the same code phase (−0.004 chips,
clean, both).

What it settles:

- **Above 45 dB-Hz the aided loop is tighter, and the reason is the
    look-back.** On a data-free stream the per-epoch loop reads 0.0060
    chips at 50 dB-Hz; with data, 0.0088. Its handling of the transitions
    — a window borrowed from the previous epoch at the previous phase, and
    a transition in the first partial that no candidate can exclude — is
    what sets its jitter there. The aided window pays nothing for the data.
- **At the floor the noise sets the jitter, and the aided window's unused
    partials cost.** The window is six of the 7.24 partials a symbol
    spans; the transition partial and the slack are left out. With the
    hypothesis pinned at the truth the aided loop reads 0.025 chips at 40
    dB-Hz, with its own argmax 0.027–0.030, the per-epoch loop 0.022. A
    power EMA four times longer, or a window one partial shorter, moves
    it by less than the trial spread; a hypothesis a partial off reads
    0.04–0.06. So the loss is the window, not its choice.
- **Pull-in is 15–20% faster in every cell**, and the loop gain is the
    same: under a 100 ppm code-rate step the two modes' integrators agree
    to under 1% of the step mid-transient (`test_dll_core.c` §6c), where a
    filter left at its per-epoch gains reads 1.8× slower.
- **What it means for the receiver: hundredths of a chip either way.** A
    0.03-chip RMS code error is under 0.1 dB of despreading loss. The
    receiver keeps the one declaration — the symbol period aids the looks
    and the loop — and the number to beat, should this be revisited, is
    0.022 chips at 40 dB-Hz.

### 12.6 What was measured (2026-09-02) — steps 2–4, the peak list

`native/validation/acq_peak_list.c` (`make validate-c`; its `--check` is
in the C suite): the continuous engine at 5 Mcps, ±50 kHz (21 tiles),
sized by its own physics at a design C/N0 (15 looks per dwell at 45
dB-Hz), one shipped synth per emitter with PRBS data at 2700 sym/s, the
shipped `awgn` at the strong emitter's C/N0, `max_peaks = 4`; 200 scenes
per cell, two dwells each and the second scored, since the two-epoch rule
lists a same-code-phase emitter from the second dwell on.

**Step 2, separability** (two equal emitters at 45 dB-Hz; P(both listed)):

| Δτ \\ Δf | 0.5 tile | 1 tile | 2 tiles | 4 tiles |
| -------- | -------- | ------ | ------- | ------- |
| 0.5 chip | 0.00     | 0.00   | 1.00    | 1.00    |
| 1 chip   | 0.00     | 0.07   | 1.00    | 1.00    |
| 2 chips  | 0.95     | 1.00   | 1.00    | 1.00    |
| 4 chips  | 0.94     | 1.00   | 1.00    | 1.00    |

Inside one tile and one chip the two are one peak, as §7.1 says the zone
makes them; outside it both are listed in every dwell, including the
same-code-phase pairs the twin rule holds for one dwell. Half a tile off
costs 5%, the straddle. Every listed peak was on its emitter's tile and
within a chip of its code phase; no false peak in 3 200 dwells.

**Step 3, the knee** (strong emitter fixed, weak stepped down 4 tiles and
100 chips away, the engine sized at the *weak* emitter's C/N0 so the floor
decides rather than the sizing; the weak emitter alone as the control):

| strong   | spread | weak C/N0 | looks  | P(weak, with strong) | P(weak alone) |
| -------- | ------ | --------- | ------ | -------------------- | ------------- |
| 55 dB-Hz | 3–15   | 52–40     | 2–88   | 0.98–1.00            | 1.00          |
|          | 18     | 37        | 256    | 1.00                 | 1.00          |
|          | **21** | 34        | 256    | **0.12**             | 0.66          |
|          | 24     | 31        | 256    | 0.00                 | 0.03          |
| 45 dB-Hz | 0–9    | 45–36     | 15–256 | 1.00                 | 0.99–1.00     |
|          | 12     | 33        | 256    | 0.24                 | 0.27          |
|          | 15     | 30        | 256    | 0.01                 | 0.00          |

The knee is where the two curves part: at a 55 dB-Hz strong emitter,
between 18 and 21 dB of spread — deeper than §12.2's −13 to −16 dB
single-look floor, because the non-coherent sum favours the weak
emitter's consistent peak over the strong one's data-dependent sidelobes.
At 45 dB-Hz the weak emitter is noise-limited before the floor reaches
it: the two curves fall together from 12 dB, and no floor-limited miss is
seen down to 33 dB-Hz. The fork of §6.3 stays at −13 dB as the single-look
worst case; a receiver that integrates buys a few dB past it.

**Step 4, pfa under the list** (pure noise, engine sized at 45 dB-Hz,
configured pfa 1e-2, 20 000 dwells): reported dwells 0.0091 / 0.0102 /
0.0085 at `max_peaks` 1 / 4 / 8, one peak per reported dwell — the list
does not change the false-alarm rate, and on the same noise the same
dwells report at 1 and at 4 (the `--check` pins that). With one strong
emitter present at 45 dB-Hz, false peaks at other code phases run at
0.0005 per dwell, the configured 1e-3 pfa or under; the emitter is listed
in 2 000 of 2 000 dwells with no twin listed.

**What it settles, and the one thing it raised:**

- The zone is the resolution and costs nothing outside it; the twin rule
    costs one dwell for a real same-code-phase emitter and nothing else.
- The rule's table must carry every pick of the previous dwell, listed or
    held: two equal emitters at one code phase swap places as the
    strongest, and holding only the held ones listed both in 30% of dwells
    (measured before the fix; 100% after).
- **Under long non-coherent integration a strong emitter's same-code-phase
    sidelobes persist and pass the rule.** At 256 looks (52 ms) a 55 dB-Hz
    emitter lists 1.8 twins per dwell at its own code phase on other tiles:
    the sum averages the data-free window away, so "still there next
    dwell" no longer separates a twin from a second emitter. A power
    rule would — a same-code-phase peak more than the floor below its
    parent is the parent's, and an emitter that far under is the
    cancellation branch's anyway (§6.3) — and it is open
    ([#1190](https://github.com/doppler-dsp/doppler/issues/1191)). At the
    operating point's 15 looks no twin was listed in 2 000 dwells.

______________________________________________________________________

### 12.7 What was measured (2026-09-05) — step 11, the block-coherent searcher

`native/validation/acq_block_coherent.c` (`make validate-c`; its `--check`
is in the C suite): the continuous engine with the depth its window buys —
813 whole code-only epochs at 5 Mcps and 324 at 2 (§2.1), the rate bound
of 500 Hz/s deciding: **D = 154 and 61**, a **31.7 Hz row** — over ±50 kHz
(21 and 53 tiles), one emitter from the shipped synth at tile 5 plus a
quarter row, sized for one look, the surface read back through §2.4's
tap in the gate's own units. Everything in dB below the emitter's peak,
cells outside the exclusion zone, by Doppler-row distance; `other` is
the worst cell at another code phase.

**The floor and the straddles** (clean, 5 Mcps; 2 Mcps agrees to 1 dB):

| block                       | peak/gate  | `conc` | row 1 | row 2    | row 3+ | **other** |
| --------------------------- | ---------- | ------ | ----- | -------- | ------ | --------- |
| aligned — pure code         | 1874 / 5.4 | 0.92   | −14.0 | −16.9    | −20.8  | **−21.0** |
| one transition mid-block    | 1121 / 5.4 | 0.49   | −5.8  | **−0.0** | −11.4  | −20.9     |
| PRBS data, the whole block  | 106 / 5.4  | 0.04   | −1.1  | −2.9     | −0.0   | −20.5     |
| the window's edge mid-block | 420 / 5.4  | 0.43   | −10.1 | −8.2     | −8.2   | −20.9     |

Five things this settles:

- **The aligned block gives the transition-free floor, −21 dB at another
    code phase** — the number §12 step 11 expected, and 8 dB below the −13
    the single-look surface has under data (§12.2). Inside the emitter's own
    column the slow-time transform's rectangular window puts its first
    sidelobe at −14 dB one row out; a taper would trade that for a wider
    main lobe, and nothing here needs it.
- **A block that straddles a data transition splits the emitter into
    twins.** One transition mid-block halves the peak and puts an equal
    copy two rows away at the same code phase; the reported row is one off
    the truth (32 Hz). That is §12.2's twin rule again at the row scale —
    at the emitter's own code phase, so the two-epoch rule and the
    concentration see it, and the floor at other code phases is untouched.
- **A block inside the data section is a weak, smeared copy, still at its
    code phase.** PRBS data through the whole block spreads the emitter over
    every row of its column at −1 to −3 dB of the peak, which itself is
    25 dB below the aligned block's (about `10·log10 D` and the data's
    spectrum) — a copy a real C/N0 leaves under the floor, and the
    assigned table excludes in any case (§2.3).
- **The window's edge mid-block — the maintainer's case — is half of
    each.** Half pure code, half data: the peak is 13 dB down, the column
    spread at −8 to −10 dB, and the peak is exactly at the emitter's code
    phase (47 for that block's chip offset). The edge falls at no chip
    phase (§5.4), so one block per window sees this at each end.
- **The concentration is the discriminator.** 0.92 aligned, 0.49 for the
    twins, 0.43 at the edge, 0.04 under data: a second emitter is a second
    column and leaves its neighbour's column alone, so a low `conc` at one
    code phase is one emitter's splatter and never two emitters. The engine
    emits it as `acq.conc` (§2.4), for the strongest pick only; the pool
    does not read it — the list carries no per-peak concentration, and the
    pool keys its zone on the code axis alone (§8.2, §12.14).

**Pfa per block** (pure noise, D = 16, 21 tiles, 300 blocks): configured
0.10, realized **64 of 300 = 0.21**; configured 0.20 over 100 blocks, 37.
That is 1 − (1 − pfa)^2 to within a sigma both times: the slow-time axis is
interpolated twofold and the maximum runs over the interpolated surface
while the threshold's N counts native cells — **doppler#1064**, the open
finding on the burst engine, which the continuous engine inherits with its
slow-time axis. The CFAR counts every row of every tile (the cell count is
pinned in `test_acq_core.c`); the factor is #1064's, and the `--check`
pins the realized rate against `1 − (1 − pfa)^interp` so the finding cannot
be mistaken for a regression, or a fix for one.

**Sensitivity** (5 Mcps, one look, 20 trials, realized Pd and the mean
peak-to-gate ratio):

| D   | 30 dB-Hz   | 34 dB-Hz   | 38 dB-Hz   | 42 dB-Hz   |
| --- | ---------- | ---------- | ---------- | ---------- |
| 1   | 0.00 (0.8) | 0.00 (0.8) | 0.00 (0.8) | 0.00 (0.8) |
| 16  | 0.00 (0.9) | 0.00 (0.9) | 0.35 (1.0) | 1.00 (1.5) |
| 154 | 0.90 (1.2) | 1.00 (1.7) | 1.00 (2.7) | 1.00 (4.3) |

A single epoch over ±50 kHz detects nothing to 42 dB-Hz with one look;
D = 16 turns on between 38 and 42; **D = 154 is on at 30**. From 16 to
154 the knee moves about 9 dB for 9.8 dB of depth — the depth buys what
it says, less the straddle. What the operating point buys against the
epoch-by-epoch searcher of §12.1 is therefore not one number but the
whole gap between "never" and 30 dB-Hz at one look; and sized at the
45 dB-Hz of §12.6, the engine's own sizer buys **15 non-coherent looks
at D = 1 and one at D = 154** (measured on the same code and span). Its cost per epoch, and how the fan
across threads takes it, is §12 step 13.

### 12.8 What was measured (2026-09-05) — step 13, the searcher's cost with D, and the fan

`bench_acq_core` (`make bench`), on a 20-core Ryzen AI 9 465, minimum of
15 pushes; the searcher's cost in ns per output sample and as a multiple
of real time, the roll per thread (§2.3) at 1, 2, 4 and 8 threads:

| row (±50 kHz)                    | 1 thread        | 2          | 4              | 8          |
| -------------------------------- | --------------- | ---------- | -------------- | ---------- |
| 5 Mcps, 21 tiles, D = 1          | 174 (1.75×)     | 98 (0.98×) | 58 (0.58×)     | 50–60      |
| 5 Mcps, 21 tiles, **D = 154**    | **624 (6.2×)**  | 398 (4.0×) | **288 (2.9×)** | 258 (2.6×) |
| 5 Mcps, ±5 kHz, 3 tiles, D = 154 | 95 (0.95×)      |            | 53 (0.53×)     |            |
| 2 Mcps, 53 tiles, **D = 61**     | **2293 (9.2×)** |            | 892 (3.6×)     |            |

Four things this settles:

- **The fan works, and it is Amdahl's.** At `D = 1` four threads buy
    3.0× — about 88% of the work is in the tiles — and eight buy little
    more; the searcher over ±50 kHz that was 1.75× real time on one core
    is 0.58 on four. The pool's hand-off per push is not visible at this
    granularity (an epoch is 0.2 ms; the D = 1 rows are 254-epoch pushes).
- **The depth costs 3.6× per sample serially, and the transforms are not
    why.** The design estimated the slow-time transform "of the order of
    the epoch transform it sits behind" — it is, but the per-cell passes
    around it are not: the magnitude of every cell, the CFAR reference over
    the whole surface, the mask copy and the list's scans grow with the
    surface, 13.2 M cells per block at the operating point against 43 k per
    epoch at `D = 1`, and they run **after** the fan, serially. That is
    why four threads buy only 2.2× at `D = 154` (72% fanned) — and inside
    the fanned block-end loop the scatter folds a row index per **cell**
    and the column gather strides by `code_bins`. Both are named in
    [#1243](https://github.com/doppler-dsp/doppler/issues/1243) with the
    fix: the magnitude, the reference and the list per tile with a serial
    merge; a per-tile row table; a chunked gather.
- **Pre-compensation is worth what §12.1 said.** Over ±5 kHz the
    `D = 154` searcher is 0.95× real time on one thread and 0.53 on four.
- **The low chip rate is the worst case, by more than before.** 53 tiles
    of `D = 61` rows is the same 13 M cells per block for 61 epochs instead
    of 154, so the per-cell passes cost 2.5× more per sample: 9.2× real
    time on one thread, 3.6× on four. The fix above is what brings it in.

What the budget said (§6.4: 100 ns per output sample per core, half as
the margin): at the operating point the block searcher on four threads
was 288 ns of wall per sample, about 1150 core-ns — a quarter of a
48-core server's budget, beside twelve receivers at 44 each (§12.1). It
fit; it was not comfortable, and #1243 was the next thing to attack.

**#1243, measured the same day, same box, same rows.** The per-cell
passes now run per tile on the pool and merge serially in tile order;
the block-end scatter reads a per-tile row table instead of folding a
row index per cell; the column gather goes 32 columns at a time so a
cache line of the block serves eight columns instead of one. The surface
and the hits are byte-identical at any thread count, as before, with a
second emitter in the comparison so the list's second scan is part of it.

| row (±50 kHz)                    | 1 thread        | 2          | 4              | 8              |
| -------------------------------- | --------------- | ---------- | -------------- | -------------- |
| 5 Mcps, 21 tiles, D = 1          | 174 (1.74×)     | 97 (0.97×) | 59 (0.59×)     | 55–62          |
| 5 Mcps, 21 tiles, **D = 154**    | **523 (5.2×)**  | 283 (2.8×) | **164 (1.6×)** | 125–138 (1.3×) |
| 5 Mcps, ±5 kHz, 3 tiles, D = 154 | 80 (0.80×)      |            | 35 (0.35×)     |                |
| 2 Mcps, 53 tiles, **D = 61**     | **2010 (8.1×)** |            | 580 (2.3×)     |                |

- **The fan is now 92% of the work at `D = 154`** (four threads buy
    3.2×, eight 4.2×), up from 72%; the serial cost fell 16% (the row
    table and the chunked gather), the four-thread cost 43%. `D = 1` is
    unchanged to the nanosecond: its passes were 43 k cells per epoch,
    never the cost.
- **What is left is the transforms.** At the operating point the block
    is 21 tiles × 2046 columns of a 308-point slow-time transform plus
    154 × 21 inverse transforms of 2046 points; the passes around them
    are now a fraction of that on any thread count. 308 = 4·7·11 is not
    a smooth length; bounding `D` to a 5-smooth number below the window's
    is the one lever left in the engine, unmeasured.
- **The budget:** four threads at 164 ns of wall per sample is about
    656 core-ns — 14% of a 48-core server's budget at the operating point,
    beside twelve receivers at 44 each; over ±5 kHz, 0.35× real time on
    four threads. Comfortable. Bit-identity across thread counts is
    pinned in `test_acq_core.c` and the C suite runs under TSan.

### 12.9 What was measured (2026-09-06) — step 12, the tracker through the window

`native/validation/tracker_through_window.c` (`make validate-c`; its
`--check` is in the C suite): the hand-off receiver, seeded by the shipped
searcher's first hit on the same received blocks (the pool's own path,
§8.2), tracking one emitter from the shipped synth with its own window
(450 code-only symbols of every 4950 on the data clock: a 167 ms window in
a 1.83 s frame) at the operating point and the shipped `awgn`, fed one
epoch (0.2 ms) at a time, both lock flags read after every block, the
release clock at the design's 2 s. Once tracking with symbol lock held for
200 blocks, ten windows per trial, three trials per C/N0; per window the
fraction of blocks with each flag off, the longest both-off run, whether
`lost` fired, and the pull-in after the data resumes. Three
conditions: no Doppler, and SPEC's two worst cases through the shipped
`doppler_channel`, apart, since they do not coincide on a pass — the
offset (20 ppm of a 2.5 GHz carrier, 50 kHz, the chip clock dilated with
it) and the rate (500 Hz/s from zero).

| condition, C/N0 (Es/N0) | settled            | window blocks | code lock off | symbol lock off    | both off | release | pull-in after the window |
| ----------------------- | ------------------ | ------------- | ------------- | ------------------ | -------- | ------- | ------------------------ |
| static, 45 dB-Hz (10.7) | 3 of 3, 68 ms      | 24 435        | **0**         | **0**              | 0        | never   | none needed              |
| static, 40 dB-Hz (5.7)  | 3 of 3, 110–163 ms | 24 435        | **0**         | 1.5% (two windows) | 0        | never   | 6 ms                     |
| offset, 45 dB-Hz        | 3 of 3, 76–81 ms   | 24 435        | **0**         | **0**              | 0        | never   | none needed              |
| offset, 40 dB-Hz        | 3 of 3, 0.30–2.8 s | 24 436        | **0**         | **0**              | 0        | never   | none needed              |
| rate, 45 dB-Hz          | 3 of 3, 68–70 ms   | 24 435        | **0**         | **0**              | 0        | never   | none needed              |
| rate, 40 dB-Hz          | 3 of 3, 145–172 ms | 24 435        | **0**         | **0**              | 0        | never   | none needed              |

(Measured with the hand-over fix of #1249 below and the object's default
refine length. The first run took the offset and the rate together and,
before the fix, settled 2 of 3 at 45 dB-Hz and 0 of 3 at 40.)

What it settles:

- **The window costs the tracker nothing.** Code lock never drops in a
    window, at either C/N0, with or without the ramp: the symbol-aided
    detector's looks are as good on a constant symbol as on data. Symbol
    lock holds too — the phase-lock statistic `cos(2φ)` reads a constant
    symbol as locked, and the symbol clock coasts 167 ms without a
    transition and picks the data up with no pull-in at all (the one
    dip, at the floor, is the flag's own chatter: 3.6% of that frame's
    data blocks were off too, §12.3's 0.5% under a different seed). The
    release never fires. The expectation that the symbol flag "may drop
    and recover within its dwell" was pessimistic; nothing here needs a
    rule loosened or a detector fixed.
- **The pool must seed from the searcher, not from the truth.** The
    harness first seeded the hand-off receiver with the stimulus's own
    chip phase; through the channel the received code is five chips late
    (the resampler's delay), the code loop sat outside its pull-in and
    nothing downstream locked. `acq_build_handoff()` of the searcher's
    hit — measured on the received stream — is the seed, and with it the
    same trials lock in 70–280 ms.
- **What the ramp found was a hand-over defect, not a window one
    ([#1249](https://github.com/doppler-dsp/doppler/issues/1249)).** From
    the searcher's seed at 20 ppm with the 500 Hz/s ramp on top (the first
    run took SPEC's two worst cases together, which a pass never does) the
    chain settled in 2 of 3 trials at 45 dB-Hz and 0 of 3 at 40, the
    searching flavor alike. The
    refine → track hand-over re-seeded the live chain with the seed's code
    phase, rounded to whole code periods — zero net advance only on an
    undilated clock. At 20 ppm the code runs 100 chips/s ahead: 1.2 chips
    over the 12 ms refine at 45 dB-Hz, 5 over the 53 ms refine at 40, and a
    Dll seeded 1.5 chips off never pulls in; the trial that lost at 45
    dB-Hz was 1.45 chips off, the one that won 1.13. (The refine-stage
    Dll's own tracked phase is not the answer either: at the floor it
    wanders 13 chips over the same 53 ms.) The hand-over now advances the
    seed's phase by the refined Doppler's dilation over the refine's whole
    periods: 45 dB-Hz under the ramp settles 10 of 10 seeds in 70 ms, and
    `test_async_dsss_receiver_core` pins it at SPEC's 20 ppm with the
    floor's refine length, where the old hand-over was 5 chips off.
- **What remains at the floor is the offset case, and it is the estimate,
    not the window ([#1252](https://github.com/doppler-dsp/doppler/issues/1252)).**
    SPEC's two worst cases do not coincide — the largest Doppler is at the
    horizon where the rate is nil, the largest rate at closest approach
    where the Doppler is nil — so they are measured apart. The rate (500
    Hz/s from zero) is no problem: 10 of 10 seeds settle at both C/N0s,
    113–432 ms at the floor. The offset (50 kHz) settles 10 of 10 in 80 ms
    at 45 dB-Hz but 9 of 10 at 40, in 0.3–3.6 s, several past the 2 s
    release clock — the code loop locked from the first block, the carrier
    slow to follow. The refine lands the Doppler 200–460 Hz low on every
    seed with the floor's dwell (a bias, worse than the shortest dwell's
    ±20 Hz), and loop 1 — `bn` 0.04, 195 Hz at the code-period
    cadence, a measured pull-in bound of 60 Hz — then acquires it slowly
    or not at all, at 17 dB of loop SNR (15.5 after the squaring loss)
    where the rule wants 20. Narrowing the loop toward the rule is measured
    to cost the floor entirely (under the combined stress: `bn` 0.02, 10 of
    10 at 45 dB-Hz and **0 of 10** at 40; 0.01, 6 and 0), so the two rules
    conflict as built and the way out is the estimate the loop starts
    from. (This paragraph called the floor's dwell "18 blocks"; it is 7 —
    §12.10 measured the margin → dwell table. And §12.10 found the bias
    was the harness's own configuration, not the estimator's.)

______________________________________________________________________

### 12.10 What was measured (2026-09-06) — the refine's Doppler on its own

**Harness:** `native/validation/refine_bias.c` (`validate_refine_bias`;
`--check` is a ctest entry). A static capture from the shipped C stimulus
(`dp_dsss_capture`: Gold-1023 at 5 Mcps, 2700 sym/s async BPSK, a fixed
carrier offset of ±1500 Hz, AWGN from the C/N0), the hand-off receiver
seeded with the stimulus's chip phase and the truth plus a chosen error,
fed one epoch at a time until `get_tracking()` first reads 1, and
`get_doppler_hz()` read there. The error of that reading against the
truth, over 10 noise seeds per point: the mean is the bias, the standard
deviation the noise. Three axes — the seed's error (0, ±500, ±1100,
±2000 Hz) at the floor's dwell, the dwell through the design margin at a
fixed +1100 Hz, and the **despread stream** the refine estimates on:
`refine_max_error_db` sets the collection Dll's dumps per epoch through
`dll_lookback_segments()`, and the shipped default (0.5 dB, eleven dumps,
53.8 kHz) was compared with 100 dB (one dump, the 4.9 kHz epoch rate),
which `objects/async_dsss_receiver.toml` records as retired because a
stream below the 2700-baud data lobe's own width aliases any residual.

**The finding before the measurement:** every C harness and test of this
receiver — `tracker_through_window.c`, the hand-over test, all fourteen
`create()` calls in `test_async_dsss_receiver_core.c` — passed the retired
100 dB, positionally, and had since the default moved. The measurement
behind #1252 was taken on that stream. Both are now on 0.5 dB.

**Bias vs the seed's error, at 45 dB-Hz, margin 19 dB (7 blocks, 42 ms),
truth +1500 Hz** (mean ± sd over 10 seeds; the −1500 Hz rows are alike):

| seed error | shipped (11 dumps) | retired (1 dump) |
| ---------- | ------------------ | ---------------- |
| 0          | −11 ± 77 Hz        | −17 ± 45 Hz      |
| +500       | −16 ± 77           | **+140** ± 47    |
| −500       | −10 ± 76           | **−150** ± 61    |
| +1100      | −26 ± 77           | **+351** ± 115   |
| −1100      | +14 ± 90           | **−367** ± 125   |
| +2000      | −53 ± 79           | **+752** ± 91    |
| −2000      | +26 ± 70           | **−787** ± 99    |

**Bias vs dwell, at +1100 Hz** (the margin → dwell table is the same on
both streams; `det_n_noncoh()` sizes it from the derated C/N0 alone):

| margin | dwell    | shipped      | retired       |
| ------ | -------- | ------------ | ------------- |
| 14 dB  | 2 blocks | −31 ± 210 Hz | +341 ± 184 Hz |
| 17     | 4        | −17 ± 117    | +357 ± 189    |
| 19     | 7        | −26 ± 77     | +351 ± 115    |
| 22     | 18       | −21 ± 36     | +329 ± 87     |
| 25     | 55       | −20 ± 26     | +368 ± 95     |

- **On the retired stream the bias is a third of the seed's error, in the
    seed's direction, at every dwell.** ±0.35 × the error, the same with
    the truth at −1500 Hz, the same at 2 blocks and at 55: the refine
    removes two thirds of the residual and hands over the rest. That is
    the aliasing the manifest describes, not the template's edge and not
    the accumulation; and it is #1252's 200–460 Hz at the searcher's
    ~1.1 kHz seed. A two-pass refine on this stream would converge
    geometrically; the shipped stream does not need it.
- **On the shipped stream the estimate is unbiased** — within ±60 Hz at
    every seed error out to ±2000 Hz at the floor's dwell, inside loop
    1's 60 Hz pull-in — and its noise falls with the dwell as an average
    should (77 Hz at 7 blocks, 36 at 18, 26 at 55); at 40 dB-Hz the
    7-block point is −74 ± 152 Hz and 18 blocks give −59 ± 71. A residual
    −20 Hz survives long dwells; nothing downstream notices.
- **The floor's dwell is 7 blocks, not 18.** Margin 19 dB at 45 dB-Hz
    (the hand-over test's stand-in for 14 at 40) sizes 7; 18 needs 22 dB.
    The test's comment, #1252 and §12.9 all said 18.
- **What the shipped stream did at the floor was the searcher's seed,
    not the refine ([#1254](https://github.com/doppler-dsp/doppler/issues/1254),
    §12.11).** With the window harness on 0.5 dB, the offset case at 45
    dB-Hz settled 10 of 10 in 76–81 ms, as before; at 40 dB-Hz it settled 2
    of 10 at the shipped margin, 4 at 17 dB, 6 at 19 and 6 at 22 (122
    blocks, 0.7 s) — every failure a give-up, the refine's detector not
    firing within the dwell and the hand-over the unrefined seed, 1.1 kHz
    off, which loop 1 can never acquire. The same seeds failed at every
    dwell, so it was not the averaging; the frame's window was not it
    either (disabled: 1 of 10). On the retired stream the detector always
    fired, because the aliasing folds the whole data lobe into the band —
    which is why #1252 saw slow locks and not give-ups.

______________________________________________________________________

### 12.11 What was measured (2026-09-06) — the searcher's seed under the dilated clock

**Where the give-up came from.** The refine's detector statistic at the
floor's hand-over was noise-like on every failing seed (1.15–1.30 against
a 1.69 gate, the peak at a random lag), while at 45 dB-Hz it was 8.3 at
the right lag after two blocks. A static capture at 40 dB-Hz with an exact
seed fires 10 of 10 at 7 blocks, so two variables separated the stimuli:
the seed's chip phase and the dilation. The seed's phase alone, on the
static capture (`validate_refine_bias` with the seed offset, 40 dB-Hz, the
shipped stream): 0 and 0.25 chip off fire 10 of 10; 0.5 chip, 5 of 10;
0.75 chip and beyond, none — the refine Dll's pull-in is under half a
chip, and from a chip off it sits where it was seeded (its own lock
statistic reading 5–8 regardless). The searcher's seed's error through
the channel, measured at 45 dB-Hz where the live chain converges to the
truth: +0.05 chip on every seed; at 40 dB-Hz, on the one seed that
eventually settled, **+0.91 chip**.

**Why: the dwell's centroid.** The continuous searcher decides a hit on a
non-coherent sum over `n_noncoh` epochs — 15 at 45 dB-Hz (3.1 ms), 88 at
40 (18 ms) — and the code phase it reports is that sum's peak, the phase
at the *middle* of the dwell. The hand-off applies it at the dwell's
*end*. Under 20 ppm the code moves 100 chips/s, so the seed is late by
the drift over half the dwell: 0.15 chip at 45 dB-Hz (inside the
pull-in), 0.9 at the floor (past it, and exactly the +0.91 measured). The
same class as #1249, one hand-over earlier; and it is why the retired
1-dump refine "worked" — its Dll wanders chips across the code and crosses
the truth, and its aliasing folds the whole lobe into the band, so its
detector fired on a seed a chip off and handed over a biased estimate
instead of none.

**The fix.** `acq_build_handoff()` takes the RF carrier (0.0 = no
coupling) and advances the hit's phase by `doppler_hz_est / carrier_freq_hz × n_noncoh × coherent_bins × code_len / 2` chips — the
drift over half the dwell — folded with `dp_fmod_pos()`. The searching
receiver passes its own `carrier_freq_hz`; the hand-off flavor's holder
passes the same carrier it gives the receiver. The formula and its sign
are pinned in `test_acq_core` (positive Doppler, fast chip clock,
advances). Measured after it, the window harness's offset case at 40
dB-Hz on the shipped stream and margin:

|                    | before       | after                      |
| ------------------ | ------------ | -------------------------- |
| seed error (chips) | +0.91        | +0.16, −0.30, +0.29, −0.06 |
| settled, of 10     | 2            | **10**, in 114–205 ms      |
| 45 dB-Hz, of 10    | 10, 76–81 ms | 10, unchanged              |

`test_async_dsss_receiver_core`'s hand-over test now runs both operating
points — 45 dB-Hz with margin 19 and the 40 dB-Hz floor with the shipped
14 — and asks every seed to lock the code and decode; with the carrier
zeroed at the receiver's call site the floor's seeds go red and the 45
dB-Hz ones survive, which is why the floor is in the test. The rate case
and the static condition are unchanged.

**What this left open.** The block-coherent searcher (§12.7) had been
measured with the Doppler *rate* but never through a dilated chip clock:
at 20 ppm its D = 154 epochs (31 ms) coherent sum spans 3 chips of code
drift — §12.12 measured it and gave the engine the code-rate hypothesis
([#1256](https://github.com/doppler-dsp/doppler/issues/1256)); the carrier
now lives on the engine (`set_carrier_freq_hz`), one declaration for the
hand-off's advance and the block's alignment. And
`doppler.dsss.handoff.dll_init_chip_from_acq`, the Python lag → phase
helper for a hand-built Acquisition → Dll chain, restates
`acq_build_handoff()`'s fold and does not carry the advance —
[#1257](https://github.com/doppler-dsp/doppler/issues/1257).

______________________________________________________________________

### 12.12 What was measured (2026-09-06) — the block-coherent searcher under the dilated clock

**Harness:** the `dilated` section of `native/validation/acq_block_coherent.c`
(`--check` on ctest). One aligned block of a clean emitter at SPEC's
Doppler, 20 ppm of 2.5 GHz = 50 kHz, three ways: the synth's own carrier
offset with the code standing still; the synth at baseband through the
shipped `doppler_channel`, the chips dilated with it (100 chips/s at
5 Mcps); and the same, the engine told the carrier. Per depth D of 1, 16
and the window's 154: the peak against the gate, `conc`, the peak's
width along the code axis, and the hand-off's chip phase raw and with
the half-dwell advance. Then the depth's realized Pd, 20 trials per
C/N0, noise from the shipped awgn after the channel.

| D   | code                  | peak/gate      | `conc` | width    | drift/block |
| --- | --------------------- | -------------- | ------ | -------- | ----------- |
| 1   | still                 | 40 / 4.7       | 0.97   | 0.5 chip | 0           |
| 1   | dilated               | 34 / 4.7       | 0.98   | 1.0      | 0.02 chip   |
| 16  | still                 | 273 / 5.1      | 0.86   | 0.5      | 0           |
| 16  | dilated               | 256 / 5.1      | 0.87   | 0.5      | 0.33        |
| 16  | dilated, carrier told | 262 / 5.1      | 0.86   | 0.5      | 0.33        |
| 154 | still                 | 2065 / 5.4     | 0.85   | 0.5      | 0           |
| 154 | dilated               | **459** / 5.4  | 0.72   | **3.0**  | 3.15        |
| 154 | dilated, carrier told | **1462** / 5.4 | 0.85   | 1.0      | 3.15        |

| D = 154, realized Pd (mean peak/gate) | 34 dB-Hz       | 38 dB-Hz   | 42 dB-Hz   |
| ------------------------------------- | -------------- | ---------- | ---------- |
| still                                 | 1.00 (1.7)     | 1.00 (2.7) | 1.00 (4.2) |
| dilated                               | **0.00** (0.9) | 0.65 (1.0) | 1.00 (1.4) |
| dilated, carrier told                 | 1.00 (1.5)     | 1.00 (2.5) | 1.00 (3.9) |

- **Told nothing, the depth is gone at 20 ppm.** Across a D = 154 block
    the code drifts 3.15 chips; the coherent sum at any fixed lag sees the
    emitter for a third of the block, so the peak is 13 dB down and three
    chips wide, and at 34 dB-Hz — where §12.7's aligned floor detects
    every block — it detects none. D = 16 (a third of a chip) loses
    0.6 dB; a single epoch nothing. The pool's searcher, as measured in
    §12.7, had this loss hidden in it.
- **The code-rate hypothesis is the tile's own frequency.** A window
    tile at `signed_r` bins of `fs/nx` implies a chip clock dilated by
    `f_tile / carrier`, `signed_r × fs / carrier` samples of drift per
    epoch (0.041 at 50 kHz). `acq_tile_epoch` shifts each epoch's
    correlation along the code axis to the block's middle — a linear
    phase over the signed frequency index on the tile's product before
    its inverse transform, exact for a fractional shift, one complex
    multiply per bin — so the slow-time transform sums a standing peak.
    The sign was measured, not derived: the other one doubles the smear
    (6 chips, 247). No new search dimension: the hypothesis rides the
    tile.
- **Told the carrier, the block reads as a still one.** 1462 against
    2065 is 3.0 dB, of which 1.4 dB is the channel's own resampler
    (the D = 1 row shows it, and the peak's width of a chip instead of
    half), so the alignment leaves about 1.6 dB — the emitter sits a
    quarter row off its tile's centre, and a tile's hypothesis is one
    number for its ±2.4 kHz. `conc` is the still block's; Pd at 34 dB-Hz
    is 20 of 20 at 1.5× the gate against the still 1.7×.
- **The hand-off is the block's end within a tenth of a chip.** With the
    epochs aligned to the block's middle the peak IS the middle (raw
    385.00 against the D = 1 start of 383.50 plus 1.58), and the
    half-dwell advance of #1254 — `coherent_bins` was already in its
    formula — lands 386.57 against a truth of 386.65 at the block's end.
    Told nothing, the smeared peak's argmax wandered (385.50) and the
    advanced seed was 0.4 chip off, inside the refine's pull-in only by
    luck.
- **One declaration.** The carrier moved from `acq_build_handoff()`'s
    argument (#1258, the previous PR) to the engine:
    `acq_set_carrier_freq_hz()`, a jm method with a read-back property,
    drives both the block's alignment and the hand-off's advance. Config,
    not running state: it is not in the blob, so a resumed engine wants it
    set again by its holder. The searching receiver sets it from its own
    `carrier_freq_hz`; the hand-off flavor's holder sets it on the searcher
    it seeds from, as the window harness does.

______________________________________________________________________

### 12.13 What was built (2026-09-06) — the pool, §8.2

`native/src/async_dsss_pool/` (`AsyncDsssPool`, Python glue only): one
searcher, `n_slots` hand-off receivers created idle, the assigned table,
the event log by attachment, exactly the object of §8.2 — and nothing
about the waveform or the population baked in: 28 create parameters, every
default the operating point of §6.1, the searcher's and the receivers' own
passed through untouched. The carrier is told to the searcher as well as
the receivers (§12.11, §12.12), and the table's rows advance by the same
dilation while a receiver still refines, so the exclusion zone is keyed on
where the emitter IS. The composition serializes as a whole (its own
counters and the table, then the searcher's blob and every receiver's).

**What `test_async_dsss_pool_core` pins**, on the shipped C stimulus (one
capture per emitter, summed; the continuous engine at D = 1 over ±6 kHz;
pfa 1e-3; a 0.3 s release interval): one emitter takes exactly one slot
however many dwells hit it — the zone drops its own — with the seed at
its row and within the searcher's half-chip cell; it tracks with code
lock, the live Doppler converged within 100 Hz, the symbols readable by
slot; off the air it is lost and released, and back on the air it is a
new detection into a free slot; two emitters two rows apart hold one slot
each, both locked, with nothing dropped; a one-slot pool holds one of them
and counts the other dropped every dwell; every counted transition
reaches the log, seeded → tracking → lost → released in order; across
threads the assignments, the counts and every receiver's symbols are
bit-identical; and a mid-stream split resumes bit for bit in a fresh pool,
the envelope and a foreign slot count rejected.

Two things the build found:

- **The searcher's false alarms are part of the lifecycle.** At pfa 1e-3
    a noise peak seeds a free slot every few hundred milliseconds, the
    receiver refines to nothing, reports tracking on noise, and is
    released one interval later: a false alarm costs a slot for
    `lost_confirm_s`, which is the release headroom of §8.2's twelve
    slots for ten emitters. Every expectation is therefore about the
    emitter's slot, never an exact count of slots.
- **An unlocked carrier loop free-runs, and the table must not key on
    it ([#1261](https://github.com/doppler-dsp/doppler/issues/1261),
    fixed).** With the refine's shortest dwell (2 blocks at 47 dB-Hz and
    the shipped margin, ±200 Hz) a hand-over past loop 1's pull-in
    (measured: 1847 Hz for a 1500 Hz emitter) leaves the receiver
    code-locked — the code loop is non-coherent — with the carrier
    unlocked on every block (symbol lock down, `lock_metric` at zero) and
    loop 1 wandering 800 Hz in a second. That is not loop 2 decoding, as
    first read: it is a degraded receiver, and `degrade` is the event it
    logs. At D = 1 the zone is 4.9 kHz wide and nothing notices; at the
    pool's D = 154 it is 31.7 Hz, and a row keyed on the wander would let
    the searcher's next hit on the same emitter look new. Two fixes: the
    receiver's `status().doppler_hz` is the whole carrier estimate — loop
    1 plus what loop 2 has taken up beyond it, the sum
    `configure_chain_raw()` already re-seeds from — and the pool refreshes
    a row's Doppler only while `locked` holds and its chip phase only
    while `code_locked` does, keeping the last locked value (the seed's
    row, until the first lock) otherwise. `test_async_dsss_pool_core`
    pins it on the reproducing stimulus: the status wanders 843 Hz off
    with the carrier unlocked on 4858 of 4858 tracking blocks and the row
    stays the seed's; at pfa 1e-3 the same emitter locks and the row
    follows within 100 Hz.

**Next:** the lifecycle soak (§12 step 7) — the shipped synth with the
window, the block-coherent searcher at D = 154, several emitters at
random Dopplers through the channel, arrivals and departures — is the
measurement that certifies the pool. **Done, §12.14.**

### 12.14 What was measured (2026-09-06) — step 7, the lifecycle soak

`native/validation/async_dsss_pool_soak.c` (`make validate-c`; its
`--check` is in the C suite): per emitter one shipped continuous-DSSS
synth with the window (450 of 4950 symbols, Gold-1023 at 5 Mcps, 2700
sym/s PRBS BPSK) through the shipped `doppler_channel` at its own Doppler
drawn within ±20 ppm of 2.5 GHz, no rate, from a random burn-in of up to a
frame; visibility a gain of 1 or 0 at the sum; the shipped awgn at 45 and
40 dB-Hz. One `AsyncDsssPool` at the operating point — `code_only_epochs`
813 so D = 154 (31.7 Hz rows), 500 Hz/s, ±50 kHz, `max_peaks` 16, twelve
slots, the carrier told, a 2 s release interval, the machine's 20
threads — fed one epoch at a time with the event log attached. Emitter 0
always on; nine more with on-times uniform in 15–30 s and off-times in
4–8 s (the design's 5–15 minutes scaled thirty-fold; the pool's maximum
on-air time 35 s, so the always-on emitter is released for its on-time and
re-acquired). An emitter's slot is the one whose seed is at its code phase
within a chip of the synth's own clock through the channel's documented
mapping (`k(1+d) − delay`, plus the burn-in) and within one native tile of
its Doppler — a tile, not a row, for the first reason below. The `--check`
is two emitters for 16 s at 45 dB-Hz: the always-on one released for its
on-time and re-acquired, the other leaving, released, returning and
re-acquired.

**What the soak found before it could run — the zone (fixed).** The first
run filled all twelve slots in 0.4 s with one emitter on the air. A
tracked emitter's *data* blocks are §12.7's smeared copy — at 45 dB-Hz
still over the gate, C/N0 estimated at 30–33 dB-Hz, at the emitter's own
code phase within 0.3 chip and hundreds of Hz off in Doppler, a different
row every block — and the zone of §7.1, one row by one chip, is the
width of one emitter's *main lobe*, 31.7 Hz at this depth. Every such hit
looked new and seeded a fresh receiver onto the same emitter (each pulled
in and reported tracking); the true aligned hit, when its window came,
found no slot. The searcher's list carries no per-peak concentration to
tell the copy from a whole emitter, and a strong emitter's coherent tile
sidelobes at its own phase would read concentrated anyway; so the pool's
zone is now **the code axis alone** — a hit within one chip of a live
row's code phase is that emitter's own at any Doppler. Pinned in
`test_async_dsss_pool_core` with the cost stated: a second emitter within
a chip of a live one is not seen until the first leaves (a pair the
surface could not tell apart within a tile in any case, and the
searcher's own twin rule already holds a same-phase peak at any tile as
suspect). After the fix every seed of the two-minute runs was an
emitter's own or a noise false alarm; none a duplicate of a healthy
receiver's emitter.

**The run**, 43 stints per C/N0 (38 scored; the five cut by the run's end
before the design's own acquisition bound are not):

|                                                                     | 45 dB-Hz                   | 40 dB-Hz                    |
| ------------------------------------------------------------------- | -------------------------- | --------------------------- |
| stints missed                                                       | 0                          | 0                           |
| arrival → held, min / mean / max                                    | 0.00 / 0.04 / 0.17 s       | 0.00 / 0.24 / 1.24 s        |
| arrival → tracking, mean / max                                      | 0.05 / 0.18 s              | 0.29 / 1.30 s               |
| seed error, worst                                                   | 1296 Hz, 0.30 chip         | 4877 Hz, 0.29 chip          |
| held, of on-air blocks after first tracking                         | 0.9965                     | 0.9994                      |
| tracking with code lock, of held blocks                             | 0.9908                     | 0.9957                      |
| departure → release, min / mean / max                               | 2.02 / 3.07 / 6.81 s (24)  | 2.01 / 2.71 / 5.44 s (30)   |
| released later than the interval + 0.5 s, or not at all             | **23** of 38               | **18** of 38                |
| stints re-locked by their own receiver on return                    | 7                          | 1                           |
| false releases (`lost` while on the air)                            | **1**                      | **1**                       |
| double assignments (two receivers code-locked on one emitter)       | **1**, for 3.0 s           | 0                           |
| seeds matching no emitter                                           | 1                          | 11                          |
| hits dropped for want of a slot                                     | 2                          | 0                           |
| most slots assigned                                                 | 12                         | 12                          |
| log: seeded / tracking / degrade / lost / released (lost + on-time) | 49 / 49 / 47 / 33 / 33 + 9 | 57 / 57 / 255 / 43 / 43 + 6 |

- **Nothing is missed and the assignment is fast.** At 45 dB-Hz the
    emitter's own data-block copy seeds it in the first block or two
    (0.04 s mean, 0.17 s worst) — the window is not waited for; a
    receiver seeded from a copy hundreds of Hz off pulls in through the
    refine's range. At 40 dB-Hz the copy is under the gate more often
    than not and the window matters: 0.24 s mean, 1.24 s worst, under the
    frame the design allows. A seed at the tile's edge (4.9 kHz) still
    pulled in.
- **The release fires late — the open finding.** After a departure both
    flags drop within milliseconds, exactly as §12.3 measured, and then
    the *code* flag comes back for a block on noise about once a second
    (103 restarts of the release clock in the 45 dB-Hz run, 63 at 40,
    every one `code 1 sym 0`); `adr_release_clock()` restarts on either
    flag, so the interval runs from the last flicker and the release
    comes at 2.0–6.8 s for a 2 s interval, more than half of them past
    the interval plus half a second, and seven emitters at 45 dB-Hz came
    back inside their own receiver's overrun interval and were re-locked
    by it (the recovered assignment of §12.3, only unintended). §12.3's
    harness watched 1.5 s after switch-off and could not see it. The
    mechanism, read and not yet measured on its own: the Dll's lock look
    is the max over the symbol-scale windows of §3.7, and its threshold is
    sized for one cell at pfa 1e-3 with two verifies, so the false-lock
    rate on noise is the max's, not the cell's. The fix is the detector's,
    proven first in `async_dsss_receiver_release.c` with a longer watch
    on noise ([#1264](https://github.com/doppler-dsp/doppler/issues/1264)).
    **Fixed, §12.15:** the looks overlapped — a decision read the same
    noise `n` times; the threshold was right.
- **Two receiver failures in 76 stints.** At each C/N0 one receiver lost
    a healthy emitter and was released `lost` — a false release; at 45
    dB-Hz that receiver's carrier loop wandered 1.3 then 2.7 kHz off
    while its code flag stayed up and its Dll walked 150 chips off the
    code under the wrong aid, so the emitter's next aligned hit was a new
    seed and two receivers reported code lock on one emitter for 3 s
    until the first was released. The second seed is the recovery §10
    describes; the first receiver's code flag on a walked-off code is
    the same false-lock finding as above, seen with the emitter present
    ([#1265](https://github.com/doppler-dsp/doppler/issues/1265); fixed,
    §12.16 — the hand-over, not the receiver).
- **The pool holds.** Never past twelve; at 40 dB-Hz eleven noise seeds
    (0.09 per second against the configured 0.03 and #1064's 0.06), each
    refining to nothing, "tracking" with both flags down and released an
    interval later, as §12.13 said; two hits dropped at 45 dB-Hz while
    twelve slots held ten emitters and two departed ones inside their
    overrun intervals — this soak's churn is a departure every few
    seconds against the design's one a minute, and the headroom is the
    design's. The two hundred and fifty-five `degrade` events at 40 dB-Hz
    are the flags' chatter at the floor, none of it a release.

### 12.15 What was measured (2026-09-06) — the code flag on noise, #1264

**The mechanism was not the one §12.14 read.** The Dll's symbol-aided
detector (§3.7) keeps `Q = ⌈P⌉` timing hypotheses, eight here, each an
EMA of its own windows' power, and takes its looks from the best one. The
guess was that a threshold sized for one cell could not carry the max of
eight. Measured on pure noise with the Dll alone, configured exactly as
the receiver configures it (pfa 1e-3, `n_looks` from `det_n_noncoh` at
the design C/N0), the max costs almost nothing — the mean statistic is 8%
above √(2n) — and the tail is what breaks: **1.7e-2 exceedances per
decision at 45 dB-Hz and 4.2e-2 at 40 for a configured 1e-3, 0.65 and 0.5
false locks per second**, against 1.9e-3 and none for the unaided
detector on the same noise. The cause is the looks, not the threshold. On
a signal the best hypothesis holds and its windows are a symbol apart;
on noise it flips between neighbours whose windows share five of six
partials, and a decision's three looks then read the same noise three
times — a χ² of two degrees of freedom scaled by three against a gate
sized for six, whose tail at the gate is 2.4e-2. The receiver's rule
restarts its release clock on either flag (§10), so every such lock cost
an interval.

**The fix is one comparison in `aid_look()`:** a window that overlaps the
last look's is not a look (`aid_last_end`, running state, blob v10).
Nothing changes with a signal present. On noise: **2.1e-3 per decision
at 45 dB-Hz, 2.5e-3 at 40, 2.9e-4 at pfa 1e-4, and no lock in twenty
seconds** at any of them — the unaided detector's own realized rate,
which is #1064's factor of two over the configured one. Pinned twice:
`test_dll_core` 6b′ feeds the aided detector forty thousand epochs of
noise and asks the exceedance rate within four times pfa and no lock
(without the guard: 1.6e-2 and six locks);
`validate_async_dsss_receiver_release --check` now watches eight seconds
of noise after the switch-off — four release intervals — and asks that
neither flag return (without the guard: 6 and 11 returns in the two
trials). §12.3's table gains the row, 30 trials per C/N0:

| C/N0     | over 240 s of noise after the switch-off | code lock returned | symbol lock returned |
| -------- | ---------------------------------------- | ------------------ | -------------------- |
| 45 dB-Hz | 8 s × 30                                 | once (0.004 per s) | never                |
| 40 dB-Hz | 8 s × 30                                 | never              | never                |

The healthy-signal rows are unchanged: code lock off in no block of the
on-time at either C/N0, drop time 3.5 and 10.8 ms after a switch-off.

**The soak, re-run on it (§12.14's stimulus and scoring, ten emitters,
120 s at each C/N0):** departure → release **2.02 / 2.03 / 2.04 s** at
45 dB-Hz and 2.01 / 2.02 / 2.04 at 40 (was 2.0–6.8 s), no release late,
no false release, no emitter re-locked by a receiver that should have let
it go, one restart of a release clock in four minutes of noise (was 166),
and the check pins the interval plus half a second on every host again.
Nothing else moved: nothing missed, arrival → held 0.05 s mean at 45
dB-Hz and 0.30 at 40, tracking with code lock on 99.6% and 99.7% of held
blocks. One event is left, and it is
[#1265](https://github.com/doppler-dsp/doppler/issues/1265)'s: at 45
dB-Hz one receiver still walks off its emitter mid-stint while its code
flag holds, a second is seeded, and both report code lock for 1.6 s —
understood and fixed in §12.16.

### 12.16 What was measured (2026-09-06) — the hand-over that never pulled in, #1265

**Interference first, and ruled out.** The soak's trace now logs every
crossing of two on-air emitters' code phases within two chips, with their
Doppler difference and relative chip rate — the one way one emitter's
full peak reaches another's prompt correlator. Around the failing stint
(emitter 1's third, 55.7–77.5 s at 45 dB-Hz) every crossing was fast, 30
to 127 chips per second and under 0.3 s, with the other emitter at least
6 kHz away, none at the seed's moment; and no slow crossing (under 5
chips per second) happened anywhere in four minutes at either C/N0.

**The hand-over.** The event log had the number: seeded from a data-block
hit +594 Hz off, the receiver reported *tracking* 12 ms later with its
carrier estimate **−506 Hz** off the truth; symbol lock came and went,
the estimate wandered to −1.4 kHz by 61 s and −2.7 kHz by 75 s, while the
code loop stayed on the emitter's phase throughout (its chip rate was
emitter 1's 37.3 chips per second), and a second receiver was seeded
when a code-flag dip left the row's phase stale. §12.10 had measured the
refine at the pool's shipped margin — 14 dB at 45 dB-Hz sizes a
**two-block** dwell — at **−31 ± 210 Hz**: −506 is a 2.4σ draw, and one
such draw in 49 hand-overs is the 1.6% it predicts. The dwell is sized
by `det_n_noncoh` for *detection* at the derated C/N0, which needs fewer
blocks the higher the C/N0, while the estimate's noise the tracking
chain has to pull in from — a few hundred Hz — does not shrink with it.

**Confirmed by the wrong fix.** The soak with `--refine-margin 22` (18
blocks, 36 Hz): at 45 dB-Hz the double is gone and every stint holds
symbol lock (0.9925 of held blocks), tracking 0.14 s later. At 40 dB-Hz
the same margin derates to 18 dB-Hz and sizes one to four seconds of
dwell, during which the row's phase — advanced on the seed's Doppler
error, hundreds of Hz for a data-block seed, a chip a second — drifts
out of the zone; the next hit seeds a second receiver and 661 545
blocks read two receivers code-locked on one emitter. The lever is a
floor on the dwell in blocks, not a detection margin.

**The fix:** `refine_min_blocks`, default 7 (42 ms, §12.10's floor
dwell, 77 Hz), applied when the refine chain is built and clamped by the
give-up cap; `set_refine_min_blocks()` on the receiver and on the pool.
Pinned in `test_async_dsss_receiver_core`: the default receiver's dwell
at 45 dB-Hz reads 7, 2 with the floor removed (proven red without it),
the cap with a floor above it. The pool's #1261 reproduction removes the
floor explicitly, because with it the wander it pins never happens.

**The soak on it**, ten emitters, 120 s at each C/N0:

|                                           | 45 dB-Hz                        | 40 dB-Hz                  |
| ----------------------------------------- | ------------------------------- | ------------------------- |
| double assignments                        | **0** (was 1, 1.6 s)            | 0                         |
| false releases                            | 0                               | 0                         |
| tracking with symbol lock, of held blocks | **0.9968** (was 0.9910)         | 0.9841                    |
| arrival → tracking, mean / max            | 0.09 / 0.21 s (was 0.06 / 0.18) | 0.35 / 1.30 s (unchanged) |
| departure → release, min / mean / max     | 2.02 / 2.09 / 3.82 s            | 2.01 / 2.02 / 2.04 s      |
| stints missed                             | 0                               | 0                         |

The one 3.82 s release is §12.15's residual: the code flag still returns
on noise at 0.004 per second, and one return inside the interval restarts
the clock once — the rule's own worst case at that rate, two intervals,
which is what the soak now bounds (two returns inside one interval is a
1e-4 event per departure). The sweep is green at both C/N0s.

### 12.17 What was measured (2026-09-06) — step 7's duration and step 8's population: ten minutes, the heap, and the budget

**The run.** The soak of §12.14 with `--duration 600 --events`: ten
emitters for ten minutes at each C/N0, 192 stints each (187 scored; the
five cut by the run's end are not), on the machine's twenty threads.

|                                                                     | 45 dB-Hz                         | 40 dB-Hz                          |
| ------------------------------------------------------------------- | -------------------------------- | --------------------------------- |
| stints scored as missed                                             | **1** (the alias, below)         | **2** (the alias, twice)          |
| false releases (`lost` while on the air)                            | 0                                | 0                                 |
| arrival → held, min / mean / max                                    | 0.00 / 0.05 / 0.73 s             | 0.01 / 0.30 / 1.59 s              |
| arrival → tracking, mean / max                                      | 0.10 / 0.78 s                    | 0.35 / 1.64 s                     |
| held, of on-air blocks after first tracking                         | 1.0014                           | 1.0006                            |
| tracking with code lock, of held blocks; with symbol lock           | 0.9981; 0.9963                   | 0.9953; 0.9710                    |
| departure → release, min / mean / max (n)                           | 2.02 / 2.06 / 4.44 s (181)       | 2.01 / 2.03 / 4.45 s (181)        |
| releases past the interval + 0.5 s; absent                          | 3; **1** (the flag, below)       | 1; 0                              |
| double assignments (two receivers code-locked on one emitter)       | **1**, 0.14 s                    | **3**, 5.6 + 7.6 + 7.4 s          |
| seeds matching no emitter                                           | 2                                | 47                                |
| on-time releases; most slots assigned                               | 17; 11                           | 16; 12                            |
| log: seeded / tracking / degrade / lost / released (lost + on-time) | 213 / 213 / 188 / 187 / 187 + 17 | 255 / 255 / 1328 / 231 / 231 + 16 |

The population holds for ten minutes as it did for two: nothing false,
nothing dropped, the release at the interval in 360 of 362 departures.
Three things ten minutes found that two could not, each read from the
event log at its moment:

- **The tile-edge alias — a seed one tile off, and what it costs.**
    Emitter 3 sits at +12 288 Hz, 68 Hz inside the edge of a 4 888 Hz
    tile. Its eleventh stint (297.7–326.1 s) was seeded at **297.723 s at
    both C/N0s** from the row at **+7 394.9 Hz — exactly one tile low** —
    and at 40 dB-Hz its sixth (145.4 s) likewise, the same 7 394.9 each
    time: the neighbouring tile sees the emitter's energy folded to its
    own edge and the pick reported that tile's row. The refine pulled
    4.9 kHz in and the receiver tracked the emitter with both flags up
    (+12 285 Hz, C/N0 read 38 dB-Hz) until it left; the score, which
    accepts a seed within one tile of the truth (§12.14), called the stint
    **missed** and the seed one that matched no emitter — all three
    misses of the run, and the pool missed none of them. What the alias
    does cost is at 40 dB-Hz: the pool advances a live row's code phase
    on the seed's Doppler until the loop locks, and a Doppler a tile wrong
    is 9.8 chips per second of phase error, so the row leaves its own
    one-chip zone inside the refine's dwell and the next hit seeds a
    second receiver — 145.443 and 145.537 s, 94 ms apart, both at
    +7 394.9 Hz. One of the three 40 dB-Hz doubles (7.6 s, emitter 2 at
    the same +7 394.9) is this; §12.16's floor did not remove it because
    the seed is not noisy, it is wrong by a tile. The pick should report a
    peak in the tile that owns it, and the score should accept a tile's
    edge; filed as #1270 and fixed at the pick (§12.18). **The other two
    doubles (5.6 and 7.4 s) are not the alias**: emitters 1 and 7 were
    seeded from a data block 658 and 806 Hz off, and each receiver
    reported tracking 53 ms later at the seed's own frequency — the
    refine had not moved it — and held code lock without symbol lock
    (0.45 and 0.00 of its held blocks) until the emitter's next aligned
    hit seeded a second receiver, which did pull in. §12.16's hand-over
    that never pulled in, at 40 dB-Hz and a data-block seed, twice in
    187 stints (#1273).
- **One receiver's code flag on noise, 250 times the rate.** Emitter 1
    left at 432.41 s; its receiver (slot 9) reported both flags down at
    once, and then its code flag returned **six times in seven seconds**
    — 433.8, 434.1, 434.7, 435.4, 437.2 and 439.1 s, every one `code 1   sym 0` with the emitter off the air — each restarting the release
    clock, so the release never came: the emitter returned at 439.52 s
    into its own receiver (the seventh flicker, at 439.6 s, is the real
    lock), and the fresh seed the searcher made for it held a second slot
    for 0.14 s. §12.15's 0.004 per second is the population's mean over
    the run; this receiver ran at one per second for seven seconds, once
    in 181 departures. Filed as #1271.
- **The heap read +53 and +38 KiB over ten minutes, and nothing leaked
    — two accountings, neither the pool's.** The watch of §5.1 (a sample
    a second, re-based at each slot's first tracking) grew by 16 KiB in
    thirty seconds and 53 in six hundred, and every candidate measured
    0 B in isolation — the event log over a thousand events, a receiver
    through seed, track and reset. The first part is glibc's **tcache**:
    freed chunks kept per thread and counted by `mallinfo2` as in use,
    filling for as long as the run meets a size it has not seen; with
    `glibc.malloc.tcache_count=0` the same run reads +32 KiB, and the
    soak now re-execs itself once with that tunable. The second part
    was found by shape: the +32 came in two clusters, +11 KiB over
    225–240 s and +20 over 440–500 s, in steps of 1.2 then 2.4 KiB at
    seeds, flat for the 180 s between through forty seeds, identical to
    0.1 KiB between a 300 s and a 600 s run — and not the allocator's
    arenas either (counted through `malloc_info`: twenty from the
    warm-up on, none created later). It was **the harness's own stint
    records**, an array per emitter that doubled at the ninth and
    seventeenth stint, 8 then 16 records of about 150 bytes; ten
    emitters cycling every 19–38 s reach their ninth stint together near
    225 s and their seventeenth near 450 s. The records are now sized
    for the run before the base is taken, and the ten-minute run then
    reads **+1.6 KiB at most at 45 dB-Hz and +0.0 at 40** — §5.1's
    requirement, held at ten minutes (the run's remaining failures are
    #1270's and #1271's). The resident
    high-water mark is reported and no longer gated: the same stimulus
    read +1.7 MiB on one run and +0.0 on the next, and the 30 s and
    600 s runs settled at +2.5 to +5.9 MiB, no larger for twenty times
    the length — a process-wide number that moves with twenty workers'
    schedule, not with time.

**The budget, the population as one run (step 8).** `--budget --duration 60`: the summed stimulus carried to 13 MSa/s, the shipped `DDC` bringing
it back to two samples per chip on the arbitrary-ratio path §6.4 forces,
the pool taking the block, the time inside each measured — on this box's
twenty threads, nothing else running, ten emitters at each C/N0 and all
ten tracked (19 stints scored, none missed, the release at 2.02–2.04 s).

|                                                     | 45 dB-Hz  | 40 dB-Hz  |
| --------------------------------------------------- | --------- | --------- |
| inside the DDC, of real time at 13 MSa/s            | 0.32      | 0.32      |
| inside `push()`, of real time                       | 3.80      | 2.16      |
| the chain, of real time at 13 MSa/s                 | **4.12**  | **2.48**  |
| the same chain fed 2.31× faster, the 30 MSa/s floor | **9.51**  | **5.71**  |
| emitters tracked, of ten; stints scored; missed     | 10; 19; 0 | 10; 19; 0 |

**The requirement is missed by its own words** — under 0.5 at both was
the target, and the chain runs at 4.1× real time at the operating point
and 9.5× at the floor, on twenty threads of one box (§6.1's server has
48 cores; this is not that machine, and one process is not the
"processes as needed" of §1.1). The stage that owns the excess is
named by §12.8: the block searcher at `D = 154` is 2.6–2.9× real time on
four to eight threads, most of the pool's 3.8, and its per-cell passes —
the mask copy, the list's scans, the column gather striding by
`code_bins` — are where §12.8 says the depth's cost is; the receivers
are the rest, and the front end is the 0.32 the DDC's serial path costs
here (§12.1's bench priced it 0.18 of a core at the minimum of rounds;
this is a run's mean). Two things this run does not settle: the pool
costs 1.8× more of real time at 45 dB-Hz than at 40 for the same
population and the same count tracked, unattributed here; and the number
is one box's — the fraction scales with the threads the searcher's roll
is given (§12.8's fan), so what the server does with 48 is a measurement
on the server. Step 8's next thing to attack is the searcher's depth.

______________________________________________________________________

### 12.18 What was measured (2026-09-07) — the tile-edge alias, #1270

**The ambiguity, measured on the pool's grid** (one emitter through the
channel, code-only, the carrier told, one look per block, at 45 dB-Hz;
`validate_acq_block_coherent`'s edge run and a probe on its stimulus):

| emitter Doppler       | position       | hand-offs a tile off, of 57 | true row over the aliased row |
| --------------------- | -------------- | --------------------------- | ----------------------------- |
| 12 220 Hz             | on the edge    | **27**                      | +0.00 dB                      |
| 12 288 Hz (emitter 3) | 68 Hz inside   | **10**                      | +0.03 dB                      |
| 12 400 Hz             | 180 Hz inside  | 0                           |                               |
| 12 100 Hz             | 120 Hz outside | 0                           |                               |

The surface holds the two hypotheses at the same row index of adjacent
tiles and cannot tell them apart: each tile is the shared epoch spectrum
rolled by whole bins of one span (4 888 Hz), so an edge emitter carries
half a span of residual inside the epoch in both neighbours, the same
sinc loss to 0.03 dB, and the slow-time transform folds the rest modulo
the epoch rate. The band is about ±100 Hz around each of the twenty
edges. Emitter 3's seeds in §12.17 were three draws of this one in six.

**The fix, and its proof.** The pick is now asked at the row's frequency
(§2.3, `acq_resolve_tile_alias`): three hypotheses per listed peak on the
block's raw epochs. With it, none of 59 decided blocks is a tile off at
either C/N0, on the edge or 68 Hz inside, the worst error 5 Hz. The
code-rate walk's sign was checked by flipping it: at ±46 436 Hz (the
edge nearest ±50 kHz) and 36 dB-Hz the flipped walk hands off 13 of 29
a tile away, the shipped one none; at 40 dB-Hz 2 of 29 against none.
Data-only blocks (PRBS through the whole block) hand off one or two picks
in forty, 20 kHz — four tiles — from the emitter: §12.7's smeared copies,
not this mechanism, and a ±1 tile test does not move them. The blob
grows by the raw block (5% at `D = 154`), version 4, so a mid-block
resume decides as the unbroken run would; `test_acq_core`'s split found
that before the blob carried it.

**The ten-minute soak on it** (the same stimulus as §12.17): no stint
scored missed at either C/N0 (was 1 and 2); the 45 dB-Hz half otherwise
identical to the block, #1271's receiver included; at 40 dB-Hz the
doubles fall from 20.6 s of shared code lock to 13.0 s — the alias's
7.6 s gone, the two data-block seeds that never pulled in (#1273)
untouched, as they should be.

______________________________________________________________________

### 12.19 What was measured (2026-09-07) — the receiver that followed a neighbour, #1271

**The mechanism, read from the trace.** After emitter 1 left at 432.41 s
of the 45 dB-Hz soak, its receiver's code phase advanced at 44.8 chips
per second where it had tracked at 37.3, its both-down clock reset every
20–130 ms, and every one of its sixteen code-flag returns sat within
0.5 chip of slot 0's live phase — emitter 0, always on, 3.8 kHz above
its carrier, whose chips run at exactly 44.8 per second. The
free-running code loop had captured the neighbour's code and followed
it, the code flag flickering on a carrier 3.8 kHz wrong. Departed
receivers in general free-ran at up to ±90 chips per second (median
+40) — every one of them sweeping through every live emitter's phase —
and this one happened to meet one crossing slowly enough (7.5 chips per
second relative) to pull in.

**The fix: the loops hold (§10).** Once locked, both flags down hold
both loops at the state marked with both flags up, the detectors still
looking; one flag down is a degrade and the loops run. Three things the
first cuts got wrong and the harnesses caught: a hold taken at the
flag's drop keeps the 23 ms of noise-driven updates before it (+2.4
chips per second); a hold of the last steer's full output keeps its
proportional term — a phase correction for one interval — as a rate (+14
chips per second), so the hold is the filter's integrator alone, marked
while both flags are up; and a code loop held on the symbol flag alone
cannot ride out two live emitters crossing each other's code phase —
the ten-minute soak on that cut had emitter 8's receiver both-down 0.8 s
before its emitter left and emitter 4's doubled, where the run before
it had neither. The harness's `cross-on` event (475 Hz off, 0.95 chips
per second, the emitter staying on) measures it: held on the symbol
flag, both flags stay down for the remaining 7.7 s of the watch in every
settled trial at both C/N0s; held on both flags, over thirty trials code
lock holds in 25 of 30 at 45 dB-Hz (five dips of 155 ms) and 29 of 29 at
40, the flags are never down together for more than 5.5 ms, and every
receiver has both flags at the end — a few on the neighbour rather than
their own (mean rate +0.25 and +0.13 chips per second against the
neighbour's 0.95): two equal signals within half a chip and 475 Hz are
one peak to the loops, #1275's regime, not the hold's.

**Proven on the release harness's new `cross` event**: a second emitter
through the channel at CROSS_PPM of the carrier, on the air throughout,
its burn-in chosen so its code phase meets the departed receiver's
0.6 s after the switch-off; measured, the code flag's returns over the
8 s watch and the receiver's chip rate over the watch's last two
seconds (a receiver that follows runs at the neighbour's rate):

| neighbour                | coast: returns (3 seeds) | coast: rate      | free-running: returns | free-running: rate         |
| ------------------------ | ------------------------ | ---------------- | --------------------- | -------------------------- |
| 3.8 kHz off, 7.6 chips/s | 0, 0, 0                  | 0.00             | 1, 0, 1               | drifting, 2–6 chips/s      |
| 2 kHz off, 4 chips/s     | 8, 8, 10 (the crossing)  | **0.00**         | **232, 241, 252**     | **4.00 — the neighbour's** |
| 1 kHz off, 2 chips/s     | 6, 0, 14                 | 0.00, 2.00, 0.00 | 1, 0, 3               | 2.00 — the neighbour's     |
| 500 Hz off, 1 chip/s     | followed, symbol lock    | 1.01             | followed, symbol lock | 1.01                       |

The `--check` pins the 2 kHz row (a receiver that does not follow, at
most twenty blips; red at 232 and 4.00 without the coast). The last two
rows are #1275: within a kilohertz and a chip or two a second the
carrier loop pulls the neighbour in and to the receiver it is a return;
of the order of 1e-3 per departure with ten emitters over ±50 kHz, and
the pool's to tell apart. Over the harness's thirty trials per event the
hold costs nothing §12.15 measured: at 45 dB-Hz the switch-off's code
flag returns **0 times in 240 s** of noise (was 0.004 per second), the
fades come back 30/30 and 29/30, the phase step holds code lock 30/30;
at 40 dB-Hz one return in 240 s, the fades 30/30. The crossing at 2 kHz
and 4 chips per second blips the code flag about seven times per pass
and is followed on one trial in 28 at 45 dB-Hz and one in 29 at 40 — the
tail of the same band #1275 names.

**The ten-minute soak on it** (the stimulus of §12.17): at 45 dB-Hz no
stint missed, no false or absent release, **no double** (was one of
0.14 s), the release 2.02 / 2.04 / 4.46 s over 183 departures with one
restart of the clock in the whole run (was sixteen on one receiver), and
the #1271 receiver released 2.0 s after its emitter left; at 40 dB-Hz the
two #1273 doubles and nothing else, the release 2.01 / 2.03 / 4.45 s. The
cut that held on the symbol flag alone had, on this same stimulus,
emitter 8's receiver both-down 0.8 s before its emitter left and a new
0.15 s double on emitter 4 at the live crossing of §12.19's third lesson;
the rule as shipped has neither.

______________________________________________________________________

### 12.20 What was measured (2026-09-08) — the searcher's surface as a code tracker

**Harness:** `native/validation/acq_surface_jitter.c` (`make validate-c`;
its `--check` is in the C suite), beside `dll_aid_jitter.c` and in its
shape. The question it answers came out of the cancellation discussion:
the searcher computes every emitter's code phase and Doppler at full
price every dwell, and each hand-off receiver then re-derives the same
position with its own DLL. The surface's cells sit on the DLL's own
half-chip grid, so the normalised `(L − E) / (L + E)` at an emitter's
cell *is* the DLL's discriminator, integrated over a 31.5 ms dwell instead
of an epoch. If it reads the code phase as well as the loop does, the
searcher can own the position and a receiver keeps only what lives at
symbol rate.

**Method.** The pool's own engine — ±50 kHz, 21 tiles, D = 154 with the
carrier told, `interp` 2, so 6468 rows of 15.9 Hz — fed one emitter from
the shipped synth at the operating point (PRBS data, the 450-of-4950
code-only window) through the shipped `doppler_channel` at 0 and 18 ppm
of 2.5 GHz, noise from the shipped `awgn` after the channel. Its surface
is read through the sink every decided dwell **at the cell nearest the
truth**, the cell a locked tracker would be holding, so the number is
the discriminator's noise and not a detector's; the truth is the synth's
clock through the channel's mapping at the dwell's middle (§12.11). The
discriminator's characteristic across the cell (its S-curve, ten bins)
and the truth's constant are calibrated once on a clean, code-only,
dilated sweep and inverted per dwell. Dwells whose block lies inside the
window are scored apart from dwells under data; under data the tile's
rows are summed as power (Parseval: the per-epoch non-coherent sum,
which the surface still holds when the coherent peak is spread across
the rows). 800 dwells per cell, 59 of them in the window.

**Calibration, clean at 18 ppm, 200 dwells:** the surface's chip axis
is the truth's mapping to −0.003 chip with no drift (0.00016 chip per
dwell over the sweep); the S-curve is monotone across the cell, its
slope through the origin −0.314 chip per unit on the truth's row and
−0.208 on the tile summed (the sign is the column convention; a
triangular chip would give 0.5, and the channel's resampler widens the
pulse to a chip, §12.12); inverted on its own sweep it reads the truth
to **0.0011 chip RMS**. So the discriminator's shape is not the limit.

**Per-dwell error at the truth's cell** (chips; 59 window and 741 data
dwells per row; the DLL column is §12.5's closed-loop per-epoch jitter):

| C/N0, ppm | window: E/L bias, σ | window: Doppler σ | data: tile-sum bias, σ | data: argmax within a chip | DLL, §12.5 |
| --------- | ------------------- | ----------------- | ---------------------- | -------------------------- | ---------- |
| 45, 0     | −0.002, **0.015**   | 0.2 Hz            | −0.170, 0.008          | 100%                       | 0.013      |
| 45, 18    | −0.006, **0.015**   | 0.4 Hz            | −0.003, **0.085**      | 100%                       | 0.013      |
| 40, 0     | −0.006, **0.026**   | 0.3 Hz            | −0.213, 0.009          | 57%                        | 0.021      |
| 40, 18    | −0.007, **0.026**   | 0.8 Hz            | −0.002, **0.116**      | 57%                        | 0.021      |

The three-point parabola on the same cells, the calibration-free
alternative, reads 0.26 chip in the window and half a chip at 0 ppm: the
pulse is not a parabola, and an interpolated peak without its S-curve is
not a code phase. The Doppler rows carry a +3.7 Hz bias at 0 ppm that is
absent at 18 ppm, a quarter of a row, not explained here.

What it settles, and what it leaves:

- **In the window the surface reads the code phase as well as the DLL,
    and the Doppler with it.** 0.015 against 0.013 at 45 dB-Hz and 0.026
    against 0.021 at 40, and these are single-dwell reads with no loop
    behind them, where the DLL's numbers are closed-loop at `bn 0.002`
    (about 50 ms of averaging); a filter over two dwells sits under the
    loop. The Doppler comes with it at under a hertz, which the DLL does
    not give and the receiver's carrier loop exists to find. The idea has
    legs where the searcher is coherent.
- **Under data the truth's row is empty and the tile's sum is the
    read.** A data-modulated block spreads the emitter across its tile's
    rows (§2.3), so the coherent discriminator reads 0.1 chip of noise;
    the rows summed as power read the phase at 0.085 chip per dwell at
    45 dB-Hz and 0.116 at 40, unbiased over a sweep. Between windows the
    code drifts at a known rate — the row's Doppler to a hertz is 0.002
    chip per second of rate error — so a tracker dead-reckons on the
    Doppler and corrects on these reads; over the 1.66 s between windows
    that is 53 of them.
- **The tile-summed S-curve is not one curve.** At 0 ppm, with the truth
    held at one sub-cell offset, the tile's sum reads 0.17 and 0.21 chip
    biased with the S-curve calibrated at 18 ppm, at 0.008 of scatter: the
    summed pulse's shape moves with the residual drift the tile's own
    code-rate hypothesis leaves, which is zero at 0 ppm and up to a
    quarter chip per block at a tile's edge. A tracker on this read needs
    the S-curve at its working drift, or a read the drift does not shape —
    the engine holds the block's raw epochs, and a prompt re-summed at
    the tracked rate is one. Open.
- **A detector would lose the emitter between windows at the floor.**
    The surface's own maximum is the emitter in every window dwell, and
    under data in 100% of dwells at 45 dB-Hz but **57%** at 40: a tracker
    that re-found the peak each dwell would leave the emitter 43% of the
    time at the floor. The cell has to be held, which is what a
    discriminator is for.
- **Not measured here:** a closed loop (no loop is built; these are the
    measurement's noise and bias, which bound what a loop can do), the
    symbol-rate detector a receiver would keep, pull-in, and two emitters
    in one tile.

______________________________________________________________________

### 12.21 What was measured (2026-09-08) — the coherent discriminator on the engine's complex intermediates

**What was built.** Three reads of what the block-coherent engine
already holds per cell before a magnitude is taken, declared on the
manifest so the Python face follows, each a copy in the engine's own
size and 0 where the engine has none: `acq_surface_complex()`, the
coherent dump the last dwell was decided on (a non-coherent dwell is a
power sum and reads 0); `acq_block_prompt(tile, col)`, one cell's column
of the last whole block — the D per-epoch complex correlations at that
code phase, rolled to the tile's centre and shifted to the block's
middle by the tile's code-rate hypothesis, the despread stream at epoch
rate; `acq_block_raw()`, the block's D epochs as pushed, for a
re-correlation at any phase, rate or symbol boundary the engine's grid
does not have. `test_acq_core` pins the modulus against `mag_buf`, the
maximum against the reported cell, the raw block against the pushed
samples, the prompt's column as the largest over every column of the
tile, and 0 on a partial block and out-of-range indices; the column and
raw sabotages were watched go red.

**What was measured.** §12.20's harness, `acq_surface_jitter.c`, with
three reads added at the same cell, the truth's: the coherent
discriminator `Re(conj(P) (L − E)) / |P|²` on the complex surface's
truth row, calibrated and inverted through its own S-curve like the
magnitude one; the same formed per epoch from the block's prompt column
at the three cells and summed over the block, `Σ Re(conj(P_k)(L_k − E_k)) / Σ |P_k|²` — data-invariant, since an epoch's three cells carry
the same symbol — which is the read under data; and a linear phase fit
along the prompt column, de-rotated first by the Doppler a tracker
holds, unsquared in the window and on P² under data. 800 dwells per
cell, 59 in the window; the clean calibration reads the coherent
S-curves to the same 0.001 chip as the magnitude one.

**Per-dwell error at the truth's cell** (chips; the DLL column is §12.5's
closed-loop per-epoch jitter; the magnitude reads are §12.20's):

| C/N0, ppm | window: magnitude E/L σ | window: **coherent** σ | data: magnitude tile-sum σ | data: **per-epoch coherent** bias, σ | DLL   |
| --------- | ----------------------- | ---------------------- | -------------------------- | ------------------------------------ | ----- |
| 45, 18    | 0.015                   | **0.012**              | 0.085                      | −0.004, **0.031**                    | 0.013 |
| 40, 18    | 0.026                   | **0.021**              | 0.116                      | −0.003, **0.065**                    | 0.021 |
| 45, 0     | 0.015                   | 0.012                  | 0.008 (biased −0.17)       | −0.040, 0.010                        | 0.013 |
| 40, 0     | 0.026                   | 0.021                  | 0.009 (biased −0.21)       | −0.095, 0.015                        | 0.021 |

The Doppler from the de-rotated phase fit reads the rows' own number in
the window at 45 dB-Hz (0.5 Hz against 0.4) and 30 Hz under data; at 40
dB-Hz the fit's unwrapping fails at 3 dB of per-epoch SNR (33 Hz in the
window against the rows' 0.8), so the rows are the Doppler estimator
and the fit is not. The fit's residual is 0.33 rad per epoch in the
window at 45 dB-Hz: the carrier phase noise a symbol detector would see
per epoch, before any symbol-length integration.

What it settles, and what it leaves:

- **The coherent read is the better one everywhere, and under data it
    is the read.** In the window it takes 16 to 18% off the magnitude
    jitter, to 0.012 at 45 dB-Hz against the DLL's 0.013, per single
    dwell with no loop. Under data the per-epoch coherent discriminator
    reads 0.031 chip per dwell where the magnitude tile-sum read 0.085,
    unbiased over a sweep — the quadrature noise the magnitude kept is
    gone, and the epochs sum with their signs intact.
- **What separates it from the loop under data is the epoch, not the
    read.** 0.031 per 31.5 ms dwell is 0.025 at the DLL's averaging, twice
    the loop's 0.013. The DLL's discriminator runs on the symbol-aided
    window, six of a symbol's 7.24 partials with the transition partial
    left out (§12.5); the epoch-dot runs on whole epochs, and at 1.83
    epochs per symbol 55% of them straddle a transition and partly cancel
    (§12.2). The block's raw samples are now exposed for exactly this: a
    symbol-aligned re-correlation on `block_raw` at the tracked phase is
    the DLL's own window, on the searcher's timing, and it is the next
    number to take.
- **The normalisation is SNR-dependent, as a normalised discriminator's
    is.** At 0 ppm, with the truth held at one sub-cell offset near the
    cell's edge, the epoch-dot reads 0.04 chip biased at 45 dB-Hz and 0.10
    at 40, with 0.01 of scatter: `Σ |P_k|²` carries the noise power, so the
    gain calibrated clean shrinks with C/N0 and the read falls short of
    the offset. Over a sweep the bias averages out and hides in the σ. A
    tracker wants the gain at its C/N0 — the engine's own `noise_est` is
    the correction — or an unnormalised loop whose gain is set by the
    prompt's tracked amplitude.
- **The window's Doppler belongs to the rows; the phase belongs to the
    column.** The rows read the frequency to under a hertz at both C/N0s
    (§12.20); the column's phase fit matches them only where the
    per-epoch SNR lets it unwrap. What the column uniquely gives is the
    phase itself, 0.33 rad per epoch at 45 dB-Hz.
- **Not measured here:** a closed loop, the symbol-aligned re-correlation
    on the raw block, the detector's decisions, two emitters in one tile.

______________________________________________________________________

### 12.22 What was measured (2026-09-08) — the symbol-aligned re-correlation on the raw block

**What was built.** The read §12.21 said was owed: the DLL's own
symbol-aided window, on the searcher's timing, run over the block's raw
samples. No new correlator — the shipped `Dll`, its loop held
(`set_coast`), the dilation as a rate aid from the held Doppler, the
symbol window on, fed the block wiped of the held Doppler by the shipped
`LO`, as a receiver's Costas wipes it before its DLL, and **put at the
cell's phase at every block's start**. That last needed one primitive the
loop did not have, `dll_set_code_phase(chips)`: the NCO moved, nothing
else touched, the other half of the coast for a holder on another clock.
It also found two defects on the coast itself, fixed here: `steer()`
returned before computing the discriminator, so a coasting loop's `.e`
probe read 0 for as long as it coasted; and the header's `dll_update()`,
the `segments == 1` path and a second copy of the same discriminator and
steer, had no hold at all and kept steering with `coast` set. Both hold
after `last_error` now, pinned in `test_dll_core`; the duplication is
[#1280](https://github.com/doppler-dsp/doppler/issues/1280).

**Two things the build measured on the way.** A coasting loop drifts on
its 32-bit NCO's quantisation of the rate aid: **−1.88 chips per second**
at 18 ppm, 0.06 chip per block, so the correction is per block and not
per second. And the DLL's 256-chip partials integrate 2.3 cycles of the
emitter's 45 kHz carrier when the block is not wiped, a 17 dB loss a
clean stream survives and a noisy one does not: un-wiped, the running
loop read locked 0% at 45 dB-Hz with its discriminator scattering to
the clamp, and its converged phase sat 2.7 chips from the cell; wiped,
it locks 100% of the time and converges 0.002 chip from the cell at both
C/N0s, which is the receiver's own case and the harness's check that its
loop is the receiver's before the coasting read is scored.

**The measurement.** The S-curve of the held loop's discriminator over
its seed offset, ten clean runs across the cell on the data stream:
monotone, zero at +0.03 chip, 1.6 per chip, 0.04 of scatter within a
block. Then, at every dwell of §12.20's runs, the loop put at the truth's
cell plus 0.1 chip at the block's start, fed the wiped block, and the
mean of its discriminator over the block inverted through the curve:

| C/N0, ppm | data: coasting DLL bias, σ | data: per-epoch coherent σ (§12.21) | data: magnitude σ (§12.20) | window: coasting DLL σ | DLL closed loop (§12.5) |
| --------- | -------------------------- | ----------------------------------- | -------------------------- | ---------------------- | ----------------------- |
| 45, 18    | +0.003, **0.014**          | 0.031                               | 0.085                      | 0.015                  | 0.013                   |
| 40, 18    | −0.010, **0.026**          | 0.065                               | 0.116                      | 0.025                  | 0.021                   |
| 45, 0     | +0.046, 0.013              | 0.012 (biased −0.05)                | 0.008 (biased −0.17)       | 0.012                  | 0.013                   |
| 40, 0     | +0.026, 0.025              | 0.018 (biased −0.11)                | 0.009 (biased −0.21)       | 0.023                  | 0.021                   |

What it settles:

- **The searcher-timed tracker reads the code phase under data as well
    as the loop does.** 0.014 chip per 31.5 ms block at 45 dB-Hz and
    0.026 at 40, against the closed loop's 0.013 and 0.021 — the same
    window, the same discriminator, on the searcher's timing instead of
    the loop's, with no loop closed and the phase put back once a block.
    The 0.031 the per-epoch coherent read left on the table (§12.21) was
    the epoch straddling a symbol, as said; the symbol window recovers it.
- **What the tracker is, then.** A searcher that lists and holds cells,
    and per held cell: a coasting DLL corrected once a block from the cell
    and reading between, fed the block wiped at the row's Doppler, whose
    prompt at symbol rate is the detector's input. No refine, no hand-off,
    no lock detector deciding presence, no loop pulling in from a seed.
    Every part of it is a shipped object; what §12.20 to §12.22 measured
    is that the parts read as well as the loop they replace.
- **The 0 ppm bias is the S-curve's, not the read's.** Calibrated at 18
    ppm, where the held loop drifts 0.06 chip across a block, the curve's
    zero carries half that; at 0 ppm the aid is zero, the NCO is exact
    and nothing drifts, and the read sits 0.03 to 0.05 chip off. A
    tracker calibrates at its working drift, or corrects the aid's
    quantisation once it is measured.
- **Not measured here:** the correction closed on the searcher's cell
    rather than the truth's (§12.20's argmax holds the emitter in every
    window dwell and under data at 45 dB-Hz; at 40 the cell must be held,
    and this read is what holds it), two emitters, and the symbol detector
    on the prompt.

______________________________________________________________________

### 12.23 What was measured (2026-09-08) — the correction closed on the searcher's own cell

**What was built.** §12.22's read, with the truth taken out of the loop.
`acq_surface_jitter.c` gains a held mode: the tracker acquires at the
first window dwell from the surface alone — the argmax cell, the
calibrated E/L on its row for the phase within the cell, the parabola
over the rows for the Doppler, which is also the code rate since the
chips dilate with the carrier — and from there dead-reckons the held
phase across each block on that rate, puts the coasting DLL at it plus
the seed offset, feeds the block wiped at the held Doppler, and corrects
the held phase by the whole of what the loop read, once the symbol aid
has settled (six blocks). The DLL and the wipe open at acquisition; the
truth is consulted only to score, as the phase the tracker held at each
block's middle. The calibration constants (`c0`, the DLL's convention,
the S-curves) are the clean sweep's, as a tracker would carry them.
800 dwells per row, 25 s; the same seeds' truth-cell runs sit beside
them.

**The measurement** (chips; the truth-cell column is §12.22's read on a
fresh seed, the DLL column §12.5's closed loop):

| C/N0, ppm | acquired: chips, Hz from the truth | data: held phase bias, σ | window: held σ | worst error | left the cell | truth-cell read σ    | DLL   |
| --------- | ---------------------------------- | ------------------------ | -------------- | ----------- | ------------- | -------------------- | ----- |
| 45, 18    | +0.006, +0.5                       | −0.004, **0.014**        | 0.013          | 0.046       | never         | 0.014                | 0.013 |
| 40, 18    | +0.011, +0.7                       | +0.012, **0.027**        | 0.025          | 0.099       | never         | 0.026                | 0.021 |
| 45, 0     | +0.017, +3.6                       | −0.040, 0.014            | 0.020          | 0.074       | never         | 0.013 (biased +0.05) | 0.013 |
| 40, 0     | +0.027, +3.5                       | −0.026, 0.026            | 0.028          | 0.089       | never         | 0.025 (biased +0.03) | 0.021 |

Under data at 40 dB-Hz the surface's own maximum was the emitter in 58%
of dwells (§12.20's 57%); the held tracker was within a tenth of a chip
in every one of them.

What it settles:

- **Closed on its own cell, the tracker holds the emitter at the loop's
    jitter and never loses it.** 0.014 chip per block at 45 dB-Hz and
    0.027 at 40 against the closed loop's 0.013 and 0.021, over 799
    blocks per row, the worst excursion under a tenth of a chip — where
    a detector re-finding the peak would have left the emitter 42% of
    the time at the floor. The searcher-timed tracker of §12.22 is now
    measured whole: acquisition, dead reckoning, the correction, and
    nothing of the truth in it.
- **The held phase carries the read's noise, because the correction is
    the whole read.** The held σ equals the truth-cell read's σ in every
    row: put at the read, the phase inherits it. A tracker that wants to
    sit under the loop filters the correction — a gain `g` on it leaves
    `g / (2 − g)` of the read's variance and lets the dead reckoning
    carry the rest, which the Doppler's under-a-hertz read allows
    (0.001 chip per second of rate error at 0.5 Hz). Not measured; the
    gain is the tracker's one design parameter.
- **The held phase sits at minus the read's bias.** The correction drives
    the read to its calibrated zero, so the held phase settles at the
    negative of §12.22's bias: −0.004 against +0.003 at 45 dB-Hz and
    18 ppm, +0.012 against −0.010 at 40, and at 0 ppm the S-curve's
    0.03–0.05 chip with its sign reversed. The 0 ppm bias is the
    calibration's (§12.22), and calibrating at the working drift removes
    it from the held phase the same way.
- **The Doppler seed's row bias is harmless.** At 0 ppm the parabola
    over the rows reads +3.6 Hz, §12.20's unexplained quarter-row; as a
    rate that is 0.007 chip per second, 0.0002 per block, which the
    correction absorbs without trace.
- **The gate.** `--check` at 45 dB-Hz and 18 ppm: the tracker acquires
    within a tenth of a chip and 20 Hz, never leaves the cell, and holds
    the phase within twice the loop's jitter without bias. Sabotaged by
    reversing the correction's sign (the held phase walks a chip off in
    both classes) and by dropping the dead reckoning (at 18 ppm the
    phase falls 2.8 chips behind per block); both go red.
- **Not measured here:** the filtered correction, two emitters in one
    tile, and the symbol detector on the prompt.

______________________________________________________________________

### 12.24 What was measured (2026-09-08) — the filtered correction

**What was built.** §12.23's tracker with a gain on its correction:
the held phase moves by `g` times what the coasting DLL read, once a
block, and the dead reckoning on the held Doppler carries the rest. A
first-order loop on a white read keeps `g / (2 − g)` of the read's
variance; the harness prints that prediction from the gain-1 row beside
each measured one. Gains 1, 1/2, 1/4 and 1/8 at 18 ppm, both C/N0s, the
same seeds as §12.23, 800 dwells each.

**The held phase under data, per block** (chips; the prediction is
`sqrt(g / (2 − g))` of the gain-1 σ; the excess is what is left in
quadrature):

| gain | 45 dB-Hz: σ, predicted, excess | worst | 40 dB-Hz: σ, predicted, excess | worst | left the cell |
| ---- | ------------------------------ | ----- | ------------------------------ | ----- | ------------- |
| 1    | 0.0144, 0.0144, —              | 0.046 | 0.0272, 0.0272, —              | 0.099 | never         |
| 1/2  | 0.0087, 0.0083, 0.0026         | 0.031 | 0.0167, 0.0157, 0.0057         | 0.062 | never         |
| 1/4  | 0.0064, 0.0054, 0.0034         | 0.022 | 0.0114, 0.0103, 0.0049         | 0.045 | never         |
| 1/8  | **0.0052**, 0.0037, 0.0037     | 0.018 | **0.0082**, 0.0070, 0.0043     | 0.034 | never         |

The bias is the gain's to leave alone: −0.004 at 45 dB-Hz and +0.012
at 40 at every gain, §12.23's S-curve zero. The window class reads the
same curve (0.0128 → 0.0046 at 45 dB-Hz, 0.0249 → 0.0071 at 40).

What it settles:

- **The filtered tracker sits under the loop it replaces.** At gain 1/8
    the phase held on the searcher's own cell is 0.0052 chip at 45 dB-Hz
    and 0.0082 at 40, against the closed DLL's 0.013 and 0.021 — 2.5
    times under it at both — with the worst excursion in 25 s under
    0.035 chip and the cell never left. The correction's time constant
    is eight blocks, a quarter of a second, which the dead reckoning
    carries without trace: at the Doppler the surface read (0.7 Hz off
    at 40 dB-Hz), the rate error is 0.0003 chip per block and its lag
    at gain 1/8 is 0.002 chip.
- **The read is white to gain 1/2, and below that a floor shows.** The
    measured σ follows the first-order line at gains 1 and 1/2 and then
    leaves it: the excess in quadrature is 0.0035 chip at 45 dB-Hz and
    0.0045 at 40 at gains 1/4 and 1/8, nearly the same at the two C/N0s
    where the read's noise differs by two. So it is not the noise; it is
    a component of the read that wanders slower than the filter's time
    constant and is tracked rather than averaged. Its source is not
    measured here. Two candidates, unranked: the calibrated S-curve's
    residual shape (ten bins across the cell, interpolated, the loop's
    zero read to 0.04 within a block), and the symbol window's boundary
    walking through the block — 154 epochs are 84.1 symbols, so the
    window's phase against the block's edges advances a tenth of a
    symbol per block and comes round every ten. A tracker that wants
    the last factor of two finds out which.
- **The gate.** `--check` runs gains 1 and 1/4 at 45 dB-Hz and 18 ppm:
    the filtered tracker never leaves the cell and holds the phase under
    three quarters of the unfiltered jitter (0.0050 against 0.0132 on
    the check's 137 blocks), without bias. Sabotaged by ignoring the
    gain; red.
- **Not measured here:** the floor's source, a gain under 1/8 (the floor
    says it buys nothing further), the Doppler refreshed from the window
    dwells, two emitters in one tile, and the symbol detector on the
    prompt.

______________________________________________________________________

### 12.25 What was measured (2026-09-09) — the DLL's one steer, and the read a holder corrects on

**What was built.** The searcher-timed receiver's build (§12.23–12.24's
tracker as a mode of `async_dsss_receiver`, [#1283](https://github.com/doppler-dsp/doppler/issues/1283))
starts from three `Dll` primitives. First, [#1280](https://github.com/doppler-dsp/doppler/issues/1280):
the discriminator, its clamp, the probe and the coast hold existed twice —
the full-epoch path's inline `dll_update()` and the partial-correlation
core's private `steer()` — and §12.22's coast fix had reached them one at
a time. Both now call one `dll_steer()`. Their control laws were found to
differ as well: with the same loop filter, the full-epoch loop applies the
integrator as a code-rate ratio and the proportional term as chips per
epoch, the partial loop applies the filter's whole output as chips over
`sps` — gains `sf·sps` apart on the integrator and `sps` on the
proportional term. Each is pinned by its own tests and the `Dll` report,
so the two gain tables stay as data the one steer reads, and reconciling
them is [#1284](https://github.com/doppler-dsp/doppler/issues/1284), a
certified-behaviour change on its own. Second, `dll_take_error()`: every
steer adds its clamped discriminator to a running sum, coasting or not;
take returns the count and the sum and zeroes them. Third, a held loop
takes a new rate aid at once — `dll_set_rate_aid()` only stored the aid
for the next steer to fold in, and a coasting loop has no next steer, so
it kept the aid it was held with (the harness never met this: it set the
aid once, before coasting; a receiver refreshing the Doppler it holds
would). Pinned in `test_dll_core` parts 9 and 10, each sabotaged red
(no accumulation; no zero on take; no recompute when held; the
accumulator dropped from the blob).

**What was measured.** §12.22's harness read the coasting DLL's
discriminator through its telemetry probe, which is each epoch's LAST
steer; the product reads the per-steer sum. The two, side by side on the
same blocks at 45 dB-Hz (`--check`, 136 data dwells):

| ppm | probe mean, σ per block | per-steer mean, σ per block | steers per block |
| --- | ----------------------- | --------------------------- | ---------------- |
| 0   | −0.1877, 0.0241         | −0.1880, 0.0232             | 85.1             |
| 18  | −0.1081, 0.0220         | −0.1091, 0.0211             | 85.0             |

The per-steer mean is the probe's to 0.001 in mean and 4% lower in
scatter (the probe samples 154 last-steers per block, the sum averages
the 85 steers themselves). §12.22–12.24's calibration and gains transfer
to the product's read as they stand. The `Dll` benchmark: 31.7 MSa/s
before the one steer, 31.6 after — noise.

What it settles: the correction a holder applies is one primitive with
one home, read the way the loop itself would have filtered it; the two
loops' gains are named as different and tracked, not unified by accident.
Not measured here: the reconciliation of #1284.

______________________________________________________________________

### 12.26 What was measured (2026-09-09) — the cell mode, the shipped object on the harness

**What was built.** §12.22–12.24's tracker as a mode of
`async_dsss_receiver`, `CellAsyncDsssReceiver` (`create_cell`), by
turning stages off rather than by a second object (the plan of
[#1283](https://github.com/doppler-dsp/doppler/issues/1283), the
decision of §11.1): no refine, the `Dll` held from the first sample and
never closing its own loop, a held code phase kept in double,
dead-reckoned on the carrier loop's Doppler and corrected once every
`correct_periods` code periods by a gain in chips times the coasting
`Dll`'s interval-mean discriminator (`dll_take_error()`, §12.25's read).
Gain 1 through `pullin_intervals` (the seed's residual), the design gain
1/8 after; with the code flag down the phase only dead-reckons. Past the
`Dll` — the symbol path, the symbol lock, the release rule, the status
record — the hand-off flavour's verbatim. Four things the plan had wrong,
each found by a measurement that went red and corrected in the build:

- **The carrier loop runs; it is not frozen.** The plan wiped the stream
    on a frozen LO at the seed's Doppler and left the residual to
    `MpskReceiver`, folding it back above a threshold. Measured on SPEC's
    500 Hz/s ramp: `MpskReceiver`'s 27 Hz loop alone lost the symbol lock
    on 40 intervals of 48, BER 0.45. The pre-despread loop 1 of the
    hand-off flavour runs unchanged, held on both flags down as there,
    and refreshes the `Dll`'s rate aid every period; there is no fold
    and no `fold_hz`. Running, the ramp is followed to 3 Hz at its end
    with the symbol flag never down.
- **Never a phase kick at a period boundary.** The plan put the `Dll` at
    the corrected phase with `dll_set_code_phase()` once an interval. A
    receiver fed whole periods is always at a boundary, a kick there
    lands on the code's wrap, and moved across it the `Dll`'s partial
    bookkeeping emits or skips a period's partials: a symbol slip every
    few intervals on SPEC's geometry, where the hand-off flavour on the
    same capture decoded clean
    ([#1287](https://github.com/doppler-dsp/doppler/issues/1287)).
- **The make-up is a rate bias every period, not over the interval.** The
    first replacement spread what the `Dll` had to make up across the
    next interval as a rate aid. That bias is a few parts in 10⁷ —
    0.075 chip over 154 periods — under the code NCO's 32-bit rate step,
    and the `Dll`'s phase sawtoothed 0.06 chip about the held one on this
    harness. Steered every period (the held phase dead-reckoned to the
    period's end against the `Dll`'s own, as a bias on the next period's
    aid — the way its own loop steers), the same step is half a
    thousandth of a chip.
- **The gain is in chips.** The read is in the discriminator's units, `2   − spacing` per chip of offset; the correction divides by that slope
    so a gain of 1 puts the phase at the read.

Pinned by six C tests, each sabotaged red eight ways (the correction's
sign; the dead reckoning dropped; the correction applied on noise with
the flag down; the carrier frozen; the make-up dropped; the gain
ignored; the held phase left out of the blob; the `Dll`'s own loop
closed). On the receiver's own tests (the release harness's synth →
`doppler_channel` → `awgn`, 45 dB-Hz, 18 ppm, truth from the synth's
clock), 450 intervals:

| gain | held phase bias, σ, worst (chips) | flags   | symbols |
| ---- | --------------------------------- | ------- | ------- |
| 1    | +0.0087, 0.0137, 0.048            | 1, 1, 1 | 11998   |
| 1/8  | +0.0087, **0.0033**, 0.018        | 1, 1, 1 | 11998   |

The bias is the channel's mapping, the same at both gains. Switched
off, the receiver is lost after 31 intervals of the release clock's 57
and its held phase is 0.003 chip from the hold point 160 intervals on
(0.015 at switch-off). SPEC's 500 Hz/s ramp: the truth 1232 Hz at the
end, the status 1229, the symbol flag never down after the pull-in,
6650 symbols at BER 0. Speed: `bench_async_dsss_receiver_core`'s cell
row is 28.70 ns per input sample in tracking at the operating point
against the hand-off flavour's warm row at 28.71 — the same, since
loop 1 runs in both and the refine is in neither's steady state.

**What was measured.** The shipped object on §12.20's harness: the
held mode's private DLL, LO and bookkeeping replaced by one
`CellAsyncDsssReceiver` fed the epochs the engine is pushed, seeded from
the surface at the first window dwell as the held mode is (the pool's
seed: the phase at the dwell's end in the `Dll`'s convention, the row's
Doppler), its phase read off `status()` at each dwell against the truth,
its symbols scored against the synth's payload by `dp_ber_measure` on
one alignment. Beside it the hand-off flavour on the same seed and
stream — the parity reference — and the harness's own held mode
(§12.24). 18 ppm, 800 dwells (25 s), both C/N0s; the phase under data
(740 dwells):

| flavour                  | 45 dB-Hz: bias, σ, worst   | flags      | 40 dB-Hz: bias, σ, worst   | flags      | left the cell |
| ------------------------ | -------------------------- | ---------- | -------------------------- | ---------- | ------------- |
| harness held mode, g=1/8 | −0.0035, 0.0047, 0.015     | —          | +0.0125, 0.0068, 0.037     | —          | never         |
| **cell, g=1/8**          | −0.0036, **0.0056**, 0.019 | 1.00, 1.00 | −0.0036, **0.0082**, 0.033 | 1.00, 0.99 | never         |
| cell, g=1                | −0.0036, 0.0143, 0.053     | 1.00, 1.00 | −0.0034, 0.0268, 0.085     | 1.00, 0.99 | never         |
| hand-off (closed `Dll`)  | −0.0037, 0.0105, 0.039     | 1.00, 1.00 | −0.0041, 0.0219, 0.074     | 1.00, 0.99 | never         |

The decode, on the same records (the window opens past the receiver's
settling; theory is BPSK at the payload's Es/N0):

| flavour     | 45 dB-Hz (Es/N0 10.7 dB, theory 6.5e-7) | 40 dB-Hz (Es/N0 5.7 dB, theory 3.3e-3) |
| ----------- | --------------------------------------- | -------------------------------------- |
| cell, g=1/8 | 1 error in 66767 bits, 0 slips          | 0.31 — 4 cycle slips; ~1% between them |
| cell, g=1   | 1 error in 66767 bits, 0 slips          | 0.42 — 3 cycle slips                   |
| hand-off    | 0 errors in 66671 bits, 0 slips         | 0.22 — 2 cycle slips; ~1% between them |

What it settles:

- **The shipped cell mode holds the phase the harness's held mode
    held.** 0.0056 chip at 45 dB-Hz and 0.0082 at 40 against §12.24's
    0.0052 and 0.0082 — the same read through the product's carrier loop
    and per-period rate steer instead of the harness's calibrated
    S-curve and per-block kick — and 1.9× and 2.7× under the hand-off
    flavour's closed `Dll` on the same seed (0.0105 and 0.0219), never
    leaving the cell, both flags up. The constant gain against §12.24's
    calibrated S-curve is not the floor's source: the excess in
    quadrature over first order is the same (0.0037 there, 0.0040 here).
- **At 45 dB-Hz both flavours decode at theory** with no cycle slip in
    25 s; the check's 137 dwells give 0 errors in 11469 bits for the cell
    and 0 in 11373 for the hand-off.
- **At 40 dB-Hz the receiver's carrier slips cycles, in both flavours.**
    A BER of 0.3 with the EVM at the SNR (−5.6 dB at Es/N0 5.7) is not a
    decoder: rescored in 500-symbol chunks on the one alignment, the
    record is at 1% between whole stretches that are inverted — 180°
    slips of the carrier, 4 in 25 s for the cell mode (3 at gain 1) and
    2 for the hand-off flavour on the same stream, at the ~5 dB Es/N0 floor
    the loop's bandwidth comment already names. The same receiver decoded
    its own C test at 40 dB-Hz over 2549 symbols, a record too short to
    meet one. The 1% between slips against theory's 0.3% is the second
    part of the same finding. Neither is the cell mode's: it is the
    carrier path both flavours share, and it is a live emitter's decode
    at the pool's lower operating point —
    [#1289](https://github.com/doppler-dsp/doppler/issues/1289).
    The harness now counts slips beside every BER, so a 0.3 cannot read
    as a decoder again; the cell gate at 45 dB-Hz requires 0.
- **The gate.** `--check` (137 dwells, 45 dB-Hz, 18 ppm) runs the cell
    mode at the design gain and the hand-off reference: the cell never
    leaves the cell, holds the phase within twice the closed loop's
    jitter without bias (0.0043), both flags up, decodes the payload
    with no cycle slip. Sabotaged red: the receiver's symbols negated from
    the record's midpoint on — the counter reads 1 slip, the BER 0.55, and
    both the decode and the slip gate fail.
- **Not measured here:** the slip rate as a curve in C/N0 (one stream at
    one point below the floor is a count, not a rate), two emitters in
    one tile, and the pool on cell receivers (§12.27).

______________________________________________________________________
