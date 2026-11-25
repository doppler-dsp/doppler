---
description: "the dense-LEO pool scene: the §1.8 passes joined to the dp_air test header as truth, the timeline that places them, and the scorer that says what each pool receiver holds"
---

# Pool constellation scene

The pool scene is a set of LEO passes that rise and set on one code, each
with its own Doppler track, its own level and its own time. It is the
instrument the pool's rule is designed against: a scene in which every
receiver's holding can be checked against what is really on the air, and in
which the emitters move. The scene and its scorer are a C validator over a
timeline header in the test tree. Nothing here chooses the pool's rule; the
scorer reports what the pool holds, and the rule is designed on that report.

## 1. What it is

Three parts, each a pure function of the one before it:

- **Passes.** Each emitter is a pass drawn from the 25-pass table of the
    receiver's dynamics (async-dsss-receiver.md §1.8), at the carrier the scene
    runs at. A pass fixes its residual carrier track and its slant-range level.
- **A timeline.** A seed and a small parameter struct give the arrivals, each
    emitter's start, its timing error, its level and its code phase. No
    rendering happens here.
- **Windows and truth.** A window is a span of the timeline, 30 s long, placed
    by a stated rule on the timeline's own events. Its truth is what `dp_air_test.h`
    reports for each emitter at each sample, the carrier and the level, not a
    value read back from the pool.

The validator renders a window through `dp_air_test.h`, runs the pool over it, and
scores each holder against the truth (§7).

## 2. Who uses it

- **The pool's rule** (#3263). It is designed against the scorer's rows: a
    duplicate released on age, a ghost held past its tenure, a cross-line
    hand-over. The scene is the only place those cases are produced together.
- **The pool's release and certification.** The scene's bar (§6.3) is the
    condition the pool is certified against for dense passes; the gallery
    scene is the example, not the bar.
- **Readers of the record.** Each run is committed with its reader, so the
    tables in a record are printed from committed rows.

## 3. Goals

- Emitters rise and set from differing tracks on one code, with levels that
    follow the pass geometry, not fixed ones.
- A total spread, the scene's axis: 16, 20, 24 and 28 dB, realised by the
    geometry and the declared power offsets together (§5.5).
    The pool's limit is read as a curve through that axis, not as one number.
- A scorer that asks, for every tracking receiver, which signal it holds, and
    says which class it is: real, ghost, cross line, or ambiguous (§7).
- Every number the scorer uses either derived from the rule it scores or taken
    from the code it measures (§9). No fitted number.

## 4. What it composes

- `dp_air_test.h` (`native/tests/dp_air_test.h`): per-emitter truth. It has a static
    level, linear Doppler and no set time. The scene needs a set time, a carrier
    track and a level track; without them an emitter renders as it does now.
- The pass model in `dp_s415_test.h`: `dp_s415_pass_el()` for the maximum
    elevation of a pass at an altitude, `dp_s415_pass_curve()` for its
    residual-free curve f, f', f'' above the 10° mask, and `dp_s415_pass_table()`
    for the per-pass rows of §5.1.
- The pool (`async_dsss_pool_core.h`) and its status, read only.
- The ghost validator's rule (`async_dsss_pool_ghosts.c`): its 0.2 s window,
    its carrier stillness, and its k of ambiguity intervals.
- The two-carrier, cross-line arithmetic of §8.2 in async-dsss-receiver.md.

## 5. The passes, their levels and the truth

### 5.1 The table

The passes are the 25 of §1.8: a LEO circular orbit, a 10° elevation mask, an
altitude grid from 350 km in 50 km steps, both carriers, and the maximum
elevation solved at each altitude so that the raw pass peaks at 400 Hz/s.

Every figure in this section is printed by `pi_design()` in
`native/validation/async_dsss_receiver_pullin.c`: the `# pass` section gives
the pass count and R_env, the `# pass_table` section gives the rows below, and
the `# spreads` section gives the three spreads of §5.2 per carrier and over
both. The rows come from `dp_s415_pass_table()` in
`native/tests/dp_s415_test.h`, which calls `dp_s415_pass_el()` and
`dp_s415_pass_geometry()`, the same geometry `dp_s415_pass_curve()` calls. The
envelope calls `dp_s415_pass_curve()`.

