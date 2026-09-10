# Tracking a Population of DSSS Emitters with `AsyncDsssPool`

`AsyncDsssPool` holds the whole multi-emitter lifecycle behind one
`push()`: one searcher over the Doppler uncertainty, a pool of cell
receivers created idle, the assigned table that keeps a re-detection from
becoming a second receiver, and the event log that records every
transition at the sample it happened. Feed it the stream one block at a
time; read each slot's status and symbols after every push. This page is
how to use it. What it is and why it is shaped this way is
[the design page](../design/async-dsss-receiver.md); the numbers behind
every claim are [the measurement record](../design/async-dsss-receiver-measurements.md);
the runnable, tested walk-through of one lifecycle is
[the gallery page](../gallery/async-dsss-pool.md).

## When it is the right object

The use case: several emitters on the air at once on **one** spreading
code in **one** band, told apart by Doppler and code phase alone, each
transmitting continuously with a **code-only window** every frame,
arriving and leaving at their own times, for hours. One `Acquisition`
finds the strongest peak per dwell and one `AsyncDsssReceiver` tracks one
signal; the pool is what turns those into a population: every peak is
listed, every unassigned one seeds a free receiver, every receiver is
released when its own rule says its emitter is gone, and the searcher
never stops.

If there is one emitter, use [`AsyncDsssReceiver`](../api/python-dsss.md)
directly (its searching flavor has the search built in). If the signals
are bursts, the burst chain is
[`DsssBurstReceiver`](../design/dsss-burst-receiver.md).

## The words

Nine words carry the object, in the order the stream produces them.

- **cell** — one test statistic value, at one code phase and one
    frequency, on the surface the searcher dumps for one coherent block
    (its dwell).
- **hit** — a cell over the threshold, listed as one detection record: a
    Doppler bin, a code phase, the statistic and a C/N0 estimate.
- **seed** — the three numbers a hit hands one receiver: chip phase,
    Doppler estimate, C/N0 estimate.
- **slot** — one of the pool's `n_slots` receivers together with the table
    row that holds it, addressed by index in `status()` and `symbols()`.
- **row** — one entry of the assigned table, one per slot: a slot's seed,
    and the coordinates the pool currently has for its emitter.
- **zone** — the code phase within one chip of a live row's, circular. A
    hit inside it is that row's emitter's own.
- **release** — the pool clearing a row and resetting its receiver to
    idle.
- **stint** — one continuous on-air period of one emitter.
- **soak** — one run of the whole population, long enough to contain
    arrivals, departures and returns.

Two of those words are used elsewhere in a second sense. On this page
they mean what is written above and nothing else:

- **row** is the assigned table's row. The searcher's surface has rows
    too — its Doppler axis, one row per resolvable frequency,
    `doppler_res_hz` apart — and inside the block searcher each tile's
    slow-time rows fold onto that axis. This page calls that axis the
    **Doppler bin** and keeps *row* for the table.
- **cell** is one value on the searcher's surface.
    `CellAsyncDsssReceiver` is named after that cell — it is the receiver
    a searcher's cell drives, on the searcher's timing, instead of
    closing a code loop of its own. It has nothing to do with a radio
    cell.

## The objects inside, and what each decides

| object                              | in the pool                                                                                                                      | decides                                                                                                         |
| ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `Acquisition` (continuous)          | one, over `doppler_uncertainty`, in coherent blocks of `D` epochs inside the code-only window, its tiles fanned across `threads` | every hit above the gate, per dwell, up to `max_peaks`                                                          |
| `CellAsyncDsssReceiver` × `n_slots` | created idle; a seed starts the pull-in → track on the searcher's timing; every one is fed every block                           | its own lock flags, and *lost* — both flags down for `lost_confirm_s`                                           |
| the assigned table                  | one row per slot: the seed, and the receiver's live Doppler and chip phase while its loops are locked                            | a hit inside a live row's zone is that emitter's own and seeds nothing                                          |
| `EventLog` (attached)               | borrowed by `set_event_log()`                                                                                                    | nothing — it records `seeded`, `tracking`, `degrade`, `lost`, `released`, `dropped` with the slot's coordinates |

