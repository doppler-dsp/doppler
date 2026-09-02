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
decision drives (§6).

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

Two cautions about the number itself, for the work in §5. The −24 dB is
the three-valued bound for a full-period, zero-Doppler cross-correlation;
at a Doppler offset the correlation is partial-period and the bound does
not apply as stated. And it is a *maximum* over lags — the RMS floor of a
1023-chip code is nearer `1/√1023`, about −30 dB — so which of the two the
detector experiences is a measurement (§5 step 1), not a lookup.

______________________________________________________________________

## 2. The two mechanisms

### 2.1 The peak list with exclusion zones

The list is the maximum, iterated:

```text
repeat up to max_peaks times
  take the maximum of the interpolated surface
  if it is below eta · noise_est: stop
  report it at its native row (the §9.1 split, per peak)
  exclude ±1 native Doppler bin × ±1 chip around it
```

**Why one bin and one chip.** They are the widths of one emitter's main
lobe: a slow-time `sinc` has its first nulls one bin either side, and the
code's autocorrelation triangle reaches zero one chip either side of its
apex. Inside that zone the surface belongs to the emitter just reported —
its own shoulders would otherwise be the next "peak" — and outside it a
second emitter has its own maximum. The zone is therefore also the
detector's **resolution**: two emitters within one bin *and* one chip of
each other are one peak, distinguishable by nothing on this surface
(`burst-bank.md` §11.3), and that is a property of the code and the dwell,
not of the detector. In surface units the zone is `±interp` rows and
`±spc` columns, circular in code phase; on the native report it is `±1`
and `±spc`.

**The threshold does not change.** `eta` is sized from `N = searched_bins · code_bins` cells (`dsss-acquisition.md` §9.1); it counts the noise's
chances over the *surface*, and a second reported peak is another draw
from the same cells against the same gate, so the per-dwell false-alarm
event — *any* reported peak is false — is bounded by the same union.
Exclusion zones remove a few cells from the count, in the safe direction
and negligibly. What does change is the floor under a strong emitter
(§1.3): the reference rises, so does `eta·noise_est`, and false peaks in
the strong emitter's sidelobes are what §5 step 4 measures.

**Fixed size.** `max_peaks` is configuration, the result is an array of
that many `(doppler_bin, code_phase, peak_mag, test_stat)` entries plus a
count, ordered by `test_stat`; nothing allocates per dwell and nothing
grows with time — the duration rule of `burst-bank.md` §11.1. Today's
single-peak result is the same array at `max_peaks = 1`.

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
`Dll`'s code phase, the despreader's amplitude, and the decided symbols. It is also the option that makes the two branches of §4 one
mechanism at two settings. Its cost is a real coupling — whoever holds the
receiver pool must also stand on the searcher's push path — which is why
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
(§5 steps 2–3), and the re-detection of an assigned emitter never becomes
a second receiver (§5 step 6).

**Spread beyond the floor — cancel, then list.** The searcher's input has
every lock-gated assigned replica subtracted (iii), and then runs the same
list. The assigned table does the same job as before, now only as a guard
against the residual: a cancelled emitter that is imperfectly cancelled
leaves a peak at its own coordinates, and the zone around the receiver's
estimate is what keeps that residual from becoming a detection. What has
to hold: the residual after cancellation sits below the unassigned
emitters the application needs to find (§5 step 5).

The branch is chosen by one number — the application's operating spread
against the knee §5 step 3 measures — and the second branch strictly
contains the first, so building the list first is right either way.

______________________________________________________________________

## 5. The work that answers it

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
1. **The lifecycle soak.** Three emitters cycling in and out over the
    run, at random Dopplers within one span and a spread on each side of
    the knee: each is acquired once, assigned once, tracked by the same
    receiver until it leaves, and re-acquired on return; no receiver is
    ever assigned twice to a live emitter, and no emitter above the floor
    is missed while another is up. Minutes first; the hours-long form
    with the memory and scratch checks is `burst-bank.md` §11.1's
    duration requirement and runs once the bank exists.
1. **Decide by the spread.** The application's operating spread
    (`burst-bank.md` §11.4 question 7) against step 3's knee: inside,
    branch one ships and (iii) is not built; beyond, (iii) is built and
    step 5's residual is the number its characterization pins.

Steps 1–4 are Python over the shipped engine plus the peak-list primitive,
and are the same harness the burst characterization already runs. Step 5
needs the hand-off-mode `AsyncDsssReceiver` (§1.1) and a replica output
it does not have today. Step 6 needs the bank.

______________________________________________________________________

## 6. What this page does not settle

The open questions of `burst-bank.md` §11.4 stay open and none of them
blocks the list: the frame epoch (3), the lifecycle owner (5), emitter
counts and duration (6), the frame cadence (4). Two are touched here.
Question 5 becomes load-bearing only on the strong branch, because (iii)
puts the receiver pool on the searcher's push path. And the receivers'
own **loss decision** — the "until they are gone" of the lifecycle — has
a detector but no transition: `AsyncDsssReceiver`'s symbol lock detector
says *locked* or not, and its state machine has no lost state to move to
when it stays not (`burst-bank.md` §11.3). That is the receiver's gap,
alongside the hand-off-mode constructor of §1.1, and the searcher depends
on it only in that a receiver which cannot say it has lost its emitter can
never release the assignment.

______________________________________________________________________

## 7. See also

- [`burst-bank.md`](burst-bank.md) — §11, the continuous use case this
    page completes, and §11.4's open questions.
- [`dsss-acquisition.md`](dsss-acquisition.md) — §9.1, the CFAR
    vocabulary used here: the sum over looks, the maximum over cells,
    `N_eff`, and the interpolated-vs-native split every peak keeps.
- [`coarse-channel.md`](coarse-channel.md) — the channel as an object,
    which is what carries the searcher.
- [`async-dsss-spec.md`](async-dsss-spec.md) — the waveform, and the
    `DetectionEvent` each reported peak becomes.
