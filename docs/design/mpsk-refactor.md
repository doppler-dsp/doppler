# M-PSK receiver: collapsing two objects into one

A design record for a refactor that is planned but not yet built. It exists
because the argument for it is a **measurement**, and the measurement is worth
writing down whether or not the refactor happens.

The receiver itself is [`mpsk.md`](mpsk.md); this page does not restate it.

______________________________________________________________________

## 1. The thesis

**`MpskReceiver` and `MpskReceiverR` are one object wearing two types.**

Everything that makes a receiver a receiver is already shared, and shared *by
value*: `mpsk_rx_loops_t` — the carrier loop, the symbol-timing loop, the
acquisition↔tracking handover, the demapper, the telemetry attachment and all
five §8.1 derivations — is one struct in one header
(`native/inc/mpsk_receiver/mpsk_rx_loops.h`, 784 lines), embedded in both.

They differ in exactly two places:

- the **front end** — `ddc_state_t *` against `ddcr_state_t *`;
- one **rate convention** — the real twin's LO runs at half the input rate,
    because its R2C halfband decimates 2:1. `ddcr`'s tuning law is
    `norm_freq = -(2·f_c + 0.5)`, so its `get_norm_freq` carries a `0.5`.

They are separate *types* rather than a view and its parent because of this
project's own axis — a difference in **constructor** is a flavor, a difference
in **method signature** is a separate type — and `steps()`/`bits()` take `cf32`
against `f32`. Nothing else separates them.

### Measured, not asserted

`native/src/mpsk_receiver_r/mpsk_receiver_r_core.c` is **372 lines and 30
public functions**, of which **16 are pure delegations** to `->l`. Of the
remainder, only `create`, `destroy`, `reset`, the two `norm_freq` accessors,
`set_norm_freq`, `get_clipped` and the state triplet genuinely differ — and
each differs by a front-end call or a factor of two, never by algorithm.

______________________________________________________________________

## 2. What the split actually costs

Not the duplication. **The shared header has no test home.**

| surface                  | header  | its own test                        |
| ------------------------ | ------- | ----------------------------------- |
| `mpsk_receiver_core.h`   | 753     | `test_mpsk_receiver_core.c` (732)   |
| `mpsk_receiver_r_core.h` | 481     | `test_mpsk_receiver_r_core.c` (630) |
| **`mpsk_rx_loops.h`**    | **784** | **none**                            |

The loops' claims are therefore asserted only where one of the two receivers'
tests happens to reach them — and the two do not overlap:

| shared claim                         | `test_mpsk_receiver` | `test_mpsk_receiver_r` |
| ------------------------------------ | -------------------- | ---------------------- |
| `set_telemetry`                      | 7 hits               | **0**                  |
| level invariance                     | 6                    | 1                      |
| the AGC is slower than every loop    | yes                  | **0**                  |
| "the LO runs at half the input rate" | **0**                | **0**                  |

So the loops' telemetry, AGC and level-invariance claims are pinned once, on
the complex side only, and the real twin's rate convention is pinned by
neither. That last row is where a `freq_scale` bug lived (gh-765): a loop
running several times narrower than configured, which passes every step test
because **a type-2 loop nulls a frequency step regardless of gain**.

**Nothing anywhere asserts that the loops behave identically regardless of
front end** — which is the one claim that makes a shared header worth having.
One object gives those 784 lines a single home.

______________________________________________________________________

## 3. The design

One core, tagged front end:

```text
typedef struct
{
  union { ddc_state_t *c; ddcr_state_t *r; } fe;
  int             real;        /* 0 = complex front end, 1 = real */
  mpsk_rx_loops_t l;
  double          centre_freq;
} mpsk_receiver_state_t;
```

Two C step entry points, because C has no overloading and the input type
genuinely differs — but both force-inlined onto the one shared body that
already exists, `mpsk_rx_take_output()`. `real` is passed as a **literal**, so
it specialises branch-free: the identical trick `ted` already uses in
`mpsk_receiver_step_ted()` ("pass a literal for a specialised (branch-free)
instantiation"). The tag costs nothing in the hot loop.

Three faces, as jm views over that one core:

| face                     | constructor                                     | input  |
| ------------------------ | ----------------------------------------------- | ------ |
| `MpskReceiver`           | complex front end                               | `cf32` |
| `MpskReceiverR`          | real front end                                  | `f32`  |
| `ContinuousMpskReceiver` | complex, pins `acq_to_track=0` / `strobe` / AGC | `cf32` |

The rate convention stays a property of the **face**, not of the loops: the
real face's `norm_freq` accessors keep their `0.5` / `2.0` factors, exactly as
today.

______________________________________________________________________

## 4. The blocker, and what was measured about it

A jm view shares its parent's methods **verbatim**. It can ADD a method with
its own `arg_type`, or override a parent method's **doc** — and nothing else.
So a per-face `steps()` dtype is precisely what it cannot express.

Scaffolded against jm **0.61.0 and 0.61.1**, three routes:

| route                                          | result                                                                                                                        |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `exclude_methods = ["steps"]` + a view `steps` | **explicit error** — "both excluded and added on view … contradictory"                                                        |
| a view `steps` with no exclude                 | **accepted, `arg_type` silently ignored** — the `.pyi` *and* the view's own C fragment both keep the parent's `NPY_COMPLEX64` |
| a view method with a **new** name (`steps_r`)  | **works end to end** — own `arg_type`, own binding, own `_max_out`                                                            |

Filed upstream as **just-makeit#1011** (the silent ignore — jm already detects
this exact collision on the exclude path, so the check simply is not applied
here) and **just-makeit#1012** (the feature: let a view override a *signature*,
not just a doc). They are linked, and #1011 closes either way: implement the
override, or error as the exclude path already does.