The pool itself decides two things: which free slot a surviving hit seeds
(or `dropped` when there is none), and the **on-time release** — a slot
held past `max_emitter_on_time_secs` is released and its emitter is a new
detection at its next window.

Every slot is a `CellAsyncDsssReceiver`: its code loop held from the seed
and corrected once every `D` code periods on the searcher's own timing, at
`gain` (default 1/8) after `pullin_intervals` (default 4) at gain 1; the
seed's carrier residual is estimated on the receiver's own despread stream
and folded once. The constructor refuses a searcher a cell receiver cannot
take. Two bounds: a block depth above 1 (`code_only_epochs > 1`, with a
window in the waveform), and a Doppler bin no wider than four times the
carrier loop's pull-in bound — 97.8 Hz at the 5 Mcps / Gold-1023
geometry, so `D ≥ 13` there — so that a seed half a bin off lands inside
the loop's pull-in. The operating point's `D = 154` gives 31.7 Hz bins.

## One slot's lifecycle

```text
idle ──seed()──▶ refining ──pulled in──▶ tracking ──both flags down
 ▲                                          │       for lost_confirm_s
 │                                          ▼
 └───────── released (reset to idle) ◀──── lost
```

- **`seeded`** — a listed hit outside every live row's zone, into a free
    slot. The seed carries the searcher's Doppler (to one bin), chip
    phase (to half a chip) and C/N0 estimate.
- **`refining`** — the pull-in. The receiver's `Dll` is held on the
    searcher's block timing from the first sample, and the seed's carrier
    residual is estimated on the receiver's own despread stream and folded
    once. This one is the receiver's own state, read off `status()`; the
    pool logs no transition for it.
- **`tracking`** — the pull-in has folded its estimate and a lock flag
    is up; from here `status()` reports the loops' own Doppler and chip
    phase, and `symbols()` returns what the receiver decided on the last
    push.
- **`degrade`** — one flag down. Nothing is acted on; the loops keep
    running.
- **`lost`** — both flags down without a break for `lost_confirm_s`.
    While that clock runs the loops **hold** what they settled on rather
    than run on noise, so a departed emitter's receiver stands where the
    emitter left it and does not sweep onto a neighbour's code.
- **`released`** — the pool clears the row and resets the receiver to
    idle, either on `lost` or on the on-time cap. An emitter released while
    still on the air is re-acquired at its next code-only window into
    whichever slot is free: the one re-assignment the lifecycle permits.

The searcher's false alarms are part of this lifecycle by design: at
`pfa = 1e-3` a noise hit seeds a free slot every few hundred milliseconds,
pulls in to nothing, reports tracking with both flags down and is released
one interval later. That is why an expectation about the pool is never a
count of assigned slots — a noise seed occupies one exactly as an emitter
does — and why `n_slots` carries headroom over the population.

Two rules meet here and are easy to merge into one. **The pool occupies a
slot by the code phase it was handed**, and nothing else: that is why the
zone it projects covers that code phase at *any* Doppler, since a tracked
emitter's own data blocks put smeared copies of it at its own phase rows
away (see the design page, §8.2). **An observer identifies whose slot it
is by both coordinates** — the seed's Doppler within the searcher's row
and its chip phase within a chip — because it is matching a slot against
an emitter it already knows. The first is the mechanism. The second is how
a test reads it, and it needs a truth the pool does not have.

## Configuring it

Every number is a constructor parameter; the defaults are the design's
operating point, and the searcher's and the receivers' own parameters pass
through untouched.