| carrier | passes | altitude, km | max elevation, deg | full pass, s | slant range at the peak, km |
| ------- | ------ | ------------ | ------------------ | ------------ | --------------------------- |
| 2.2 GHz | 11     | 350 to 850   | 15.8 to 71.0       | 218 to 663   | 893 to 1020 (1.16 dB)       |
| 2.8 GHz | 14     | 400 to 1050  | 13.0 to 76.3       | 183 to 785   | 1076 to 1272 (1.45 dB)      |

Over both carriers the slant range at the peak spans 893 to 1272 km, 3.07 dB.
The full pass is 2 t_max, the time from rise to set above the mask.

### 5.2 Three spreads, named

A level is set by the slant range through the free-space law, so every spread
below is a ratio of ranges, read from the table's extremes (`# spreads` in
`pi_design()`). There are three, and they are different quantities:

- **(a) Between passes, at their peaks.** The shortest peak range against the
    longest, over the passes of one carrier: 1.16 dB at 2.2 GHz and 1.45 dB at
    2.8 GHz. Over both carriers, 3.07 dB. Each pass is at its own maximum
    elevation, so this is the spread of the pass set itself.
- **(b) Within one pass, peak to mask.** The largest single pass's sweep from
    its maximum to the 10° mask: 8.8 dB at 2.2 GHz (the 850 km pass, 893 to
    2468 km) and 8.5 dB at 2.8 GHz (the 1050 km pass, 1076 to 2857 km). Over both
    carriers, 8.8 dB.
- **(c) Instantaneous, among emitters in view together.** Each emitter is at its
    own point of its own pass, so at one instant two emitters can be at very
    different ranges. The table bounds this from both sides: the shortest peak
    range against the longest mask range, per carrier. At 2.2 GHz that is 8.8 dB
    (893 km at the 850 km peak, 2468 km at the 850 km mask). At 2.8 GHz it is 8.5
    dB (1076 km at the 1050 km peak, 2857 km at the 1050 km mask). Over both
    carriers it would be 10.1 dB, but a scene runs on one carrier (§6.1), so the
    per-carrier figure is the one a scene can show.

(c) is a maximum, not a distribution. Which combinations occur, and how often,
depends on which passes are in view together, so the distribution is not
asserted here; the timeline's function prints it (§6.1). Emitters in view
together are not all at their peaks, which is why (c) is not bounded by (a).

### 5.3 The level inside a window

Inside a 30 s window a level changes by about 1 dB, so it is treated as constant
within a window. The worst 30 s change over every pass in the table
(`worst30_db` in `# pass_table`) is 1.01 dB, from the 2.2 GHz pass at 850 km (and
1.007 dB at 800 km); at 2.8 GHz the worst is 0.82 dB, from the 1050 km pass. A
rise or a set is then a step at the mask, not a drift.

### 5.4 The residual carrier

The receiver is never given the raw pass. The link is steered from an
ephemeris, and what reaches the receiver is the residual of the timing error:
for an emitter that arrives `dt` early or late, `f(t) - f(t - dt)`, within
±1,500 Hz. Its rate is the envelope of §1.8, 11 Hz/s after the rotation factor.
Over a 30 s window the residual carrier moves by at most 11 Hz/s × 30 s = 330 Hz,
which is the carrier movement the scorer sees in a window.

### 5.5 The power offset and the reference

Each emitter's level is the slant-range level from the table, plus its own power
offset:

```
level_e = F − 20 log10(range_e / range_ref) + offset_e,   0 <= offset_e <= D
```

- `range_ref` is the longest mask range of the scene's carrier: 2857 km at
    2.8 GHz (the 1050 km pass at the mask), 2468 km at 2.2 GHz (the 850 km pass).
