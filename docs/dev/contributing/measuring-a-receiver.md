---
description: "The path from a receiver to a number you can defend: the adapter, the four metrics together, how to read a refusal, and what to gate on"
---

# Measuring a receiver

You have a receiver — ours or your own — and you need a number you can defend.
This page is the path from one to the other.

It is the **how**. [Receiver Test Harness](../../design/rx-test.md) is the *why*
(the ten goals, and the failures that motivated them);
[`native/tests/README.md`](https://github.com/doppler-dsp/doppler/blob/main/native/tests/README.md)
is the *reference* for the harness family; [Object Validation](validation.md)
is the certification process this feeds. None of them restate each other.

______________________________________________________________________

## The one rule

**You compose. You do not build.**

Every piece of a receiver measurement already exists and is tested: the
generator, the impairment, the estimators, the confidence interval, the
alignment detector. A harness that grows its own is the failure this whole
layer exists to correct — and it is not a hypothetical, it is where the
project's confidently-wrong numbers came from. Three copies of one synthesis
loop differed only in their amplitude convention, and because a TED's slope
goes as `A²` two of them measured loop bandwidths ~16× apart while both read
as "the RRC BPSK test".

So: no private pulse, no private level convention, no private estimator, and
no private random number generator. `make lint` enforces the last one and
`scripts/check_stimulus_sources.py` enforces most of the rest.

______________________________________________________________________

## The fast path: give the battery an adapter

If what you have is a **receiver object** — something that takes samples and
emits symbols — you do not write a harness. You write an adapter and the
instrument does the rest.

`native/tests/dp_rx_test.h` is that instrument, and `dp_rx_iface_t` is the
only part that forks per design. Twelve members, all of which a receiver worth
measuring already exposes:

<!-- docs-snippet: skip=the harness family lives in native/tests/, which is deliberately NOT on the doc-snippet compiler's include path (it takes native/inc only) — this is a real declaration, compiled where it lives -->

```c
typedef struct
{
  const char    *name;   /**< printed with every number it produces      */
  dp_rx_domain_t domain; /**< drives the step call AND the Es/N0 offset  */

  /** Construct at an operating point, or NULL if the point is unbuildable. */
  void *(*create) (const struct dp_rx_point *);
  void (*destroy) (void *);

  /** One input sample in, 0 or more terminal outputs out. A real receiver
   *  takes `crealf(x)`; the instrument does that conversion, not the caller.
   */
  int (*step) (void *, float _Complex x, float _Complex *y);

  double (*norm_freq) (const void *);  /**< tracked carrier, cycles/sample */
  double (*last_error) (const void *); /**< discriminator output, radians  */
  double (*lock) (const void *);       /**< normalised lock statistic      */
  int (*locked) (const void *);        /**< lock declared                  */
  long (*lock_time) (const void *);    /**< symbols to first lock, -1 none */
  int (*clipped) (const void *);       /**< front end clipped: reading dead */
  double (*zeta) (const void *);       /**< carrier damping, as BUILT       */
} dp_rx_iface_t;
```

`native/validation/rx_battery.c` is the worked example: an adapter for
`MpskReceiver`, and a loop. Copy it.

**A design that cannot fill one of these in is telling you something real
about its observability**, not about the harness. That is deliberate — a
receiver whose lock state you cannot read is a receiver whose settled window
you cannot establish, and every metric below depends on that window.

### `domain` is not cosmetic

A real front end takes `Re{}`, which halves the signal energy **and** the
noise variance — but the real path's convention counts the real noise against
the halved `Es`, which is 3 dB less noise. The stimulus must therefore be
generated **3 dB hot** for a real receiver to see the Es/N0 it was asked for.
The instrument does that from this one field. Getting it wrong measures a
receiver 3 dB better than it is, and nothing else in the record says so.

______________________________________________________________________

## What comes back

One record, per operating point, with everything needed to defend every number
in it:

| group         | what                                                                  | why it is there                                                         |
| ------------- | --------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| **BER / SER** | rate + exact confidence interval                                      | truth-referenced; the only metric that sees a false lock **with** truth |
| **EVM**       | self-referenced against hard decisions                                | truth-free; needs no alignment                                          |
| **M2M4**      | blind Es/N0 from 2nd/4th moments                                      | truth-free and rotation-blind                                           |
| **FER**       | frame error rate + sync-miss rate                                     | truth-free **and** sees a false lock                                    |
| loop          | acquisition fraction, lock time in `1/Bn`, discriminator peak and RMS | explains the four above                                                 |

**They are reported together, always, and that is the point.** They fail
differently, and the disagreement is the diagnostic. Under a stable false lock
at `Δf = k·Rs/M` the constellation is stationary, so EVM looks clean, M2M4
looks clean, and the receiver declares lock — measured at 8PSK, the EVM
penalty is 0.21 dB, which is nothing. Only the truth-referenced alignment
refuses, and only a CRC-checked frame catches it without truth.

Reporting one of them alone is what makes a false lock invisible.

______________________________________________________________________

## Refusals are results

`dp_rx_result_t.refused` is a **field**, not an exception, and a refusal names
itself:

```text
MpskReceiver acq   REFUSED — frame carries no payload, nothing to demodulate
MpskReceiver x     REFUSED — no burst settled, the loops never locked
MpskReceiver y     REFUSED — no burst aligned, the marker never detected
MpskReceiver z     REFUSED — front end clipped, the reading is worthless
```

A refusal **does not fail the gate**. It is the harness declining to report a
number it cannot defend, which is the design working. What fails is a record
that claims to be a measurement and is not.

Read them carefully, because they are diagnostic and they are not
interchangeable. "The loops never locked" and "the marker never detected" call
for different repairs, which is exactly why they are separate counters rather
than one.

And a refusal can be about **your indicator rather than your receiver**. A
receiver whose lock statistic is miscalibrated refuses at every point while
demodulating perfectly — that is
[#791](https://github.com/doppler-dsp/doppler/issues/791), found this way.
Check EVM before concluding the receiver is broken: if it is on the bound, the
refusal is about observability.

______________________________________________________________________

## What to gate on

**Assert on an interval's limit, never on a point estimate.** Every rate here
comes with an exact confidence interval, and the reason is not fussiness: a
run stopped on an error count is inverse-binomial sampling, so the relative
standard error is `1/√r` — a function of the error count **alone**. 20 000
symbols at SER 1e-3 yields ~20 errors and ~22% relative error, which reads as
seed-to-seed variation in the receiver rather than as sampling noise.

So gate the implementation loss on the interval's **lower** limit:

```text
loss = esn0_db − ber_esn0_db_for_ser(m, ser.lo)     ≤ your bound
```

**Do not gate a loop number as a value.** A pull-in ceiling moves with the
record length you allowed, so pinning one pins your observation window rather
than the receiver.

**Gate lock on time, and time on the offset.** A pull-in time is a function of
the offset in loop bandwidths: inside the loop's lock-in range it is
predictable, and past it the pull-in law says how long. Gate that the link
locks inside the time its offset predicts and inside the stated allocation
(415.1-B-1's is 20 s, `DP_S415_LOCK_S` in `dp_s415_test.h`). Then score from
the observed lock (the first trap below).

**Slips before lock are printed, never gated.** Cycle slips during pull-in
cannot be reliably controlled, and they help the loop pull in. **A half-cycle
slip after lock, inside the scored window, is a failure**: a phase reversal in
a data stream that nothing detects. The marker alignment counts it
(`slips`, `dp_ber_test.h`), and `record_415.py acq` reads such a draw as
`slipped`.

**Do check that your gate can fail.** This is the one that is repeatedly
learned the hard way: sabotage the thing the gate is for and watch it go red.
A gate never seen to fail is not evidence — the receiver battery's own FER
anchor was measured to have 1.64× slack and pass a deliberate corruption
([#796](https://github.com/doppler-dsp/doppler/issues/796)), and its per-frame
sync gates stayed green with the detector hard-wired to accept.

______________________________________________________________________

## Doing it by hand

If what you are measuring is not a receiver object — a despreader, a loop, a
filter — compose the same pieces directly. Three calls:

<!-- docs-snippet: skip=the harness family lives in native/tests/, which is deliberately NOT on the doc-snippet compiler's include path; these headers are compiled and tested where they live -->

```c
#include "dp_tx_test.h"   /* stimulus  */
#include "dp_ber_test.h"  /* the gates */

dp_tx_cfg_t cfg = dp_tx_defaults ();   /* every field you do not set is the
                                          shared convention, provably       */
cfg.nsym = 20000;
cfg.sps  = 8.0;

size_t          n = 0;
int8_t          tx[20000];     /* the +-1 BPSK sequence, as transmitted */
uint8_t         truth[20000];  /* the same, as symbol indices (0..m-1)  */
float _Complex *x = dp_tx_make (&cfg, tx, &n);
for (size_t i = 0; i < cfg.nsym; i++)
  truth[i] = tx[i] < 0;        /* index k is phase 2*pi*k/m: +1 -> 0    */

/* ... run your object over x, collecting `out` and per-symbol lock flags ... */

/* The window opens at the observed lock: the latest of the flags your
   object publishes, each sustained, inside your lock allocation. */
const unsigned char *flags[2] = { lock_t, lock_c };
const int64_t start = dp_ber_lock_all (flags, 2, nout, 200, 0.9, limit, NULL);
const int     ok    = start >= 0;   /* -1: not locked inside the allocation */

dp_ber_t acc;
dp_ber_init (&acc, 2, DP_BER_TARGET_ERRORS);
dp_ber_report_t rep = dp_ber_measure (&acc, out, nout, truth, cfg.nsym,
                                      esn0_db, ok ? (size_t)start : 0, ok,
                                      NULL);
dp_ber_print ("my object @ 6.8 dB", &rep);
```

`dp_ber_measure()` is the sanctioned one-call path. It wires sync → window →
score → report in the one order that is correct, including the window rule
that has to respect both the settled point and the marker's shape. Copies of
that sequence have been written by hand and have each been subtly wrong; there
is no reason to write a fourth.

______________________________________________________________________

## The traps, and who owns each

Every one of these was paid for once already. The header named is where the
reasoning lives — go there before writing a threshold.

| trap                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          | owner                                                                       |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| A window pinned to a **fraction of the record**, or to an analytic settle budget, measures the acquisition transient or a loop that has not locked. Open it at the **observed lock** of every flag the receiver publishes, each sustained, inside a stated lock allocation, and align once at that start (`dp_ber_lock_all`). A link whose flags do not all sustain inside the allocation is "not locked" and is never scored. The analytic budget `2*(5/bn_t + 5/bn_c)` (`dp_ber_settle`) is a check for a start inside the loops' lock-in range only.                                                                                                                                                                       | `dp_ber_test.h`                                                             |
| An offset the loop is **seeded on** measures nothing — it never leaves its initial state — and one **past its prompt-lock window** measures the pull-in instead. State it in cycles per SYMBOL against `bn_carrier / (4 m)`, seed at or under it, and convert to cycles per sample once, at the constructor. The `m` is there because the discriminator is an M-th power. The 4 is there because `bn` is the closed loop's noise bandwidth: the detector's slope is divided out (#3224), so the loop is no wider than `bn` says. Inside `bn / (4 m)` the loop settles within `2 / bn`; at `bn / m` it settles within `5 / bn` (`carrier_nda`'s validation report). `freq_offset_inside_bw` / `dp_test_freq_offset_inside_bw`. | `dp_sym_test.h`                                                             |
| An EVM threshold written against **0 dB** is meaningless at high M. A fully scattered constellation reads −1.4 / −7.0 / **−12.9 dB** at M = 2/4/8, so `< -12.0` passes on a receiver with no carrier recovery at all. State thresholds against `dp_test_evm_scatter_floor_db(m)`.                                                                                                                                                                                                                                                                                                                                                                                                                                             | `dp_sym_test.h`                                                             |
| A **min over (lag, rotation)** is an optimisation over the answer, not a measurement. It false-passes on garbage and false-floors on a healthy receiver. Detect the alignment against a known marker with a Pfa gate.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | `dp_ber_test.h`                                                             |
| Stopping on a **symbol count** makes precision depend on the rate you are measuring. Stop on an error count.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | `dp_ber_test.h`                                                             |
| Amplitude is a **symbol** amplitude, never a peak normalisation. An RRC stream peaks ~1.582× above its symbols, so "normalise to 0.25 of peak" delivers 0.158 against a unit-amplitude contract.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | `dp_tx_test.h`                                                              |
| A CIC clips **silently** past ±2.0 input, costing ~25 dB of EVM that no lock metric reveals. Assert `clipped == 0`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | `dp_mf_test.h`                                                              |
| A carrier offset and a sample-clock error are **the same physical parameter**. Ramping one without the other measures a signal no receiver will ever see — use `doppler_channel`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | `dp_rx_test.h`                                                              |
| A loop's **`bn` read off a jitter model** inherits the model's errors. Pin it by injecting white phase noise of known variance `s²` and reading the NCO's phase variance against `2 bn s²`: no detector-noise model enters. A detector whose slope is not divided out reads 5/3 there (1.75 to 1.79 measured), not 1.                                                                                                                                                                                                                                                                                                                                                                                                         | `test_carrier_nda_core.c` §18                                               |
| A **lock threshold** placed by a rule of thumb, or a block length from a Gaussian for the statistic, has rates other than the stated ones. Take the threshold from the statistic under noise at a stated false-alarm rate, and the block length from the exact tail under signal. For a ratio of sums of `I² − Q²` over `I² + Q²` at the 415.1-B-1 floor (jitter 4.40° and 4.50°), the delta method's Gaussian gives 31 and 40 updates, which detect at 0.982 and 0.983; the exact tail gives 34 and 43 for 0.99. `det_ratio_threshold` / `det_ratio_pd` / `det_ratio_block_len`.                                                                                                                                             | `detection_core.h`                                                          |
| A **formula recalled from a text** is a claim about some other loop or statistic. Check it against a measurement wherever it can be measured, before extrapolating from it: the block length's 31 and 40 failed exactly this check.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | the test that measures it (e.g. `test_detection_core.c`, the ratio section) |

______________________________________________________________________

## Where things live

| you want                                       | it is here                                                           |
| ---------------------------------------------- | -------------------------------------------------------------------- |
| a receiver measured end to end                 | `native/tests/dp_rx_test.h`, run by `native/validation/rx_battery.c` |
| stimulus                                       | `native/tests/dp_tx_test.h`, or `wfm_synth` for a framed waveform    |
| a named frame                                  | `native/tests/dp_frame_test.h`                                       |
| error rates, settling, alignment, the interval | `native/tests/dp_ber_test.h`                                         |
| truth-free symbol quality                      | `native/tests/dp_sym_test.h`                                         |
| randomness                                     | `native/tests/dp_rng_test.h` — and nowhere else                      |
| assertions                                     | `native/tests/dp_test.h`                                             |

Everything in that table has a self-test beside it, so the instrument is
itself measured rather than assumed. `dp_rx_test.h` is the one exception and
it is tracked as such.

**Shipped, so a caller outside this repo gets the same path:** `BerMeter`,
`FrameMeter`, the frame description (`Frame`/`FrameDesc`), `doppler_channel`, and both estimators, all with
Python bindings. The named operating points deliberately do **not** ship —
they are our conventions, not an API, and a caller measuring their own
receiver wants the machinery rather than our choice of `sps = 8`.

______________________________________________________________________

## Where a measurement lives

Three homes, told apart by the question and by what may run it:

| the question                                       | its home                                                                             | what runs it                                                                |
| -------------------------------------------------- | ------------------------------------------------------------------------------------ | --------------------------------------------------------------------------- |
| does one behavior of one object hold?              | a unit test: `native/tests/test_<obj>_core.c`, or a module's `tests/`                | `make test`, `make test-python`, every push                                 |
| does a claim the C header makes hold?              | a validation harness, `native/validation/<obj>_*.c`                                  | its `--check` subset in CTest every push; the full run by `make validate-c` |
| how does it behave across an envelope, in minutes? | a characterization subject, `src/doppler/<module>/tests/characterization/<subject>/` | `make characterize`, on purpose; never CI                                   |

A characterization's per-push cover is its **fast twin**: a test that pins
the qualitative result the sweep found, and the calibrations its columns
rest on, in seconds. `make characterization-check` refuses a subject
without one. `src/doppler/track/tests/characterization/ted_low_snr/` is a
worked example.

### A dilated scene moves every clock together

When the question has a clock error in it, the default stimulus is the
composer's Doppler channel: `doppler` in ppm with the `carrier_hz` it is
referred to, and `snr_mode="esno"`. The channel rescales the time base, so
the code, carrier and data clocks move together, the way they do on the air:

```python
from doppler.wfm import Composer, Segment

seg = Segment(
    type="bpsk",
    sps=8,
    fs=24_000.0,  # 3000 sym/s
    snr=3.0,
    snr_mode="esno",  # Es/N0, per symbol
    doppler=1.0,  # ppm of the time base
    carrier_hz=2.2e9,  # the carrier that ppm is referred to
    num_samples=24_000,
)
x = Composer([seg]).compose()
assert x.size == 24_000
```

`RateConverter` is not a way to make one. Its only rate offset,
`execute_ctrl`, steers its terminal stage, not the output rate, and a clock
moved by resampling a finished stimulus is moved apart from its carrier:
the last trap in the table above. Telling a receiver a different nominal
`sps` (`clock_offset_inside_bw`) is the same move, and it is sound only
where there is no carrier for the clock to part from, as in a baseband
timing-loop measurement.

## Related pages

- [Receiver Test Harness](../../design/rx-test.md) — the design and the ten goals
- [Object Validation](validation.md) — the certification process
- [MPSK Receiver](../../design/mpsk.md) — the receiver this path was built against
