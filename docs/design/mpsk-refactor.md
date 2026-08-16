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

## 4. The final API surface

jm#1012 shipped in jm 0.62.0 (§6), so the faces share one method **name** with
per-face dtypes. The surface below is what the collapsed object exposes.

### 4.1 The constructor: 17 params, and most of them are derivable

The two full constructors are **already byte-identical in signature** — same
17 parameters, same order, same types. That is the thesis at the API level.

But their declared *defaults* diverge in six places, and the pattern is one
finding rather than six:

| param          | complex        | real      | what the complex twin DERIVES |
| -------------- | -------------- | --------- | ----------------------------- |
| `sps`          | 8.0            | 32.0      | — (a link parameter)          |
| `m_out`        | **0** (derive) | **8**     | 8 at `sps = 8`                |
| `zeta`         | **0** (derive) | **0.707** | 0.70710678118654752           |
| `lock_thresh`  | **0** (derive) | **0.5**   | 0.4999                        |
| `num_phases`   | **0** (derive) | **1024**  | 64                            |
| `bn_agc_ratio` | **0** (derive) | **0.05**  | 0.05                          |

**The real twin never adopted §8.1's "zero means derive".** It pins five
values the complex twin computes — and three of them disagree with the
computed answer: `num_phases` is the legacy `MPSK_RX_NUM_PHASES` (1024) rather
than `MPSK_RX_NUM_PHASES_DEFAULT` (64), a 16× bank; `lock_thresh` is a round
0.5 rather than the `σ_H0·η(Pfa)` result; `zeta` is a typed-out 0.707 rather
than `1/√2`. `m_out = 8` happens to equal the derivation at `sps = 32` and
stops doing so anywhere else — at `sps = 16` the strict `sps/2` cap derives 6.

That is the argument for cutting the signature rather than unifying it as-is:
**a parameter that can be pinned, gets pinned — and drifts.**

### 4.2 A user does not know a loop bandwidth

The tier that survives scrutiny is smaller than "link plus axes", because most
of what is called an axis is stated in the receiver's units rather than the
caller's.

Nobody integrating a modem knows what `bn_carrier` should be. They do not pick
an M-th-power tap, and they have never heard of a handover. What they know is
their **link** and their **requirement**:

| they know                        | they do not know                      |
| -------------------------------- | ------------------------------------- |
| the modulation is QPSK           | `m_out`, `num_phases`                 |
| 8 samples per symbol             | `zeta`, `lock_thresh`, `bn_agc_ratio` |
| the transmitter is RRC, β = 0.35 | `nda_tap`                             |
| the carrier is within ±2 kHz     | `bn_carrier`, `bn_timing`             |
| it Dopplers at ~500 Hz/s         | `acq_to_track`                        |
| lock within ~2000 symbols        |                                       |

Both columns are the same information. The right-hand one is the left-hand one
already solved — and §5 is the solution.

### 4.3 State the link and the requirement; derive the loops

