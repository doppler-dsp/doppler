# Multi-peak acquisition — every emitter on one surface

*Phase 1 of [adding an algorithm](../dev/contributing/adding-algorithms.md).
Written 2026-09-02 from the continuous use case in
[`burst-bank.md`](burst-bank.md) §11, which named this as the one piece the
existing tools lack. Reviewed, not gated. Nothing below is implemented and
nothing below has been measured; the last section is the work that would
measure it.*

______________________________________________________________________

## 1. Context

### 1.1 What is settled, and what the page is for

The C++ application's waveform fixes the frame this page works in, and none
of it is re-derived here ([`burst-bank.md`](burst-bank.md) §11):

- **One Gold code, one frequency channel.** Every emitter is on the same
    1023-chip code in the same band; what tells them apart is Doppler, code
    phase and power — three coordinates on **one** (Doppler × code phase)
    surface, the surface a channel already computes.
- **`reps = 1`.** The data-free window is one code period, so the search is
    the continuous engine's: coherent depth one, sensitivity from
    non-coherent looks, no coherent gain to buy.
- **The channel always searches.** It never hands its samples over to a
    tracker and stops; search and track are concurrent.
- **The hand-off is a policy** — track, capture a window, or report — chosen
    per bank, and not a property of the channel.
- **The population** (maintainer, 2026-09-02): **at least one emitter is
    always on**, there may be **up to 10 at once**, and each is on for **5
    to 15 minutes** on average. So the surface never has fewer than one
    peak, has up to ten, and an emitter rises or sets about once a minute
    at the full population — every data-free window of every emitter is a
    re-acquisition opportunity, and a rise between two of them is the
    normal event the searcher exists for. This answers `burst-bank.md`
    §11.4's question 6: the receiver pool is sized at ten plus release
    headroom (§5), and the soak's population is known (§6 step 7).
- **The rate** (maintainer, 2026-09-02): all of it — the front end, the
    searcher, every receiver, and the cancellation if it is built — must
    run **comfortably at 30 MSa/s or more**, and running at exactly 30
    MSa/s counts as slow. That is the machinery's floor; the waveform's
    own operating point is below it (13 MSa/s in, next table), and the
    page prices every option at both (§1.4), not as a benchmark to run
    at the end.

The numbers the page is worked at (maintainer, 2026-09-02) — these
supersede `burst-bank.md` §11.2's, which were the async spec's waveform:

| quantity                       | value                                                 | from                                                        |
| ------------------------------ | ----------------------------------------------------- | ----------------------------------------------------------- |
| chip rate                      | **5 Mcps**                                            | given                                                       |
| code                           | 1023 chips → one epoch is **204.6 µs**                | given                                                       |
| coherent depth                 | **`D = 1`** — one epoch, no slow-time FFT             | given                                                       |
| DDC input                      | **13 MSa/s**                                          | given                                                       |
| DDC output                     | **2× chip rate = 10 MSa/s** (`spc = 2`)               | given; decimation is only 1.3×                              |
| samples per epoch              | 2046                                                  | `1023 · spc`                                                |
| Doppler tile                   | `1/T_epoch` = **4.89 kHz**, a tile spans ±2.44 kHz    | at `D = 1` the Doppler axis is the `window_bins` tile index |
| tiles over ±50 kHz             | 23                                                    | `2·ceil(50/4.89)+1`, *if* the uncertainty stays the spec's  |
| budget, one core, operating    | **77 ns per input sample = 100 ns per output sample** | `1/13e6`, `1/10e6`                                          |
| budget, one core, at the floor | **33 ns per input sample** (43 per output)            | `1/30e6`, same 1.3× ratio                                   |

The maintainer's description of the running system (2026-09-02) adds the
lifecycle the policy serves, and it is the shape everything below is fitted
to:

> The acquisition part continuously looks for signals, and async receivers
> track them as they are found, until they are gone. A receiver does not
> stop tracking once it has been assigned.

So there are two kinds of thing on the air side of the bank. A **searcher**
per channel (`DDC → search`), which runs on every block for the whole life
of the process. And a pool of **async receivers**, one per emitter, each
spawned by the track policy from one detection, fed the same samples as the
searcher, and living from that hand-off until *its own* loss decision — the
searcher never stops one, never re-seeds one, and never assigns a second
receiver to an emitter that already has one. The searcher's product is
therefore not "the strongest signal present"; it is **every emitter present
that is not yet assigned**, per dwell.