- `F` is the reference level, Es/N0 4 dB (38.77 dB-Hz), the design point (§11).
- `D` is the declared power-offset range. It is not the axis: the axis is the
    total spread, and `D` is set from it per carrier (below).

The weakest case is the corner where the emitter's range is `range_ref` and its
offset is 0. There `level = F`, the design point. At every other emitter
`range_e <= range_ref` and `offset_e >= 0`, so `level_e >= F`: every emitter on air
is at or above Es/N0 4 dB (38.77 dB-Hz). The bars score at Es/N0 5 dB (39.77
dB-Hz) (§11), so the corner and the emitters near it sit in the band below that,
on the air and printed but not scored.

The axis is the total spread among emitters in view, 16, 20, 24 or 28 dB. The
total is the most the scene can show: the level spread between two emitters
is at most (c) + D (§5.2), so each axis value sets D by `D = total − (c)`, with
(c) the carrier's printed instantaneous spread. At 2.8 GHz, with (c) = 8.5 dB,
that is D = 7.5, 11.5, 15.5 and 19.5 dB; at 2.2 GHz, with (c) = 8.8 dB, D = 7.2,
11.2, 15.2 and 19.2 dB. The realised spread of each window is printed with the
window, and it is at most the total; a window need not reach it.

### 5.6 The truth

For every emitter, `dp_air_truth()` reports at sample k the carrier and the
level the emitter has at that sample, from its tracks. Truth is a closed form of
the timeline, so it is exact by construction; the scorer never reads the pool to
decide what is on the air. Which fields are exact is stated in `dp_air_test.h`:
the code, symbol, level and on-air fields are exact closed forms.

The carrier is exact to the track's own closed form, its integral, but not to the
carrier the channel applies. Under a carrier track the channel reads its carrier
off its resampler's position, which steps in 2^-32 of an input interval, so the
realised carrier is within `fc × 2^-32` Hz of the truth's: 0.51 Hz at 2.2 GHz and
0.65 Hz at 2.8 GHz. That is the channel's own bound, pinned by
`test_doppler_channel_core.c` §8b. So carrier PHASE from the truth is not exact,
and the scorer classes on carrier in Hz, where that bound is small against `ε`
(§7.2).

## 6. The timeline, the windows and the bar

### 6.1 The arrivals and the carrier

Each scene runs at one carrier. Both carriers are run; 2.8 GHz is where the
pool's held ghosts are (#3263), so it is the primary case.

Arrivals are drawn at a mean of 8 emitters in view, capped at 10 on the air at
once: the signal count the specification allows on one code (async-dsss-receiver.md,
the requirements table). Each arrival is a pass drawn uniformly from the passes
of the scene's carrier (14 at 2.8 GHz, 11 at 2.2 GHz), a timing error `dt`
drawn over the range that reaches 1,500 Hz, a power offset within [0, D] (D from the total spread, §5.5), a code
phase and a data seed. The timeline function prints the distribution of (c) it
produces.

### 6.2 The windows

A window is 30 s, placed by a rule on the timeline's events, never by hand:

- each rise;
- each set;
- each instant two residual tracks come within one ambiguity interval of each
    other (the cross-line case);
- the instant of largest spread among the emitters on air.

A window is named by its event. Each window is run from a cold pool (§10), and
its truth comes from the timeline alone, so it does not depend on the run it was
cut from.

### 6.3 The bar

The bar is held over a run's tenures, and each rate has its own unit:

- a ghost tenure ends within 0.5 s, at least 98% of the time. The unit is one
    ghost tenure;
- a decodable emitter is released wrongly at most 1 time in 1,000. The unit is
    one decodable emitter's tenure, and wrong means released while it is on the
    air and at or above Es/N0 5 dB (39.77 dB-Hz);
- every emitter is held while it is at or above Es/N0 5 dB and released when it sets,
    and no eligible slot is left starved. The held fraction is per emitter per
    window.