| parameter                              | meaning                                                                                                                                                                                                                               | default         |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| `code`                                 | the spreading code, 0/1 chips; every emitter is on it                                                                                                                                                                                 | required        |
| `chip_rate`, `symbol_rate`, `spc`, `m` | the waveform: chips per second, data symbols per second, samples per chip, the PSK order of the receivers                                                                                                                             | 1e6, 1000, 2, 2 |
| `cn0_dbhz`, `pfa`, `pd`                | the sensitivity the searcher and the receivers are sized for, and the searcher's per-dwell false-alarm and detection targets                                                                                                          | 55, 1e-3, 0.9   |
| `doppler_uncertainty`                  | the searcher's span, Hz one-sided; it tiles the span in windows one epoch rate wide                                                                                                                                                   | 100             |
| `code_only_epochs`                     | the whole code epochs the waveform's code-only window holds at any chip phase; sizes the coherent depth `D`, which must exceed 1 — and the Doppler-bin bound above puts its floor at 13 at the 5 Mcps / Gold-1023 geometry            | 813             |
| `doppler_rate`                         | the Doppler rate the depth is bounded against, Hz/s (0 = no bound)                                                                                                                                                                    | 0               |
| `max_peaks`                            | the hit list's capacity per dwell: the population plus the false hits the gate admits                                                                                                                                                 | 16              |
| `n_slots`                              | receivers held; the population plus release headroom                                                                                                                                                                                  | 12              |
| `threads`                              | the workers the searcher's tiles and the receivers run across: 1 is serial, ≤ 0 takes the online core count. The result is bit-identical at any count                                                                                 | 1               |
| `carrier_freq_hz`                      | the RF carrier the Doppler is physically coupled to; told, the searcher walks its blocks by each tile's code rate, advances the seed by half a dwell's drift, and the receivers aid their code loops from the carrier (0 = uncoupled) | 0               |
| `lost_confirm_s`                       | the release rule's interval: both flags down this long is *lost*; longer than the longest fade the link must ride                                                                                                                     | 2.0             |
| `max_emitter_on_time_secs`             | the on-time cap: a slot held this long is released and its emitter re-acquired; 0 never releases for time                                                                                                                             | 900             |
| `segments`, `sps`, `differential`      | the receivers' despreader partials per code period, the demodulator's samples per symbol, differential decoding                                                                                                                       | 4, 8, 0         |
| `gain`, `pullin_intervals`             | the receivers' correction: chips per chip of the interval-mean discriminator after the pull-in, and the intervals at gain 1 before it                                                                                                 | 0.125, 4        |

Anything the constructor cannot accept — an empty code, a non-positive
rate, `gain` outside `(0, 1]`, a searcher whose depth or Doppler bin a
cell receiver cannot take — is a `ValueError` naming the whole admissible
set, not a `MemoryError`.

One knob is a method, because it is decided after the population is
known: `set_event_log(log)` attaches the run's log (`None` detaches).

A physically-coupled carrier (`carrier_freq_hz > 0`) is the setting that
matters most for a moving emitter: without it a 50 kHz Doppler smears the
searcher's coherent block by three chips and the receivers' code loops
have no aid.

## The call surface

`push()` is the whole cycle; everything else reads back or manages the
object's life.

- **`push(x) -> int`** takes one block of `complex64` and returns the
    number of assigned slots after it. In order: the searcher; the table
    refreshed; every hit in a live row's zone dropped as that emitter's
    own; each survivor seeded into a free slot or counted `dropped`; every
    receiver fed, across the pool's threads; every receiver that reports
    lost, or has held its slot past the on-time cap, released. Any block
    size is accepted — a hit decided inside the block is referred back to
    the block's start before it seeds, on the dilated clock when the
    carrier is known.
- **`status(slot) -> PoolSlot`** returns one record by value, allocation
    free. A slot outside `[0, n_slots)` returns a zero record with
    `state == -1` rather than raising. Read it after every push; it is not
    `get_state()`, which is the bytes for resuming the pool elsewhere.
- **`symbols(slot) -> NDArray[complex64]`** is what the slot's receiver
    decided on the last push, copied out of the pool's own buffer, which
    the next `push()` overwrites. Empty while the slot is idle, refining
    or lost. Pass `out=` to write into an array you own.
    `symbols_max_out()` is the per-slot capacity — grown with the largest
    block pushed so far, and 0 before the first push. A slot outside
    `[0, n_slots)` raises `ValueError`.
- **`reset()`** releases every slot and starts over: the searcher reset,
    every receiver back to idle, the table cleared, the counters zeroed.
    An attached log stays attached and nothing is logged — a reset is the
    holder's decision, not an emitter's transition.
- **`set_event_log(log)`** attaches or (with `None`) detaches the log. It
    is borrowed, never owned: the holder opens, finalizes and closes it. A
    log that has already failed keeps failing, and the pool counts the
    transition either way.