The receiver is the object that exists. `AsyncDsssReceiver`
(`native/inc/async_dsss_receiver/async_dsss_receiver_core.h`) is the
validated `search → refine → track` chain in one C object: its searching
stage feeds an embedded `Acquisition`, a hit is turned into a hand-off by
`acq_build_handoff()`, and that hand-off seeds the refine stage. What the
lifecycle needs from it is two things and no new receiver
(maintainer, 2026-09-02): **an acquisition input** — the searcher's
detection arrives from outside as the hand-off — and **an internal
acquisition bypass** for that mode, so the object starts in refining from
the given seed and its own `Acquisition` never runs. That is a difference
in constructor, not in method, so it is the `ddc`/`MatchedDDC` shape: a
second `create` over the same state, a view in the manifest, the chain
past the seed shared verbatim. The receiver already carries a symbol lock
detector (`lockdet`, hysteretic, on the emitted symbols), which is where
"until they are gone" is decided — what it lacks is the transition that
decision drives (§5).

### 1.2 What one maximum per dwell loses

Today's detector reports one cell. `det_result2d_t`
(`native/inc/detector2d/detector2d_core.h`) is one `(row, col, peak_mag,   noise_est, test_stat)`, and the acquisition engine's `acq_compute_stat`
(`native/src/acq/acq_core.c`) takes the same two maxima
[`dsss-acquisition.md`](dsss-acquisition.md) §9.1 describes — the
interpolated one to gate, the native one to report — and stops. The argmax
itself is a private loop in each of the two objects; only the CFAR
reference under it, `det_noise_estimate`, is shared through
`det_private.h`. There is no exclusion zone and no second peak, in either.

With `K` emitters up, the surface has `K` peaks, and a maximum reports the
strongest. The rest are not below threshold; they are simply not looked
at. In the burst use case that costs little — bursts are short and rarely
overlap in one channel. In the continuous case the strongest emitter is up
for hours, and every dwell for those hours reports it and nothing else, so
a second emitter rising beside it is **never** acquired while the first is
on the air. Nor does hand-off help: the assigned receiver goes on tracking
the first emitter, the searcher goes on re-detecting it at every data-free
window (the suppression-by-emitter `burst-bank.md` §11.3 asks the bank
for), and after the suppression drops that re-detection the dwell has
reported nothing at all. **The single maximum is the gap, and it is the
searcher's, not the bank's** — the bank's channel count partitions Doppler
into spans, but emitters within one span share a surface, and that is the
normal case here.

### 1.3 What the power spread decides

Two emitters at different Dopplers or code phases are two peaks on the
surface, and a detector that reports every peak above threshold finds
both — provided the second *is* a peak above threshold. A strong emitter
does not only put one peak on the surface: a 1023-chip Gold code's
cross-correlation with itself at every other lag is not zero, and the
maintainer's figure for that floor is **about −24 dB** below the peak
([`burst-bank.md`](burst-bank.md) §11.3, not re-derived here). That floor
lies across the whole surface — every Doppler bin, every code phase — so an
emitter weaker than the strongest by more than the floor plus the
detection margin is under the strongest one's sidelobes: it is not a peak,
and no peak detector reports it.

Two things follow, and they are why the mechanism forks on the spread:

- **The CFAR reference is right to rise.** `det_noise_estimate` measures
    the surface's floor, and with a strong emitter present that floor *is*
    the strong emitter's sidelobes. The threshold moves up with it, which
    is what CFAR means — the weak emitter is genuinely below the floor of
    the surface as it stands.
- **Only removing the strong emitter lowers that floor.** A peak list
    cannot; that needs cancellation, and cancellation needs a replica of
    the strong emitter — which is a different object with a different
    information source (§2.2).

So the decision is the emitters' **power spread**, `burst-bank.md` §11.4's
question 7, and it is open. Inside the floor a peak list suffices; beyond
it the weak emitters need the strong ones cancelled first. This page
covers both branches (§4), so that whichever way the number falls the page
already says what to build.