These are the #2529 criteria carried over, with the pool terms of #3263. Each rate
is printed with its count and a Wilson interval. Tenures in the ambiguous class
(§7.1) are left out of the rates, and their count is printed beside them. The
1-in-1,000 term sets the run size: a run resolves that rate only as far as its
tenure count allows (§10).

## 7. The scorer

### 7.1 The classes

Every tracking receiver is classed against the truth at each sample of a window.
Two things decide the class, in order: which code the holder sits on (its code
phase), and then which carrier hypothesis is nearest among the hypotheses that
code allows.

**The code decides the family.** A holder is on an emitter's code when its code
phase is within `DP_CORRELATOR_SAME_PHASE_CHIPS` (one chip, the pool's own
duplicate test) of that emitter's phase. A holder whose phase is a sidelobe lag
of a parent, and within one chip of no emitter, sits on a ghost code.

**The carrier hypotheses.** Each code allows these carriers, all on the comb of
ambiguity intervals, which are lattice steps of `1/(2T)` with `T` the code period
(318.2 µs at 2.2 GHz, 250.0 µs at 2.8 GHz):

- on an emitter's own code: its own carrier `f_e`, and each cross line
    `(f_s + f_e + k/T)/2` for every stronger emitter `s` on the air. A cross line is
    a holder on the weaker emitter's code that the fine hand-over has placed
    between the two carriers (async-dsss-receiver.md §8.2);
- on a ghost code of parent `p`: the parent's comb, `f_p + k/(2T)`, `k` a whole
    number. `k` is the ghost's class detail, reported and not used to class.

**The classes.**

- **real**: the holder is on an emitter's code, and the nearest hypothesis is that
    emitter's own carrier, with no competing family inside the resolution.
- **cross line**: the holder is on the weaker emitter's code, and the nearest
    hypothesis is a cross line, with no competing family inside the resolution.
- **ghost**: the holder is on a ghost code, and its carrier is nearest the
    parent's comb.
- **ambiguous**: the two nearest hypotheses from different families lie within
    the resolution of each other. This is the case no tolerance can separate: a
    cross line and the weaker emitter's own carrier coincide whenever
    `f_s − f_e` is near a whole multiple of `1/T`, and then the carrier alone does
    not say which is held.
- **unmatched**: none of the above.

Within one family the comb's spacing, `1/(2T)`, is 1.6 to 2 kHz, far above the
resolution, so the nearest line within a family is always unambiguous. Only
different families compete.

### 7.2 The resolution

The resolution `ε` is the carrier distance below which two families cannot be
told apart by the holder's reported carrier. It is a property of the receiver's
carrier report, not of the rule being scored, so it is measured, not taken from
the rule's constants. The rule's bounds are what the pool's rule will change, and
a scorer whose resolution moved with the rule would let the rule set its own
grade.

**Definition.** `ε` is the 99th percentile, nearest rank, of the per-tenure
largest `|reported carrier − truth|` over the on-phase tenures of a lone emitter
at Es/N0 4 dB (38.77 dB-Hz). That is the lowest level anything on the air has, and
every holder is classed, scored or not, so the resolution must cover the weakest
one. A lone emitter has no other family on the air, so the error is the
receiver's own.

Each tenure's value is its largest error from one second into the tenure: the
pull-in transient is a separate figure. A tenure is on-phase when its code phase
stays within one chip of the emitter's, the scorer's own real-class test. A
tenure that leaves that band is a ghost: it is excluded from `ε` and counted
apart, with the count printed, because a ghost is what the scene scores.

Why the 99th percentile and not the maximum. The maximum over a run grows with
the number of draws, and one off-phase tenure sets it: 2.28 Hz over the first 20
draws of the 2.2 GHz run and 1403 Hz over 40, from draws 23 and 30 alone. A
maximum of that kind is not a property of the receiver. The upper tail is what
matters: a holder whose error exceeds `ε` could be classed as another family, so
the scorer needs the tail, and the 99th percentile is the tail it can read from a
pool of on-phase tenures without the largest one deciding it.