- **`state_bytes()` / `get_state() -> bytes` / `set_state(blob)`** are the
    checkpoint triplet. The blob carries the pool's counters and table, the
    searcher's state and every receiver's, each self-validating; the
    geometry, the slot count, the carrier and the attached log are restored
    by the constructor instead, and a blob from a pool of another slot
    count is rejected. The last push's symbols are scratch and do not
    survive. A mid-stream split resumes bit for bit
    ([Checkpoint & Resume](state-serialization.md)).
- **`destroy()`**, or a `with` block, releases the C resources at a
    definite point. Otherwise they go at collection.

The counters are properties: `n_slots`, `n_assigned`, `dropped` (hits
that found no free slot), `events` (transitions since create or reset,
logged or not), and `samples_consumed` — the stream position every event
is stamped at. Two more report what the searcher chose from the geometry:
`doppler_res_hz`, the width of a Doppler bin and so the resolution a
seed's Doppler is reported at, and `coherent_bins`, the depth `D`.

`status()` returns a `PoolSlot`, a named tuple of seventeen fields
(`src/doppler/dsss/dsss.pyi`):

| field                                                    | meaning                                                                                                          |
| -------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `slot`                                                   | the index asked for                                                                                              |
| `assigned`                                               | 1 while a receiver holds an emitter                                                                              |
| `state`                                                  | the receiver's state: 1 refining, 2 tracking, 3 idle, 4 lost; −1 for a slot that does not exist                  |
| `seed_sample`                                            | stream position the row was assigned at                                                                          |
| `seed_chip_phase`, `seed_doppler_hz`, `seed_cn0_dbhz`    | the seed, verbatim: chips, Hz, dB-Hz                                                                             |
| `doppler_hz`, `chip_phase`, `code_rate`                  | where the emitter is now: Hz, chips, chips advanced per nominal chip                                             |
| `cn0_dbhz_est`                                           | the C/N0 estimate, dB-Hz                                                                                         |
| `code_locked`, `locked`, `lock_metric`                   | the presence flag, the symbol-lock flag, and the statistic the second is a threshold on                          |
| `state_samples`, `both_down_samples`, `assigned_samples` | three clocks in input samples: since the state was entered, since both flags dropped, since the row was assigned |

**The event log** is a flat, tail-able JSON-lines file while the run is
live and a SigMF sidecar when finalized; every line carries
`core:sample_start`, `core:label` and the slot's `doppler:*` fields, plus
`reason` (`lost` or `on_time`) on a `released`. Replay and live runs
produce identical records, because nothing below the pool sees a time.

## A worked run

The gallery page walks one lifecycle end to end with the figure; the
regions below are its tested source. The geometry — the code, the rates,
the searcher's span and a release interval short enough to watch:

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:geometry"
```

The stimulus is the shipped waveform: two emitters on one code at their
own Doppler and code phase, one leaving and returning, noise at the sum.

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:stimulus"
```