Two cautions about the number itself, for the work in §6. The −24 dB is
the three-valued bound for a full-period, zero-Doppler cross-correlation;
at a Doppler offset the correlation is partial-period and the bound does
not apply as stated. And it is a *maximum* over lags — the RMS floor of a
1023-chip code is nearer `1/√1023`, about −30 dB — so which of the two the
detector experiences is a measurement (§6 step 1), not a lookup.

### 1.4 The throughput floor

At the operating point one core has **100 ns per DDC-output sample** for
everything after the front end, and **77 ns per input sample** for the
front end itself; at the 30 MSa/s floor those are 43 and 33 ns.
"Comfortably" means a margin under that, and this page takes **half** as
the working target — the whole population inside 50 ns per output sample
per core at the operating point, 21 at the floor, across the cores the
application gives it — with the margin a number the benchmark reports,
not one it assumes. Equality with the budget is a failure by the
requirement's own words.

The decimation is only 1.3×, and that is the fact that shapes the cost:
**nothing runs at a fraction of the input rate.** The searcher and every
receiver run at 10 MSa/s, three-quarters of what the front end sees, so
the population's cost is `(searcher + 12 receivers + 10 replicas)` per
output sample, not that divided by anything.

The one measured number is an order of magnitude, not a price.
`burst-bank.md` §10.4 put a `DDC → BurstCapture` channel at **47–51 ns
per source sample** — but on the 3.069 Mcps waveform, with its own
decimation, and with the front end's share of it unmeasured, so it does
not transfer to this rate as a figure. Taken as it stands it is 62% of
one core at 13 MSa/s and 1.4× real time at 30, for one channel and no
receiver; which is enough to say the chain is priced near the budget
before the population is on it. Three things follow for the shapes:

- **One front-end DDC, shared.** There is one frequency channel, so the
    only stage at the input rate is one decimation, 13 to 10 MSa/s. The
    receivers take chip-rate input already (`AsyncDsssReceiver` ingests
    at `chip_rate · spc`), so they share the front end rather than each
    owning one.
- **The searcher is one window-tiled engine, not a DDC bank.** A bank of
    23 `DDC → search` channels at anything like 48 ns each is 14× real
    time at the operating point on one core and fits on no node; the
    continuous engine's own `window_bins` tiling covers the uncertainty
    in one engine at the same `D = 1` sensitivity (`burst-bank.md`
    §11.2), and with the peak list inside it (§3 (a)) it lacks nothing
    the bank had for this use case. That is a change to what §11.2
    assumed, and the throughput floor is what forces it.
- **The receivers are the population's cost, and they parallelize; the
    cancellation does not.** Twelve receivers at 10 MSa/s on the
    application's threads scale across cores; the replicas on the strong
    branch are subtracted on the searcher's path, serially, ten of them
    per block — so (iii)'s coupling has a per-sample price on one
    thread, and it is the searcher's.

What is not known is every per-stage number at this rate: the front-end
DDC per input sample; the searcher per output sample with the list and
23 tiles; one receiver per output sample; one replica per output sample.
§6 step 8 measures them, and the bench that does it must count what it
acquired and tracked beside the rate — a throughput that was reached by
missing an emitter is not a throughput.

______________________________________________________________________

## 2. The two mechanisms

### 2.1 The peak list with exclusion zones

The list is the maximum, iterated:

```text
repeat up to max_peaks times
  take the maximum of the surface
  if it is below eta · noise_est: stop
  report it (at its native row where the surface is interpolated)
  exclude ±1 Doppler bin × ±1 chip around it
```

At `D = 1` the surface is the native one: the engine interpolates only
the slow-time axis, and there is none, so the interpolated-vs-native
split of `dsss-acquisition.md` §9.1 collapses and the gate and the report
read the same cells. The Doppler axis is the `window_bins` tile index,
one row per tile, 4.89 kHz apart.

**Why one bin and one chip.** They are the widths of one emitter's main
lobe: an epoch's frequency response is the `sinc` of a 204.6 µs
rectangle, whose first nulls fall one tile (4.89 kHz) either side, and
the code's autocorrelation triangle reaches zero one chip either side of
its apex. Inside that zone the surface belongs to the emitter just reported —
its own shoulders would otherwise be the next "peak" — and outside it a
second emitter has its own maximum. The zone is therefore also the
detector's **resolution**: two emitters within one bin *and* one chip of
each other are one peak, distinguishable by nothing on this surface
(`burst-bank.md` §11.3), and that is a property of the code and the dwell,
not of the detector. In surface units the zone is `±interp` rows (one
row at `D = 1`) and `±spc` columns — two, here — circular in code phase;
on the native report it is `±1`
and `±spc`.

