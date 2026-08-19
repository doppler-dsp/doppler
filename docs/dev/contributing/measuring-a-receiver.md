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
only part that forks per design. Eleven entries, all of which a receiver worth
measuring already exposes:

<!-- docs-snippet: skip=the harness family lives in native/tests/, which is deliberately NOT on the doc-snippet compiler's include path (it takes native/inc only) — this is a real declaration, compiled where it lives -->

```c
typedef struct
{
  const char    *name;
  dp_rx_domain_t domain;              /* complex baseband, or real IF     */

  void *(*create) (const struct dp_rx_point *);
  void  (*destroy) (void *);
  int   (*step) (void *, float complex x, float complex *y);

  double (*norm_freq) (const void *);  /* tracked carrier, cycles/sample  */
  double (*last_error) (const void *); /* discriminator output, radians   */
  double (*lock) (const void *);       /* normalised lock statistic       */
  int    (*locked) (const void *);     /* lock declared                   */
  long   (*lock_time) (const void *);  /* symbols to first lock, -1 none  */
  int    (*clipped) (const void *);    /* front end clipped               */
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
int8_t          truth[20000];
float _Complex *x = dp_tx_make (&cfg, truth, &n);

/* ... run your object over x, collecting `out` and per-symbol lock flags ... */

size_t settle = dp_ber_settle (bn_timing, bn_carrier, NULL, lock_c, NULL,
                               nout, &ok);
dp_ber_report_t rep = dp_ber_measure (&acc, out, nout, sym, nsym,
                                      esn0_db, settle, ok, NULL);
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

| trap                                                                                                                                                                                                                                                                                                                                                                                                                | owner           |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| A window pinned to a **fraction of the record** measures the acquisition transient. Derive it: `2*(5/bn_t + 5/bn_c)`, because the loops cascade so their budgets add, then double for joint tracking.                                                                                                                                                                                                               | `dp_sym_test.h` |
| An offset the loop is **seeded on** measures nothing — it never leaves its initial state — and one **past its acquisition bound** measures the dice. State it in cycles per SYMBOL against `bn_carrier / m` (the `m` because the discriminator is an M-th power), seed at or under the bound, and convert to cycles per sample once, at the constructor. `freq_offset_inside_bw` / `dp_test_freq_offset_inside_bw`. | `dp_sym_test.h` |
| An EVM threshold written against **0 dB** is meaningless at high M. A fully scattered constellation reads −1.4 / −7.0 / **−12.9 dB** at M = 2/4/8, so `< -12.0` passes on a receiver with no carrier recovery at all. State thresholds against `dp_test_evm_scatter_floor_db(m)`.                                                                                                                                   | `dp_sym_test.h` |
| A **min over (lag, rotation)** is an optimisation over the answer, not a measurement. It false-passes on garbage and false-floors on a healthy receiver. Detect the alignment against a known marker with a Pfa gate.                                                                                                                                                                                               | `dp_ber_test.h` |
| Stopping on a **symbol count** makes precision depend on the rate you are measuring. Stop on an error count.                                                                                                                                                                                                                                                                                                        | `dp_ber_test.h` |
| Amplitude is a **symbol** amplitude, never a peak normalisation. An RRC stream peaks ~1.582× above its symbols, so "normalise to 0.25 of peak" delivers 0.158 against a unit-amplitude contract.                                                                                                                                                                                                                    | `dp_tx_test.h`  |
| A CIC clips **silently** past ±1.0 input, costing ~25 dB of EVM that no lock metric reveals. Assert `clipped == 0`.                                                                                                                                                                                                                                                                                                 | `dp_mf_test.h`  |
| A carrier offset and a sample-clock error are **the same physical parameter**. Ramping one without the other measures a signal no receiver will ever see — use `doppler_channel`.                                                                                                                                                                                                                                   | `dp_rx_test.h`  |

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
`FrameMeter`, `wfm_frame_t`, `doppler_channel`, and both estimators, all with
Python bindings. The named operating points deliberately do **not** ship —
they are our conventions, not an API, and a caller measuring their own
receiver wants the machinery rather than our choice of `sps = 8`.

## Related pages

- [Receiver Test Harness](../../design/rx-test.md) — the design and the ten goals
- [Object Validation](validation.md) — the certification process
- [MPSK Receiver](../../design/mpsk.md) — the receiver this path was built against
