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

## The objects inside, and what each decides

| object                              | in the pool                                                                                                                      | decides                                                                                                         |
| ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `Acquisition` (continuous)          | one, over `doppler_uncertainty`, in coherent blocks of `D` epochs inside the code-only window, its tiles fanned across `threads` | every peak above the gate, per dwell, up to `max_peaks`                                                         |
| `CellAsyncDsssReceiver` × `n_slots` | created idle; a seed starts the pull-in → track on the searcher's timing; every one is fed every block                           | its own lock flags, and *lost* — both flags down for `lost_confirm_s`                                           |
| the assigned table                  | one row per slot: the seed, and the receiver's live Doppler and chip phase while its loops are locked                            | a peak within one chip of a live row's code phase is that emitter's own and seeds nothing                       |
| `EventLog` (attached)               | borrowed by `set_event_log()`                                                                                                    | nothing — it records `seeded`, `tracking`, `degrade`, `lost`, `released`, `dropped` with the slot's coordinates |

The pool itself decides two things: which free slot a survivor seeds
(or `dropped` when there is none), and the **on-time release** — a slot
held past `max_emitter_on_time_secs` is released and its emitter is a new
detection at its next window.

Every slot is a `CellAsyncDsssReceiver`: no refine stage, its code loop
held from the seed and corrected once every `D` code periods on the
searcher's own timing, at `gain` (default 1/8) after `pullin_intervals`
(default 4) at gain 1; the seed's carrier residual is estimated on the
receiver's own despread stream and folded once. The constructor refuses a
searcher a cell receiver cannot take — `code_only_epochs` must give `D ≥ 13`
at the 5 Mcps / Gold-1023 geometry, so that a seed half a row off is
inside the carrier loop's pull-in (see the design page, §8.2). The pool on
hand-off receivers, with a refine chain per seed, was retired on
2026-09-10 once this one matched it on the soak (the record's §12.27–12.28).

## One slot's lifecycle

```text
idle ──seed()──▶ refining ──hand-over──▶ tracking ──both flags down
 ▲                                          │       for lost_confirm_s
 │                                          ▼
 └───────── released (reset to idle) ◀──── lost
```

- **`seeded`** — a listed peak at no live row's code phase, into a free
    slot. The seed carries the searcher's Doppler (to one row), chip phase
    (to half a chip) and C/N0 estimate.
- **`tracking`** — the pull-in has folded its estimate and a lock flag
    is up; from
    here `status()` reports the loops' own Doppler and chip phase, and
    `symbols()` returns what the receiver decided on the last push.
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
`pfa = 1e-3` a noise peak seeds a free slot every few hundred milliseconds,
pulls in to nothing, reports tracking with both flags down and is released
one interval later. That is why a slot is an emitter's **by both
coordinates** — its seed within the searcher's row of the emitter's
Doppler *and* within a chip of its code phase — never by a count, and why
`n_slots` carries headroom over the population.

## Configuring it

Every number is a constructor parameter; the defaults are the design's
operating point, and the searcher's and the receivers' own parameters pass
through untouched.

| parameter                              | meaning                                                                                                                                                                                                                                            | default         |
| -------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| `code`                                 | the spreading code, 0/1 chips; every emitter is on it                                                                                                                                                                                              | required        |
| `chip_rate`, `symbol_rate`, `spc`, `m` | the waveform: chips per second, data symbols per second, samples per chip, the constellation order                                                                                                                                                 | 1e6, 1000, 2, 2 |
| `cn0_dbhz`, `pfa`, `pd`                | the sensitivity the searcher and the receivers are sized for, and the searcher's per-dwell false-alarm and detection targets                                                                                                                       | 55, 1e-3, 0.9   |
| `doppler_uncertainty`                  | the searcher's span, Hz one-sided; it tiles the span in windows one epoch rate wide                                                                                                                                                                | 100             |
| `code_only_epochs`                     | the whole code epochs the waveform's code-only window holds at any chip phase; sizes the coherent depth `D`, which must be at least 13 here (a windowed waveform)                                                                                  | 813             |
| `doppler_rate`                         | the Doppler rate the depth is bounded against, Hz/s (0 = no bound)                                                                                                                                                                                 | 0               |
| `max_peaks`                            | the peak list's capacity per dwell: the population plus the false peaks the gate admits                                                                                                                                                            | 16              |
| `n_slots`                              | receivers held; the population plus release headroom                                                                                                                                                                                               | 12              |
| `threads`                              | the workers the searcher's tiles and the receivers run across (1 = serial; the result is bit-identical at any count)                                                                                                                               | 1               |
| `carrier_freq_hz`                      | the RF carrier the Doppler is physically coupled to; told, the searcher walks its blocks by each tile's code rate, the hand-off advances the seed by half a dwell's drift, and the receivers aid their code loops from the carrier (0 = uncoupled) | 0               |
| `lost_confirm_s`                       | the release rule's interval: both flags down this long is *lost*; longer than the longest fade the link must ride                                                                                                                                  | 2.0             |
| `max_emitter_on_time_secs`             | the on-time cap: a slot held this long is released and its emitter re-acquired                                                                                                                                                                     | 900             |
| `segments`, `sps`, `differential`      | the receivers' despreader partials per epoch, the demodulator's samples per symbol, differential decoding                                                                                                                                          | 4, 8, 0         |
| `gain`, `pullin_intervals`             | the receivers' correction: chips per chip of the interval-mean discriminator after the pull-in, and the intervals at gain 1 before it                                                                                                              | 0.125, 4        |

One knob is a method, because it is decided after the population is
known: `set_event_log(log)` attaches the run's log (`None` detaches).

A physically-coupled carrier (`carrier_freq_hz > 0`) is the setting that
matters most for a moving emitter: without it a 50 kHz Doppler smears the
searcher's coherent block by three chips and the receivers' code loops
have no aid.

## Reading it back

- **`status(slot)`** returns one record by value: whether the slot is
    assigned, the receiver's state, the seed it was assigned from (stream
    position, chip phase, Doppler, C/N0), the emitter's **live** Doppler,
    chip phase and code rate, the C/N0 estimate, both lock flags with the
    symbol-lock metric, and three clocks in input samples — since the
    state was entered, since both flags dropped, since the row was
    assigned. Read it after every push for the picture; it is not
    `get_state()`, which is the bytes for resuming the pool elsewhere.