**The threshold does not change.** `eta` is sized from `N = searched_bins · code_bins` cells (`dsss-acquisition.md` §9.1); it counts the noise's
chances over the *surface*, and a second reported peak is another draw
from the same cells against the same gate, so the per-dwell false-alarm
event — *any* reported peak is false — is bounded by the same union.
Exclusion zones remove a few cells from the count, in the safe direction
and negligibly. What does change is the floor under a strong emitter
(§1.3): the reference rises, so does `eta·noise_est`, and false peaks in
the strong emitter's sidelobes are what §6 step 4 measures.

**Fixed size.** `max_peaks` is configuration, the result is an array of
that many `(doppler_bin, code_phase, peak_mag, test_stat)` entries plus a
count, ordered by `test_stat`; nothing allocates per dwell and nothing
grows with time — the duration rule of `burst-bank.md` §11.1. Today's
single-peak result is the same array at `max_peaks = 1`. The population
sizes it: on the branch where the searcher sees every emitter (§4) the
list must hold all ten plus the false peaks the gate admits, so
`max_peaks` is of order 16; on the branch where assigned emitters are
cancelled it holds only what rose since the last window, a few.

### 2.2 Cancellation

Cancellation subtracts a replica of a strong emitter so the surface
underneath it can be searched. The replica needs the emitter's code phase,
Doppler, amplitude and **carrier phase** — and, for any epoch that is not
that emitter's own data-free window, its **data**. That last item decides
the shape, because emitters' frames are not aligned: while emitter A is in
its data-free window, emitter B is carrying data, and B's contribution to
A's dwell is a data-modulated, straddle-lossed correlation whose sign flips
at a place the searcher does not know.

Where the replica's information comes from is therefore the design axis:

- **From the peak** (acquisition-side). The detection gives code phase and
    Doppler to within a cell; amplitude and phase must be estimated from
    the complex peak; the data is unknown. Exact only in the strong
    emitter's own data-free epoch — which is not, in general, the epoch
    being searched.
- **From the assigned receiver** (decision-directed). The receiver already
    tracking the strong emitter knows its chips, its carrier, its
    amplitude, and its decided bits, block by block, and refines all of
    them continuously. Its replica is exact to the tracker's own error,
    data included.

And where the subtraction happens is the second axis: on the **surface**
(subtract the emitter's known response, the code's autocorrelation across
lag times a `sinc` across Doppler, scaled by the complex peak — the radio
astronomer's CLEAN) or on the **samples** (regenerate the chip stream,
subtract, correlate again).

______________________________________________________________________

## 3. The shapes — where each piece lives

The peak list has one place it belongs and two it could be put:

|                                                 | mechanism                                                                                                                                                                                | fits                                                                                                                                                   | cost                                                                                                                                                   |
| ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **(a) one primitive under both detectors**      | a peak-list function beside `det_noise_estimate` in `det_private.h`: `(mag, ny, nx, gate, excl_rows, excl_cols, out[], max_peaks) → count`; both callers use it at `max_peaks = 1` today | one argmax instead of the two private copies; `CorrDetector2D` gains the list for free; the interpolated/native split stays where it is, in the caller | both result structs become an array plus a count, and every consumer of `acq_result_t` sees `n_peaks`                                                  |
| **(b) inside `acq_compute_stat` only**          | the engine's loop iterates with exclusion; `detector2d` stays single-peak                                                                                                                | the engine alone changes                                                                                                                               | a third private copy of the pick, and the two detectors' behaviours diverge on the same surface                                                        |
| **(c) a second pass over the surface, outside** | the bank asks the engine for its surface and picks peaks itself                                                                                                                          | no engine change                                                                                                                                       | the surface is the engine's scratch, not a product — exporting it is a copy of `ny·nx·interp` floats per dwell, and the gate's `eta` leaves the engine |