**The fallback is real.** Route 3 means the collapse is possible today, with
`steps_r()` / `bits_r()` on the real face. It costs an asymmetric public
surface — two classes with the same semantics, one spelling its block API
differently — which is most of the way back to being a second type. That is a
trade, not a blocker.

______________________________________________________________________

## 5. Why now — the claim inventory

`dev/validation.md` opens the certification process with a claim inventory, and
running it over both receivers produced the finding that motivates this page.

**Well pinned**: lifecycle and argument validation, reset reproducibility, all
five derivations with their read-backs and supplied-value-wins, the continuous
flavor's pins *with a handover-enabled control on the same record*, the
handover's flip / drop-back / re-declare, the state round-trip and its envelope
reject, 14 telemetry probes plus the AGC's off-grid pair plus detach plus
table-full, the AGC being slower than every loop it feeds, a blob taken
mid-convergence resuming, level invariance, and SER→0 at every M through both
pulses. `MpskReceiverR` adds its own `sps > 2·m_out` and occupied-band
constraints, both written as *"enforced, not merely documented"*.

**Absent — verified at zero mentions in both tests, and all of them numbers:**

| claim                                                                    |
| ------------------------------------------------------------------------ |
| "`m_out = 8` is not optional at M = 8" — 3.0 dB squaring loss            |
| "`m_out = 8` is where an I&D filter reaches the bound" — 0.41 vs 3.11 dB |
| "Never pair `m_out = 2` with `IANDD`" — lock −0.34 against +0.95         |
| the handover carries the frequency estimate across, both ways            |
| `bn_carrier` is normalised to the symbol rate                            |
| `σ_H0 = 0.1132` for every M; `0.5` = 4.42σ, Pfa 5e-6                     |
| `mpsk_rx_tlm_flush()` is the caller's on the composition path            |

Every **derived value** is pinned; **no derivation is**. The object's behaviour
is well tested and its measured claims are not — which is exactly how the
`nda_tap` cost column read *"none"* for as long as it did. Nothing in the tree
ever read it back.

______________________________________________________________________

## 6. Sequence

1. The `strobe` pin, `native/validation/rx_dynamics.c` and the corrected
    `nda_tap` cost — landed separately, already green.
1. The C tests the inventory found missing, each proven by sabotage, in
    priority order: the `m_out = 2` + `IANDD` degeneracy; `m_out = 8` at
    M = 8 against the bound; **the handover carrying the frequency estimate
    across a drop-back** — the one whose failure is silent and expensive; and
    `bn_carrier`'s symbol-rate normalisation. The `σ_H0` / Pfa row belongs to
    `carrier_nda`'s own test, not here.
1. This collapse, once just-makeit#1012 lands — or on `steps_r()` if it does
    not.
1. The validation report, written against the collapsed object, so one object
    has one report.

______________________________________________________________________

## 7. Decisions this page records

- **`strobe` is the continuous flavor's tap.** Measured on that flavor's own
    waveform — NRZ, modulation off then dense, under a coupled Doppler ramp:
    lock 0.935 quiet, **0.860** at the data onset, 0.920 at the end, against
    `mf_out`'s 0.478 and `mf_in`'s 0.417 at the onset. An unmodulated NRZ
    carrier is **sampling-phase invariant**, so the strobe tap's timing
    dependency costs nothing exactly where timing is impossible.
- **`mf_in`'s cost is a stated price, and the arm filter is declined.** Its
    node carries `10·log10(bank_sps)` dB of excess noise bandwidth — measured
    6.01 dB at `bank_sps = 4`, *identical* at 6.79, 12 and 20 dB Es/N0, which
    is a pure bandwidth ratio rather than an SNR-dependent effect. Recovering
    it costs serialized state on every object carrying the tap, and `strobe`
    reads the node already matched to the signal for free.
- **The TED is not cosmetic.** On a rectangular pulse, Gardner deepens the
    data-onset lock dip from 0.075 to 0.306 — four times, from the detector
    choice alone. Use DTTL with I&D.