- **`symbols(slot)`** is the symbol stream the slot's receiver decided on
    the last push, borrowed from the pool's own buffer.
- **`n_assigned`, `dropped`, `events`, `samples_consumed`** are the pool's
    counters; `doppler_res_hz` is the searcher's row, the resolution a
    seed's Doppler is reported at; `coherent_bins` is the depth it chose.
- **The event log** is a flat, tail-able JSON-lines file while the run is
    live and a SigMF sidecar when finalized; every line carries
    `core:sample_start`, `core:label` and the slot's `doppler:*` fields.
    Replay and live runs produce identical records, because nothing below
    the pool sees a time.

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
status read after each push; the `owner()` helper is how a test decides
whose a slot is — by both coordinates:

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:pool"
```

In C the same run is `native/examples/async_dsss_pool_demo.c`, which also
shows what the binding hides: the symbol buffer sized after the first
push, and the borrowed log the caller closes.

## Sizing and cost

- **Slots.** The population plus the receivers still inside a confirm
    interval on emitters that have just left, plus the false alarms (one
    slot each for one interval): twelve for ten emitters at a departure a
    minute and a two-second interval.
- **The peak list.** The population plus the false peaks the gate admits
    per dwell; sixteen for ten.
- **Threads.** The searcher's tiles and the receivers both fan across
    `threads`; the result is bit-identical at any count. At the design's
    operating point (5 Mcps, ±50 kHz, `D = 154`, ten emitters) the whole
    population runs at about 4× real time on twenty threads, most of it
    the block searcher's depth — size the machine from
    [the measurement record](../design/async-dsss-receiver-measurements.md)
    §12.8 and §12.17, not from the single-emitter numbers.
- **Memory.** The searcher's block and surface at `D = 154` are about
    half a gigabyte; nothing allocates per push once a block size has been
    seen, and the heap is flat over a ten-minute run to a page.
- **Checkpointing.** The pool is serializable, every child included: a
    mid-stream `get_state()` / `set_state()` resumes bit for bit
    ([Checkpoint & Resume](state-serialization.md)).

## What it does not do

- It does not cancel a strong emitter to find a weak one under its
    sidelobes: the list branch ships for a power spread inside the floor
    (about 13 dB in the operating case, 21 inside the code-only window);
    a wider spread needs the cancellation branch, designed and not built.
- Two emitters within a chip of each other on the code axis are one
    while the first is live: the exclusion zone is the code axis alone.
- A neighbour within about a kilohertz crossing a departed receiver's
    code phase at a chip or two a second looks like that emitter's return
    to the receiver; the pool's zone is where it can be told apart, and it
    is not yet ([#1275](https://github.com/doppler-dsp/doppler/issues/1275)).

The certified envelope — what a caller may rely on, and what was found
and left open — is the pool's
[validation report](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/async_dsss_pool/results.md).