The pool, fed one epoch per push, with the log attached and every slot's
status read after each push. The `owner()` helper is the *identification*
above rather than the pool's own rule: it matches a slot's seed against a
known emitter on both coordinates, which a test can do because it has the
truth and the pool cannot:

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:pool"
```

In C the same run is `native/examples/async_dsss_pool_demo.c`, which also
shows what the binding hides: the symbol buffer sized after the first
push, and the borrowed log the caller closes.

## Performance

The certified envelope — every limit a caller may rely on, and the three
findings still open — is the pool's validation report,
`src/doppler/dsss/tests/validation/async_dsss_pool/results.md`
([on GitHub](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/async_dsss_pool/results.md)).
It is generated by the `validate.py` beside it from the C implementation
through its own binding; nothing in it is modelled. **15 of 15 limits
hold**, and of its ten findings F5, F9 and F10 are open. The numbers here
are quoted from it and from the ten-emitter soak
(`native/validation/async_dsss_pool_soak.c`), not re-derived.

**The lifecycle, at the operating point.** The certified run is two
emitters for 16 s at 45 dB-Hz; the soak is ten emitters arriving and
leaving, 120 s at 45 dB-Hz and 600 s at the 40 dB-Hz floor. All three on
`D = 154`, ±50 kHz, sixteen hits per dwell, twelve slots, the carrier
told, a 2 s release interval, twenty threads.

| quantity                                 | certified run, 45 dB-Hz | soak, 45 dB-Hz | soak, 40 dB-Hz |
| ---------------------------------------- | ----------------------- | -------------- | -------------- |
| arrival → tracking, mean / max           | 0.11 / 0.12 s           | 0.09 / 0.21 s  | 0.41 / 1.76 s  |
| tracking with code lock, of held blocks  | 0.9852                  | 0.9982         | 0.9954         |
| with symbol lock, of held blocks         | 0.9790                  | 0.9969         | 0.9808         |
| departure → release, max                 | 2.02 s                  | 2.15 s         | 2.04 s         |
| emitters missed / released while on air  | 0 / 0                   | 0 / 0          | 0 / 0          |
| blocks with two receivers on one emitter | 0                       | 0              | 0              |

A slot holds an arriving emitter well before it tracks it: 0.02 / 0.03 /
0.04 s min / mean / max in the certified run, the rest of the time to
tracking being the pull-in. The certified floor on the two lock fractions
is 98% of held blocks with code lock and 95% with symbol lock.

Design to the release rule's **two** intervals, not one: the code flag
returns on noise at about 0.004 per second, and one return inside the
interval restarts the clock once — of the order of one departure in a
hundred. The seed a receiver pulls in from is coarse by design: worst
observed 781 Hz and 0.17 chip in the certified run, because at 45 dB-Hz
the first hit on an emitter is usually a smeared copy from its own data
blocks rather than from its code-only window.

**Cost.** A number here is a multiple of real time: above 1 means the
stage takes longer than the signal it consumed.

| stage                                                          | × real time |
| -------------------------------------------------------------- | ----------- |
| inside `push()`, two emitters, twenty threads                  | 2.95        |
| the chain: the shipped `DDC` from 13 MSa/s, then that `push()` | 3.16        |
| the same chain at a 30 MSa/s front end                         | 7.29        |
| the chain, the whole ten-emitter population at 13 MSa/s        | 4.12        |
| the same, at 30 MSa/s                                          | 9.51        |
| the block searcher alone, `D = 154` over ±50 kHz, four threads | 1.6         |
| the block searcher alone, `D = 154` over ±5 kHz, four threads  | 0.35        |

The two ten-emitter figures were taken on the receiver flavour this pool
replaced, measured side by side against it at 2.954 of a core per second
of signal against 2.941, so they carry over. The widest operating point
does **not** keep up on one twenty-thread box, and the stage that owns
the excess is the searcher's depth, not the receivers — one receiver costs about 44 ns per sample, twelve of them a
fraction of the searcher's block. The two levers are the Doppler span —
pre-compensating to ±5 kHz is a 4.6× saving in the searcher — and more
threads: the block searcher's fan is 92% of its work at `D = 154`, so
four threads buy 3.2× and eight 4.2×, and every thread count gives
bit-identical records.

**Memory.** The searcher's block and surface at `D = 154` are about half
a gigabyte (501.6 MiB of heap after warm-up, 254 MiB resident). Nothing
allocates per push once a block size has been seen, and each receiver
builds its track chain on its first seed — a first-use step, not growth:
once every slot has been used, the heap moves by +0.1 KiB over 120 s and
+0.3 KiB over 600 s.

**Sizing.** Slots are the population, plus the receivers still inside a
confirm interval on emitters that have just left, plus the false alarms
(one slot each for one interval): twelve for ten emitters at a departure
a minute and a two-second interval. The hit list is the population plus
the false hits the gate admits per dwell; sixteen for ten.

## What it does not do

- It does not cancel a strong emitter to find a weak one under its
    sidelobes: the list branch ships for a power spread inside the floor
    (about 13 dB in the operating case, 21 inside the code-only window);
    a wider spread needs the cancellation branch, designed and not built.
- Two emitters within a chip of each other on the code axis are one
    while the first is live: the zone is the code axis alone.
- A neighbour within about a kilohertz crossing a departed receiver's
    code phase at a chip or two a second looks like that emitter's return
    to the receiver; the pool, which knows the neighbour has a slot, is
    where it can be told apart, and it is not yet
    ([#1275](https://github.com/doppler-dsp/doppler/issues/1275)).