**The units are already specified** — [`mpsk.md` §7](mpsk.md#7-rates-and-units)
takes `sample_rate_hz` and `symbol_rate_hz`, makes `sps` the derived double
`sample_rate_hz / symbol_rate_hz` and removes it from every API, and converts
everything normalized along with it (`center_freq_hz`, `pull_in_hz`,
`freq_hz`, timing offset in ppm). That page owns the rule — *an object that
owns a complete signal chain takes physical units; a stream processor takes
normalized ones* — and eleven objects, `carrier_acq` among them, already sit on
the physical side. This page does not restate it.

Taking a user's two rates rather than their ratio has a second effect worth
noting here, because it is about this refactor: **the irrational-`sps`
headline claim stops being exotic and becomes the ordinary case.** A caller
with a free-running ADC against a symbol clock supplies two rates and never
computes their ratio; `17.33389` is what the division happens to produce, not
something anyone typed.

So the constructor:

```text
/* the LINK — what is on the wire */
m                  constellation order
sample_rate_hz     required; no sane default (S7)
symbol_rate_hz     required
pulse              iandd | rrc   (+ rrc_beta when rrc)
center_freq_hz     where the carrier sits

/* the REQUIREMENT — the caller's units, all optional */
acquire_time_s     how quickly lock is needed
doppler_rate_hz_s  how fast the carrier moves
coherent           whether anything downstream pins absolute phase
```

**Where this extends §7 rather than applying it.** §7 converts `bn_carrier`
and `bn_timing` to Hz — still supplied, in better units. This page argues they
should not be supplied at all: a loop bandwidth is not a fact a caller has, it
is the answer to a requirement they do have. Each derivation is an equation
already in §5 rather than a new invention:

| derived                                                      | from                                   | equation                                                          |
| ------------------------------------------------------------ | -------------------------------------- | ----------------------------------------------------------------- |
| `bn_carrier`                                                 | the **binding** one of the three below | max of them                                                       |
| … seeding                                                    | `pull_in_hz`                           | `bn ≥ M·\|Δf\|` per symbol                                        |
| … acquisition                                                | `acquire_time_s`                       | lock time scales `~1/bn`, at 5–10% of `5/bn`                      |
| … ramp headroom                                              | `doppler_rate_hz_s`                    | `θ_ss = 2π·r/wn² < π/(2M)` ⇒ `wn² > 4M·r`                         |
| `nda_tap`                                                    | `pull_in_hz`                           | smallest `F ∈ {Rs, m_out·Rs, bank_sps·Rs}` with `F/(2M) ≥ \|Δf\|` |
| `bn_timing`                                                  | `bn_carrier`                           | the AGC must stay slower than both (§5.6)                         |
| `acq_to_track`                                               | `m`                                    | on at M = 8, whose ±π/8 margin the NDA jitter would cross         |
| `differential`                                               | `coherent`                             | the M-fold ambiguity is permanent without a sync word             |
| `m_out`, `zeta`, `lock_thresh`, `num_phases`, `bn_agc_ratio` | rates, M, Pfa                          | §5.6                                                              |

Two properties of this shape are what make it better rather than merely
smaller.

**A requirement the receiver cannot meet becomes a refusal, not a bad lock.**
A residual beyond `bank_sps·Rs/(2M)` has no tap that can see it. Today that is
a **stable false lock at `k·F/M`** — §3.5's "single quiet failure", invisible
to EVM, blind M2M4 and the lock statistic alike, and detectable only with an
external frequency reference. Derived, it is a `create()` returning NULL with
the reason, before the caller loses a pass. The same for a
`doppler_rate_hz_s` whose `θ_ss` leaves the linear range: that is arithmetic
the object can do up front.

**Every derived value stays readable.** The getters already exist, so a caller
asks what they got — `rx.bn_carrier`, `rx.nda_tap`, `rx.sps` — rather than
having had to supply it. That is the pattern §8.1 established for five
parameters, applied to the rest.

The escape hatch stays and stops being the default path: a post-construction
setter, legal before the first sample, for the caller who genuinely must pin
one. Pinning becomes a named call rather than a zero in a positional slot —
which is what let the real twin pin three values wrong (§4.1).

### 4.4 Acquisition goes in front; its job is to land inside the loop

`CarrierAcquisition` (`carrier_acq`) cascades ahead of the receiver, and
already takes `sample_rate_hz` / `symbol_rate_hz`, so the two agree on units
without conversion.

**`pull_in_hz` is not a receiver parameter.** The frequency uncertainty is the
*search*, and the search belongs to the stage that does it. `carrier_acq` has
no range knob at all — it searches the band it is given at a granularity set by
`resolution_hz` (0 derives `symbol_rate_hz/10`). Out of the box it should
therefore acquire and track anywhere in the **flat input bandwidth**, which is
a front-end property rather than something a caller states.

**The contract is exactly one thing: get the residual inside the carrier
loop's pull-in.** Not "be accurate" — the loop does the rest. That bound is the
seeding rule:

```text
|Δf| ≤ bn_carrier / M      cycles/symbol
     = bn_carrier · Rs / M  Hz

acq residual ≈ resolution_hz / 2

⇒  resolution_hz  ≤  2 · bn_carrier · symbol_rate_hz / M
```

With `bn_carrier = 0.01` as the standard, that is `Rs/200` at M = 4.

!!! warning "The two objects' defaults do not compose"

    `carrier_acq` derives `resolution_hz = symbol_rate_hz/10`, so its residual
    is `±Rs/20`. The seeding bound at `bn_carrier = 0.01` is `Rs/400` at
    M = 4. The default acquisition is therefore **20× too coarse to hand off**
    — 10× at M = 2 and 40× at M = 8, because the bound scales as `1/M` while
    the resolution does not.

    Nothing has caught this because nothing cascades them yet. It is the first
    thing the cascade has to fix, and it fixes itself: **`resolution_hz` is
    derived from the receiver's loop**, not defaulted independently.

### 4.5 The taps go away

`nda_tap` should be deleted, not defaulted. Three arguments, and the third is
the one that settles it.

**It offers no better option.** `strobe` wins on every axis measured — node
SNR, steady-state lock, transient depth at a data onset, recovery, and
insensitivity to a band-limited transmitter. `mf_in` costs
`10·log10(bank_sps)` dB and loses everywhere; `mf_out` carries an ISI bias that
appears the moment transitions exist and is documented as fatal at M = 8. A
knob whose alternatives are all worse is not a knob.

**Its stated benefit was never the binding constraint.** A tap buys pull-in
range `F/(2M)`. But the loop cannot *seed* beyond `bn·Rs/M`, and

```text
(F/(2M)) / (bn·Rs/M)  =  1/(2·bn)  =  50   at bn = 0.01
```

— independent of M. **The loop bandwidth binds fifty times before the tap's
ambiguity limit does**, so widening the tap solves a constraint that was never
active. That is why `rx_nda_tap.c` measured `mf_in`'s residual error as the
same order as `strobe`'s at every rate ratio and recorded "no evidence yet"
(gh-766): it was sweeping the wrong axis. With an acq in front the point is
moot twice over.

**And it is the sole cause of all three invariant violations in [§2.3](mpsk.md#23-the-invariant).**
That section records three places where a rate-keyed constant is declared in
the wrong clock's units, and every one of them exists *because a tap can run
faster than `Rs`*:

| §2.3 defect                                                                                                  | why deleting `nda_tap` removes it          |
| ------------------------------------------------------------------------------------------------------------ | ------------------------------------------ |
| the carrier loop filter's update period is set from the acquisition tap's clock and never re-set at handover | with one tap there is one clock, `upd = 1` |
| the lock EMA's `α` is per-update, so a fast tap shortens its memory in symbols and correlates its looks      | no tap is faster than `Rs`                 |
| the two `lockdet`s carry the same 8/32 counts on different clocks                                            | both are the symbol clock                  |

§2.3 already says the resolution: *"Mode 1 has one clock on the steering path
and one on the reporting path, and both are fixed at construction. None of the
three can arise."* Deleting the tap **is** that property, made structural.

What goes with it: `mpsk_rx_updates_per_symbol()` (always 1), the re-run of
`mpsk_rx_config_carrier()` after `create()` publishes `bank_sps`, `mf_in_sps`,
`tap_timed`, the `MF_OUT` branch in `mpsk_rx_take_output()`, and
`rx_nda_tap.c` entirely. `freq_scale` reduces to `(1/2π)/lo_sps` — the
expression gh-765 got wrong, with its variable term removed.

### 4.6 The pre-matched-filter node stays — as telemetry, not as a knob

**The node survives; the knob does not.** Deleting `nda_tap` removes a *control*
choice. It should not remove the only observation point between the front end
and the symbols, because that node is where a receiver is diagnosed when
something is wrong upstream of the loops: a pulse that is not the shape
declared, a rate that is not the rate stated, an AGC riding an interferer, a
band with nothing in it at all. By the time a signal reaches the strobe it has
been matched-filtered and strobed — both of which *hide* those faults by
design.

So `zpre` keeps its place in `ddc_execute_ctrl_push_tap2()`, and the receiver
publishes it as a **fixed-rate** decimated pair of probes alongside the ones it
already has:

```text
rx.mf_in.i   post-MIX, post-DEC, post-AGC, ahead of the MFR
rx.mf_in.q
```

**Fixed rate is the point.** `bank_sps` is a planner outcome, so the raw node
arrives at a rate that moves with the caller's geometry — fine for a
discriminator that only wants phase, useless for a human comparing two
captures. Decimating to a stated 2 samples/symbol is what
[§3.3](mpsk.md#the-2-samplessymbol-decimation-considered-and-declined)
proposed and declined, and the objection there was **serialized state**: a
decimator on the carrier path has to be packed, versioned and resumed
bit-exactly.

That objection does not apply to an observation path. Telemetry is already
excluded from the state blob by rule — `mpsk_rx_tlm_t` is documented as *"live
attachment; zeroed in state blobs"* — so a telemetry decimator's phase is
zeroed on restore like every other telemetry field. §3.3's trade was real for a
tap that steers, and simply is not the same trade for one that reports.

The shape has precedent in this very object: the AGC's two probes already sit
**pre-terminal on their own grid**, and the C test pins that they do
(`n_agc / 2 != n_sym`). Records carry sample indices, so `read_dict(index=True)`
puts a 2-sps pair and a 1-per-symbol probe on one time axis without either
knowing about the other.

And this is what the 6 dB finding was always describing. A node
`10·log10(bank_sps)` dB down on the terminal is a poor place to *steer* from
and a perfectly good place to *look* — the excess bandwidth that ruins an SNR
measure is exactly the wider view a diagnostic wants. It stops being a defect,
or even a price, and becomes the characteristic of an observation point.

Critically, **none of §2.3's three defects come back.** Every one of them is
about the clock on the *steering* path: the loop filter's update period, the
lock EMA's `α`, the two `lockdet` counts. A probe that steers nothing has no
say in any of them.

### 4.5 What stays per-face

Three things, and each is the rate convention rather than a parameter:

|                        | complex           | real                                                  |
| ---------------------- | ----------------- | ----------------------------------------------------- |
| `m_out` derivation cap | `sps`, inclusive  | `sps/2`, **strict**                                   |
| `sps` constraint       | `sps ≥ m_out`     | `sps > 2·m_out`                                       |
| `init_norm_freq` means | baseband residual | the real IF centre; the LO tunes via `−(2·f_c + 0.5)` |

### C — one core, two entry points, one set of accessors

```text
/* lifecycle: one create per front end, everything else shared */
mpsk_receiver_state_t *mpsk_receiver_create        (...);   /* complex */
mpsk_receiver_state_t *mpsk_receiver_create_real   (...);   /* real IF */
mpsk_receiver_state_t *mpsk_receiver_create_continuous (...);
void                   mpsk_receiver_destroy       (mpsk_receiver_state_t *);
void                   mpsk_receiver_reset         (mpsk_receiver_state_t *);

/* the hot path: the input type differs, the body does not */
int  mpsk_receiver_step_ted      (s, float complex x, float complex *y, int ted);
int  mpsk_receiver_step_real_ted (s, float         x, float complex *y, int ted);

/* block API, per input type */
size_t mpsk_receiver_steps      (s, const float complex *x, ...);
size_t mpsk_receiver_steps_real (s, const float         *x, ...);
size_t mpsk_receiver_bits       (s, const float complex *x, ...);
size_t mpsk_receiver_bits_real  (s, const float         *x, ...);

/* state: one triplet over the tagged front end */
size_t mpsk_receiver_state_bytes (const mpsk_receiver_state_t *);
void   mpsk_receiver_get_state   (const mpsk_receiver_state_t *, void *);
int    mpsk_receiver_set_state   (mpsk_receiver_state_t *, const void *);
```

**Every accessor collapses to one implementation**, which is the 16 pure
delegations disappearing: `get_m`, `get_sps`, `get_m_out`, `get_zeta`,
`get_num_phases`, `get_lock_thresh`, `get_bn_agc_ratio`, `get_lock`,
`get_locked`, `get_lock_time`, `get_last_error`, `get_tracking`,
`get_timing_rate`, `get_clipped`, `configure_lock`, `set_telemetry`.

Four stay front-end aware, and each carries the rate convention rather than
hiding it: `get_norm_freq`, `get_nco_freq`, `set_norm_freq`,
`get_agc_gain_db`.

### Python — three faces, one surface

| face                     | `steps(x)`           | pins                               |
| ------------------------ | -------------------- | ---------------------------------- |
| `MpskReceiver`           | `NDArray[complex64]` | —                                  |
| `MpskReceiverR`          | `NDArray[float32]`   | real front end                     |
| `ContinuousMpskReceiver` | `NDArray[complex64]` | `acq_to_track=0`, `strobe`, AGC on |

All three return `NDArray[complex64]`, share every property and method
verbatim, and differ only in their constructor and — for the real face —
`steps`/`bits` input dtype. That last part is jm#1012.

______________________________________________________________________

## 5. Governing equations

Read from the source, not from prose; each names where it lives.

### 5.1 Loop filter (both loops) — `loop_filter_core.c`

```text
wn  = 8·ζ·bn / (4·ζ² + 1)          rad per update
θ   = wn·t                          t = the update period, in symbols
den = 4 + 4·ζ·θ + θ²
kp  = 8·ζ·θ / den
ki  = 4·θ²   / den
```

`control = integ + kp·e`, and `integ += ki·e`, so **the integrator is the
frequency memory** and `kp·e` is the instantaneous phase nudge. This is why
`mpsk_rx_freq_est()` reads `integ` alone and not the control.

### 5.2 The carrier loop's update period and scale — `mpsk_rx_config_carrier()`

```text
upd        = 1        (STROBE)
           = m_out    (MF_OUT)
           = bank_sps (MF_IN)

t          = 1 / upd                       symbols per update
freq_scale = (1 / 2π) · upd / lo_sps       rad/symbol -> cycles/LO-sample
lo_sps     = sps      (complex front end)
           = sps / 2  (real front end — the R2C halfband decimates 2:1)
```

`bn_carrier` keeps its **symbol-rate** meaning at every tap: the tap changes
how often the loop is updated (`t`), not the loop. Getting `freq_scale` wrong
is gh-765, and it is invisible to a step test because a type-2 loop nulls a
step regardless of gain — only a **ramp** separates them.

### 5.3 Discriminators — `carrier_nda_disc()`, `mpsk_rx_take_output()`

With `p = |z|² = I² + Q²`, one divide at the first squaring so every later
squaring is of a unit vector:

```text
NDA, M = 2:   e = Im((z/|z|)²) = 2IQ/p          lock = Re((z/|z|)²) = (I²−Q²)/p
NDA, M = 4:   e = Im((z/|z|)⁴)/2                lock = Re((z/|z|)⁴)
NDA, M = 8:   e = Im((z/|z|)⁸)/4                lock = Re((z/|z|)⁸)

decision-directed:  e = Im(y·conj(â)) / |y|
```

The `{1, ½, ¼}` phase-error scaling equalises the S-curve slope across M, so
one `bn` means one loop at every constellation order. The **lock** signal is
left unscaled so it reads ≈1.0 at lock for every M.

### 5.4 Pull-in range, and what a tap costs

```text
|Δf| < F / (2M)          F = the tap's update rate

STROBE:  Rs/(2M)                   MF_OUT: m_out·Rs/(2M)
MF_IN:   bank_sps·Rs/(2M)          and costs 10·log10(bank_sps) dB
```

An M-th-power detector updating at `F` folds any error beyond `F/(2M)`, so the
tap point **is** the pull-in range. `MF_IN`'s cost is excess noise bandwidth,
not lost signal energy: DEC band-limits to its own Nyquist `±bank_sps·Rs/2`
while the signal occupies ~`±Rs`. Measured 6.01 dB at `bank_sps = 4`, identical
at 6.79 / 12 / 20 dB Es/N0 — a pure bandwidth ratio.

### 5.5 Lock detector — `carrier_nda_core.h`

Under H0 the phase is uniform, so `Var[Re(e^{jMθ})] = ½` for **every** M:

```text
σ_H0        = sqrt(½·α / (2−α)) = 0.11322770341445956   at α = 0.05
lock_thresh = η · σ_H0                                   per-look Pfa = Q(η)
              0.5 ⇒ η = 4.416 ⇒ Pfa = 5.0e-6
```

One threshold is therefore one false-alarm probability at every M — which is
the property that makes `lock_thresh` a plain fraction of what a locked
constellation reads.

### 5.6 Derived parameters — §8.1's "zero means derive"

```text
m_out        = 8 if h ≥ 4 else 2·h,   h = floor(lim/2),  lim from
               cap = sps (complex) or sps/2 (real), strict for the real twin
ζ            = 1/√2
num_phases   = 64
lock_thresh  = σ_H0 · η(Pfa = 5e-6)  = 0.4999
bn_agc       = ratio · min(bn_carrier, bn_timing),  ratio = 0.05
```

The AGC is derived off the **slowest** loop it feeds, so it cannot integrate
against either one.

### 5.7 Rate conventions — the real face

```text
ddcr tuning law:  norm_freq = −(2·f_c + 0.5)
get_norm_freq  =  centre_freq + 0.5 · mpsk_rx_freq_est(l)
get_nco_freq   =  centre_freq − 0.5 · l.freq_ctrl
constraint     :  sps > 2·m_out          (the cascade behind the halfband
                                          runs at twice the overall rate)
```

The `0.5` is the halfband's 2:1 decimation seen from the input rate. It belongs
to the **face**, not to the loops — which is exactly why the loops can be
shared and why "the LO runs at half the input rate" needs a test that neither
receiver currently has.

### 5.8 Ramp response — the measurement that ranks loops

```text
θ_ss = 2π·r / wn²        r = the Doppler rate, cycles/symbol²
```

A type-2 loop nulls a frequency **step** to zero steady-state error regardless
of gain, so a step test cannot size a loop. Against a ramp it holds a constant
phase lag with the closed form above, and the loop breaks when `θ_ss` leaves
the S-curve's linear range, ≈`π/(2M)`.

______________________________________________________________________

## 6. The blocker — RESOLVED in jm 0.62.0

**This section is kept as the record of what was measured; the constraint it
describes no longer holds.** jm **0.62.0** (2026-08-16) ships both filed
issues, and doppler is pinned to it (#806, merged). A view method restating
a parent's name
may now declare its own `arg_type`/`return_type`, bound to its own C symbol
via **`fn`** — which is the discriminator, and not as a convenience: the
parent's C symbol carries the parent's prototype, so a different signature is
only callable through a different symbol.

```toml
[[mpsk_receiver.views.methods]]
name        = "steps"                     # same Python name
fn          = "mpsk_receiver_steps_real"  # its own C symbol
arg_type    = "float"                     # honoured, not discarded
return_type = "float _Complex"
```

Verified on doppler's exact shape before the pin moved, rather than from the
release notes — a scaffold with a `cf32` `steps` and a view overriding it to
`f32` gave `NPY_COMPLEX64`/`rx_steps` on the parent and `NPY_FLOAT`/
`rx_steps_real` on the view, same Python name, both prototypes in the shared
header, the parent untouched. **So §4's per-face `steps()` is real, and the
`steps_r()` fallback below is retired.**

### What was measured while it was a blocker

A jm view used to share its parent's methods **verbatim**: it could ADD a
method with its own `arg_type`, or override a parent method's **doc**, and
nothing else. Scaffolded against jm **0.61.0 and 0.61.1**, three routes:

| route                                          | result                                                                                                                        |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `exclude_methods = ["steps"]` + a view `steps` | **explicit error** — "both excluded and added on view … contradictory"                                                        |
| a view `steps` with no exclude                 | **accepted, `arg_type` silently ignored** — the `.pyi` *and* the view's own C fragment both keep the parent's `NPY_COMPLEX64` |
| a view method with a **new** name (`steps_r`)  | **works end to end** — own `arg_type`, own binding, own `_max_out`                                                            |

Filed upstream as **just-makeit#1011** (the silent ignore — jm already detects
this exact collision on the exclude path, so the check simply was not applied
here) and **just-makeit#1012** (the feature: let a view override a *signature*,
not just a doc). They were linked, and #1011 closed either way: implement the
override, or error as the exclude path already does. jm did the first, so
route 2 is now honoured with `fn` and refused by name without it.

Route 3 was the fallback — `steps_r()` / `bits_r()` on the real face, possible
even then, at the cost of an asymmetric public surface: two classes with the
same semantics, one spelling its block API differently, which is most of the
way back to being a second type. **It is no longer needed.** It is recorded
because it is what made this a trade rather than a hard blocker, and that is
why §4 could be written before the fix shipped.

______________________________________________________________________

## 7. Why now — the claim inventory

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

## 8. Sequence

1. The evidence, landing separately. The `strobe` pin is **#807**; this
    page's own branch carries `native/validation/rx_dynamics.c` — the
    measurement that ranks the taps on the continuous flavor's own waveform —
    and the corrected `nda_tap` cost in `mpsk_rx_loops.h`. (An earlier branch,
    #792, carried the pin and the harness together and was closed in favour of
    that split.)

1. The C tests the inventory found missing, each proven by sabotage, in
    priority order: the `m_out = 2` + `IANDD` degeneracy; `m_out = 8` at
    M = 8 against the bound; **the handover carrying the frequency estimate
    across a drop-back** — the one whose failure is silent and expensive; and
    `bn_carrier`'s symbol-rate normalisation. The `σ_H0` / Pfa row belongs to
    `carrier_nda`'s own test, not here.

1. This collapse. **Unblocked** — just-makeit#1012 shipped in jm 0.62.0 (§6),
    which doppler is pinned to (#806, merged), so it uses a real per-face
    `steps()`. Nothing else gates this step.

1. **Give the shared header its test home — the step that cashes §2.** The
    collapse *permits* one home; it does not create one. Left here, the merged
    object simply inherits whichever twin's test file survives, and §2's
    asymmetry persists silently into it — the refactor delivering everything
    except the thing that motivated it. So the collapse is not done until each
    row of §2's second table has a single owner:

    | shared claim                         | today             | after                       |
    | ------------------------------------ | ----------------- | --------------------------- |
    | `set_telemetry`                      | complex only      | migrate onto the one object |
    | level invariance                     | complex (real: 1) | migrate                     |
    | the AGC is slower than every loop    | complex only      | migrate                     |
    | "the LO runs at half the input rate" | **neither**       | **new test**                |

    The first three are migrations and carry no new risk: they already pass on
    one side, and the point is that one object cannot have a side. The fourth
    is the one that matters, because it is not a migration — nothing has ever
    asserted it, and it is where the gh-765 `freq_scale` bug lived (fixed in
    #772, merged 2026-08-15, with **no guard left behind**). It gets a
    regression test with a named sabotage target: **revert #772's `freq_scale`
    and it must go red.** A test for that row which passes against the
    pre-#772 loop is not testing the convention — recall that the bug survived
    every step test in the tree, because a type-2 loop nulls a frequency step
    regardless of gain, so the stimulus has to be a **ramp** (see
    `docs/design/mpsk.md` §5.8).

    And the claim §2 ends on — *the loops behave identically regardless of
    front end* — becomes writable for the first time here, because one object
    with two faces can run the same vector through both and compare.

1. The validation report, written against the collapsed object, so one object
    has one report.

______________________________________________________________________

## 9. Decisions this page records

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