Stable once the pool holds 300 on-phase tenures; the run prints the count and
says NOT STABLE below it. Off-phase tenures are a number the page carries: a pool
of four on a lone emitter holding a ghost is what the scene exists to score.

**What prints it.** The ghost validator's ALONE arm prints the quantile, its
count, and the excluded tenures (off-phase, and shorter than the skip) at each
level. Its value is pending a run that gives the pool its count (§10).

The scorer does not widen `ε` to hide a result. If a run's ambiguous count is
large, the run reports that count; it does not take a larger `ε` to shrink it.

### 7.3 What each class does besides its carrier

The scorer prints these beside each class and does not classify by them. They
are the cross-check that the class is right.

- **real**: the holder decodes (BER below 1e-2) and its symbol lock is up while
    held.
- **cross line**: its symbol lock stays down (#3295, where a cross-line holder
    held the weaker emitter's code with symbol lock never up).
- **ghost**: its code lock is low. The record's k = 0 ghost (#3216) read a code
    lock of 0.11.
- **ambiguous**: the same observables, reported per case; no class is assigned
    from them.

If a real holder fails to decode, or a cross-line holder decodes, the class is
wrong, and the run reports that count.

### 7.4 The rows

Reported per window, each with the rows that produced it:

- per emitter: time to take once eligible, the held fraction while at or above
    Es/N0 5 dB, symbol lock, BER against the truth bits (decodable is BER below
    1e-2, the line `read_cen.py` uses), and time to release after it sets. BER is
    taken per window against the alignment that window holds (below);
- per tenure: the slips after all five lock flags, with the window each fell in.
    A slip is a whole-symbol alignment change with no flag marking it (#3349).
    It is its own row, not folded into BER. A tenure's symbols count from the
    sample at which all five lock flags are up, which `dp_s415_lock_flags()`
    reads; a tenure's symbols before that are not scored;
- per ghost tenure: its length, parent, `k`, lead in dB, and the window's largest
    `|delta - k × interval|` (the column `gh_win_dev` writes);
- per release: lost, duplicate or ghost, and whether it was right or wrong against
    the truth. A wrong duplicate release is the `keeps()` case of #3263;
- starved time: an eligible emitter untaken while a receiver sat idle;
- per class: the count, and the §7.3 observables beside it.

## 8. The scorer's own controls

Four planted cases are classed as themselves by a unit test:

- a ghost, on a parent's sidelobe lag with its carrier on the parent's comb;
- a duplicate, two holders on one code phase;
- a cross-line holder, with the carriers apart and the holder on the weaker code;
- an ambiguous case: a holder on the weaker emitter's code whose carrier is within
    `ε` of both its own carrier and a cross line.

Each classifier clause is removed in turn, and its row turns red. The ambiguous
control is the one that proves the class exists: without it the scorer would
class that holder as either real or cross line, and nothing would turn red. These
are the scorer's negative controls, written with the scorer.

## 9. Tolerances and where each comes from

| tolerance                                         | value                    | from                                                                                                   |
| ------------------------------------------------- | ------------------------ | ------------------------------------------------------------------------------------------------------ |
| same code phase (on an emitter's code, duplicate) | 1 chip                   | `DP_CORRELATOR_SAME_PHASE_CHIPS`, the pool's own duplicate test                                        |
| carrier resolution (ambiguous class)              | 99th percentile, unknown | per-tenure largest reported-carrier error over on-phase tenures of a lone emitter at Es/N0 4 dB (§7.2) |
| ambiguity interval (comb spacing)                 | `1/(2T)`                 | the code's lattice: `chip rate / (2 × code length)`, from the chip rate                                |
| ghost tenure window                               | 0.2 s                    | `ASYNC_DSSS_POOL_GHOST_WINDOW_S`, the rule being scored                                                |
| ghost end value                                   | 10 Hz                    | `ASYNC_DSSS_POOL_GHOST_EPS_HZ`, the rule being scored                                                  |
| ghost end, existing validator                     | 40 Hz                    | EPS + 3 × STILL; the 3 is a 3-sigma coverage choice, labelled as such                                  |
| decodable                                         | BER below 1e-2           | `read_cen.py`'s line                                                                                   |
| bars                                              | 0.5 s, 98%, 1 in 1,000   | carried from the #2529 criteria with the #3263 pool terms (§6.3)                                       |

The 40 Hz end value is the one number here that is not derived: its 3 is a
coverage choice on a sample standard deviation, and the scene does not inherit
it. The bound the scene uses for a ghost's carrier is read from the recorded
`|delta - k × interval|` column of §7.4, not set beforehand.

## 10. Unknowns

- **Pool throughput on this scene.** It sets how many tenures a run can afford,
    and so whether 1 in 1,000 is resolvable on a run or is stated as a looser
    bound. It is measured first, at 8 to 10 emitters on air at 11 MSa/s, and the
    run is sized from it.
- **The slip rate after all five lock flags.** A receiver has been measured to
    slip a whole symbol after all five flags are up, with no flag marking it
    (#3349: 2 of 1023 runs slip, 4 slips in all, 3 of them in one run, both at
    2.8 GHz; time to five flags has a 90th percentile of 1.0 s at 2.2 GHz, Es/N0
    4 dB). The scorer reports slips as their own row (§7.4); their rate is what
    the run must measure.
- **Whether 30 s is long enough.** Some endings in the existing records came at
    20 s; a 30 s window is only enough if none runs past it.
- **The cold start.** An emitter mid-pass at a window's start is on the air at
    t = 0. It is scored as a take, and that is declared with the results.
- **The resolution `ε`.** Its value is not yet printed for a run long enough to
    fix its quantile (§7.2). Until it is, the ambiguous class is defined but its
    size is not, and the run prints the ambiguous count; it does not widen `ε`.
- **The 0.5 s and 98% bars.** They are the #2529 criteria carried over. The run
    states the counts behind them; it does not re-derive them.

## 11. Decisions

These six are settled and not reopened by this page:

- **Home.** `dp_air_test.h` is extended and stays a test header; the scene and the
    scorer are a C validator.
- **Passes.** Seeded draws from the 25-pass table of §5.1, at the scene's carrier.
- **Levels.** The slant-range level from the table, plus a per-emitter power
    offset within D, where D is set from the total spread (§5.5).
- **Bar.** The #2529 criteria carried over, plus the pool terms of #3263. The
    shipped pool is run first, and it is expected to fail.
- **Length.** Windows cut from a long timeline.
- **Two levels.** Both from the header (`native/inc/doppler/ccsds/ccsds_415.h`):
    Es/N0 4 dB, the design point, `dp_ccsds_415_cn0(4.0)` = 4 + 10 log10(`DP_CCSDS_415_SYM`)
    = 38.77 dB-Hz, is the reference `F` and the level the resolution is measured at;
    Es/N0 5 dB, the stated sensitivity, `dp_ccsds_415_cn0(5.0)` = 39.77 dB-Hz, at
    3,000 symbols a second, is the level the bars score from. Emitters between the
    two are on the air, printed, and not scored: an emitter the bars do not score
    cannot fail them. The reference corner of §5.5 sits at Es/N0 4 dB, so the band
    is real, not empty.

## 12. The work, in order

1. This page, reviewed before any code.
1. `dp_air_test.h` gains a set time, a carrier track and a level track, each with a
    sabotage-proven test; the existing scene's stream stays byte-identical.
1. The timeline in `dp_s415_test.h` (the per-pass table is already there), and the
    scorer as a shared header with its four negative controls.
1. The validator, `--check` in ctest under the sweep label, beside
    `async_dsss_pool_ghosts`.
1. The baseline: predictions written in the PR before the run, then one sharded
    run of the shipped pool on a heavy box, committed with its reader under
    `docs/design/async-dsss-receiver-measurements/`.