(a) is the repository's rule applied — fix it where the primitive is
defined, once — and the only one under which the burst detector and the
acquisition engine keep agreeing.

Cancellation is a separate object, and its shape follows its information
source:

|                                                             | mechanism                                                                                                                                                                                                                                       | fits                                                                                                                                                                                                                                       | cost                                                                                                                                                                                                                                                 |
| ----------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **(i) surface CLEAN, from the peak**                        | subtract `A·acf(τ − τ_i)·sinc(f − f_i)` from the *complex* surface for each strong peak, then re-pick                                                                                                                                           | no second correlation; stays inside the engine                                                                                                                                                                                             | needs the complex surface where the engine keeps `\|·\|`; the response is exact only in the strong emitter's own data-free epoch, and a data transition inside the dwell leaves a residual the model does not have                                   |
| **(ii) sample SIC, from the peak**                          | regenerate the strong emitter from its detection, subtract from the epoch, correlate again                                                                                                                                                      | one object, no dependency on the tracker pool                                                                                                                                                                                              | one extra correlation per cancelled emitter per dwell; the same unknown-data residual as (i); amplitude and phase from a single cell's estimate                                                                                                      |
| **(iii) sample cancellation fed by the assigned receivers** | `DDC → cancel(assigned) → search`: each assigned receiver publishes its replica for the block (or the estimates that make one: code phase, Doppler, amplitude, phase, decided chips); the searcher subtracts every replica before it correlates | the only replica that is right through data; makes the searcher see **exactly what is not assigned**, which retires the suppress-by-emitter table (`burst-bank.md` §11.3) — an assigned emitter is not re-detected because it is not there | couples the searcher to the receiver pool on the push path; a receiver that has lost lock publishes a wrong replica, so the subtraction must be lock-gated; one replica per assigned emitter per block; and the *receivers* still see the raw stream |

(iii) is the shape the lifecycle already asks for. The receivers own the
emitters and keep tracking them regardless of what the searcher does; the
searcher wants to see only what they do not own; and only they know the
data — `AsyncDsssReceiver`'s track stage holds exactly the replica's
ingredients per block: the live carrier loop's phase and frequency, the
`Dll`'s code phase, the despreader's amplitude, and the decided symbols.
It is also the option that makes the two branches of §4 one mechanism at
two settings. Its cost is a real coupling — whoever holds the receiver
pool must also stand on the searcher's push path — which is why
`burst-bank.md` §11.4's question 5 (who owns the lifecycle) becomes
load-bearing the moment the strong branch is chosen, and not before.

A refinement (iii) opens but this page does not take: a receiver can be
fed the stream with every *other* assigned emitter cancelled, which lowers
its own floor as well. That is the receivers' concern, on their own path,
and it changes nothing about the searcher.

______________________________________________________________________

## 4. The two branches

Both branches share the peak list (a) and the assigned-emitter table the
bank keeps in any case: which emitters have a receiver, at what Doppler
and code phase **now** (the receiver's estimate, since an emitter drifts at
up to 500 Hz/s between windows, not the detection's). The branches differ
in what the searcher is allowed to see.

**Spread inside the floor — the list is enough.** The searcher sees every
emitter, assigned or not, and reports every peak above `eta`. The bank
drops any peak within one exclusion zone of an assigned emitter's current
estimate and hands the rest to the policy. An assigned receiver is never
touched by a re-detection of its own emitter. What has to hold: no
unassigned emitter above the floor is missed while a stronger one is up
(§6 steps 2–3), and the re-detection of an assigned emitter never becomes
a second receiver (§6 step 7).

**Spread beyond the floor — cancel, then list.** The searcher's input has
every lock-gated assigned replica subtracted (iii), and then runs the same
list. The assigned table does the same job as before, now only as a guard
against the residual: a cancelled emitter that is imperfectly cancelled
leaves a peak at its own coordinates, and the zone around the receiver's
estimate is what keeps that residual from becoming a detection. What has
to hold: the residual after cancellation sits below the unassigned
emitters the application needs to find (§6 step 5).

The branch is chosen by one number — the application's operating spread
against the knee §6 step 3 measures — and the second branch strictly
contains the first, so building the list first is right either way.

______________________________________________________________________

## 5. The release — the lock detector decides "gone"

"Until they are gone" is a decision the receiver makes about itself, and
the pieces of it exist. `AsyncDsssReceiver` carries two de-chattered lock
flags, each a `lockdet` — level hysteresis between a declare and a drop
threshold, time hysteresis of consecutive looks either way, a NaN look
counted as a miss (`native/inc/lockdet/lockdet_core.h`):

- **Code lock**, `get_code_locked()`: the live `Dll`'s own CFAR-based,
    verify-counted detector — "am I despreading". This is the fundamental
    DSSS lock: an emitter that leaves takes its code with it, and the
    correlation at the tracked code phase and Doppler falls to the floor.
- **Symbol lock**, `get_locked()`: the BPSK statistic `cos(2φ)` over the
    emitted symbols, SNR-weighted over a 30-symbol dwell, declared after
    30 consecutive symbols at or above 0.5 and dropped after 15 consecutive
    below 0.3 (`ASYNC_DSSS_RX_LOCK_*`). This is the health of the *carrier*
    leg: a cycle slip or a deep fade drops it while the code is still
    being despread.

What is missing is the transition. Today a receiver whose flags fall keeps
running its loops on noise, and the only exit is `reset()`, which returns
to *searching* — a state the hand-off mode of §1.1 does not have.

**The rule.** An emitter is gone when **code lock drops and stays
dropped** for a confirm interval; symbol lock alone is a degrade, not a
release. Code lock is the right flag because it is the one that measures
presence rather than quality: a receiver can lose the carrier and re-lock
it on the same emitter (the loops are designed to pull in), but it cannot
re-lock a code that is no longer on the air. Symbol lock stays on the
record as health — a long stretch of code-locked but symbol-unlocked
tracking is a receiver reporting that it is holding an emitter it cannot
decode, which the application may want to know and this page does not
decide.

**The transition.** Hand-off mode adds a fourth state, **lost**, beside
searching / refining / tracking, and the receiver enters it on the rule
above. In it the loops stop updating, the replica (§3 (iii)) is no longer
published — the lock gate that (iii) already needs is the same flag, so
publication stops at the *drop*, before the confirm interval has run —
and the receiver reports lost to whoever holds the pool. The holder then
**releases the assignment**: the emitter leaves the assigned table, so the
searcher may report those coordinates again, and the receiver is reset to
the hand-off mode's idle — *waiting for a seed*, not searching — for the
pool to reuse. Nothing else moves: the searcher was never told to stop
looking there and the other receivers are untouched. The one
re-assignment the lifecycle permits is this one: an emitter released while
in fact still present is re-detected at its next data-free window and
seeded into a fresh receiver, which is a recovery, not a hand-back.

**What the interval costs, and what it buys.** Against on-times of 5 to
15 minutes, release latency is nothing: the symbol detector's drop is 15
symbols — milliseconds at any data rate in the thousands of symbols per
second — and any confirm interval under a second is well under 1% of the
shortest on-time. The number that matters
is the other one, the **false release**. A receiver that releases an
emitter still on the air loses that emitter's data until the next
data-free window plus a refine (the cadence of `burst-bank.md` §11.4
question 4), and on the cancellation branch its replica leaves the
searcher's input for the same interval, so the floor rises under every
weaker emitter for a frame. The confirm interval is therefore sized from
a false-release budget — far rarer than once per on-time, per receiver —
in exactly the vocabulary `lockdet` documents: at the per-look miss
probability the tracked C/N0 gives, `n_down` consecutive misses set the
false-drop rate, and `det_verify_count()` sizes `n_down` against the
budget. Both the miss probability and the resulting interval are
measurements (§6 step 6).

**The pool.** Ten emitters at once plus the receivers still inside a
confirm interval on emitters that have just left: at one departure a
minute and a confirm interval of a second, the headroom is one. A pool of
about twelve hand-off-mode receivers, each a tracker chain on the
application's threads beside the searcher's own cost (`burst-bank.md`
§11.2), is the whole population.

**The read-back.** Whoever holds the pool needs to know, for each
receiver, which signal it is tracking, for how long, and in what
condition (maintainer, 2026-09-02). The facts have two owners, and the
split falls out of who produced each one:

- **The orchestrator owns the assignment.** It handed the seed to the
    receiver, so it holds the `DetectionEvent` verbatim — `timestamp_ns`,
    `samples_consumed`, `chip_phase`, `doppler_hz_est`, `cn0_dbhz_est` —
    beside the receiver it went to. It fed every sample since, so it holds
    the sample count at assignment and the count now; duration is their
    difference over the rate, the repository's `dp_sample_clock_t`
    arithmetic, replay-safe. And it recorded the state changes it was
    told about — refining to tracking, tracking to lost — with the sample
    count at each. Nothing here needs the receiver to remember its own
    history, which keeps the receiver thin: it tracks; the orchestrator
    keeps the books. This is the assigned table of §4 with three more
    columns, and it is what question 5's holder holds.
- **The receiver owns its condition.** Only it knows where the emitter
    is now — the live carrier loop's Doppler, the `Dll`'s code phase, the
    C/N0 the despreader sees (the drift since the seed is that against
    the orchestrator's row) — and its health: the state it is in, both
    lock flags, the symbol-lock metric against its declare threshold, the
    residual carrier errors the header already exposes, and, in lost, the
    samples since the code flag dropped. Today that is a scatter of
    getters — `get_locked`, `get_code_locked`, `get_lock_metric`,
    `get_car_nco_freq`, and the rest — each a separate call, so a reader
    that wants one consistent picture across a `push` on another thread
    cannot get one. The shape that fits is **one status record, returned
    by value** — the `measure` objects' `single` record (`ToneMetrics`), a
    jm-generated structseq over a C struct — read on demand and never
    pushed.

The orchestrator's *now* columns are refreshed from the receiver's record
at whatever cadence it reads, and the exclusion zone of §4 is keyed on
those, not on the seed. So one read per receiver per window is the
minimum, and the table is the join of the two owners' facts.

The record is a read of live state, distinct from `get_state()`: the
bytes triplet is for resuming the receiver elsewhere, the record is for
describing it here, and the two must not be confused — a record that
tried to be both would be a serialized blob a human cannot read. On the
cancellation branch the replica output is a third thing again, per block
and on the push path, and rides neither.

______________________________________________________________________

## 6. The work that answers it

1. **Measure the floor one emitter puts on the surface.** One emitter,
    no noise, design C/N0; tabulate the surface's maximum and RMS relative
    to the peak over `(Δf, Δτ)` — at zero Doppler across every lag, and at
    Doppler offsets of 0.5, 1, 2, 4 bins — with and without a data
    transition inside the epoch. Beside it, `noise_est` with and without
    the emitter present: how far the CFAR reference rises. Expected: the
    three-valued −24 dB at zero Doppler, something between that and −30 dB
    elsewhere. This is the number the branch decision uses, and it is the
    engine's, so it belongs in `acq`'s characterization.
1. **Separability of two equal emitters.** Two emitters at the design
    C/N0 separated by `Δf ∈ {0.5, 1, 2, 4}` bins and
    `Δτ ∈ {0.5, 1, 2, 4}` chips, 200 trials per cell: Pd of *both* under
    the list, and the coordinates each is reported at. Expected: both
    found outside the exclusion zone, one found inside it, and no cell
    where the second is reported off its own coordinates by more than a
    cell. This pins the zone's edges as the resolution.
1. **The power-spread knee, list only.** Strong emitter fixed at the
    design C/N0, weak stepped from 0 to −40 dB below it in 3 dB, 200
    trials each, at a `Δf`/`Δτ` well outside the zone: Pd(weak). Expected
    a knee at the floor plus the detection margin. **The knee is the
    decision** — an operating spread inside it means branch one and no
    cancellation object.
1. **Pfa under the list.** Pure noise, `max_peaks ∈ {1, 4, 8}`, the same
    frame count `dsss-acquisition.md` §9.1 used: realized per-dwell Pfa
    against configured. Expected unchanged, since `N` counts cells, not
    peaks. Then with one strong emitter present: the rate of false peaks
    in its sidelobes — if it is not the configured rate, the reference is
    not tracking the raised floor and that is a CFAR finding, not a
    list finding.
1. **Cancellation depth, (iii).** An assigned receiver locked on the
    strong emitter; measure the residual after subtraction, relative to
    the strong peak, against C/N0 and against the receiver's steady-state
    phase and timing error; then re-run step 3 with cancellation on. The
    residual is the new floor and the distance it moves the knee is what
    the object buys. Run it once with (ii) as the control: the gap
    between the two is the price of not knowing the data.
1. **The release.** A hand-off-mode receiver locked on one emitter at the
    design C/N0; the emitter is switched off mid-track, faded 10 and 20 dB
    for a second, and given one carrier cycle slip, 100 trials each:
    the time from the event to code-lock drop and to symbol-lock drop, and
    whether the code flag survives the fade and the slip. Then, with the
    emitter left on for the length of an on-time, the per-look miss
    probability of each flag — the number `det_verify_count()` turns into
    the confirm interval for a false-release budget. Expected: the code
    flag rides through the slip, both flags drop within tens of
    milliseconds of switch-off, and the deep fade is the case that
    decides the interval.
1. **The lifecycle soak.** The population of §1.1 — one emitter always
    on, up to ten, on-times drawn around 5 to 15 minutes — at random
    Dopplers within one span and a spread on each side of the knee: each
    is acquired once, assigned once, tracked by the same receiver until
    it leaves, released by the rule of §5, and re-acquired on return; no
    receiver is ever assigned twice to a live emitter, no emitter above
    the floor is missed while others are up, and the pool never exceeds
    twelve. An hour sees about sixty arrivals at the full population,
    enough to count misses and false releases; the hours-long form with
    the memory and scratch checks is `burst-bank.md` §11.1's duration
    requirement and runs once the bank exists.
1. **The budget, per stage.** Its own bench target, on one core,
    minimum of runs, at the operating point's numbers (§1.1): the
    front-end DDC in ns per input sample at 13 MSa/s; then, in ns per
    output sample at 10 MSa/s, the window-tiled searcher with 23 tiles
    and `max_peaks = 16`, one hand-off-mode receiver tracking, and one
    replica subtraction. Then the whole population — the front end, the
    searcher, ten receivers, and on the strong branch ten replicas — on
    the core count the application gives, reported as the fraction of
    real time **beside the count of emitters acquired and tracked** in
    the same run, twice: at the operating point and at the 30 MSa/s
    floor (the same chain fed 2.3× faster). Target: under 0.5 at both.
    At 1.0 the requirement is missed by its own words, and the stage that
    owns the excess is the next thing to attack — §1.4's channel number
    says today's chain is already priced near it.
1. **Decide by the spread.** The application's operating spread
    (`burst-bank.md` §11.4 question 7) against step 3's knee: inside,
    branch one ships and (iii) is not built; beyond, (iii) is built and
    step 5's residual is the number its characterization pins.

Steps 1–4 are Python over the shipped engine plus the peak-list primitive,
and are the same harness the burst characterization already runs. Steps
5–6 need the hand-off-mode `AsyncDsssReceiver` (§1.1) with the lost state
of §5 and, for step 5, a replica output it does not have today. Steps 7–8
need the orchestrator holding the population.

______________________________________________________________________

## 7. What this page does not settle

Of `burst-bank.md` §11.4's open questions, this page answers 6 (the
population, §1.1) and designs the receiver half of 5 (the release, §5).
Still open, and none of them blocking the list: the frame epoch (3), the
frame cadence (4) — which is what a false release costs, so it prices §5's
budget — and the other half of 5, who holds the pool and the assigned
table. That holder becomes load-bearing only on the strong branch, because
(iii) puts it on the searcher's push path. What the receiver still lacks
is implementation, not design: the hand-off-mode constructor (§1.1), the
lost state and the idle it resets to, the status record (§5), and, for
the strong branch, a replica output.

______________________________________________________________________

## 8. See also

- [`burst-bank.md`](burst-bank.md) — §11, the continuous use case this
    page completes, and §11.4's open questions.
- [`dsss-acquisition.md`](dsss-acquisition.md) — §9.1, the CFAR
    vocabulary used here: the sum over looks, the maximum over cells,
    `N_eff`, and the interpolated-vs-native split every peak keeps.
- [`coarse-channel.md`](coarse-channel.md) — the channel as an object,
    which is what carries the searcher.
- [`async-dsss-spec.md`](async-dsss-spec.md) — the waveform, and the
    `DetectionEvent` each reported peak becomes.
