---
description: "the BPSK carrier loop every despreader and DSSS receiver embeds: what one update does, what it owns, and a second loop order it can switch to"
---

# Costas

`Costas` is the BPSK carrier loop the despreaders and the DSSS receivers
embed by value. It is two inline steps and the state between them:

- **`costas_wipeoff()`**, once a sample, de-rotates the input by the
    integer-phase `lo` NCO.
- **`costas_update()`**, once an update (every `tsamps` samples), turns one
    prompt `P` into a phase error, filters it, and steers the NCO.

The block API, `dp_costas_steps()`, runs both over an integrate-and-dump of
`tsamps` samples. It is the Python face (`doppler.track.Costas`). An
embedder that forms its own prompt (the asynchronous receiver squares and
sums partials, then hands `costas_update()` a unit phasor) calls the two
inline steps from its own loop.

## 1. What one update does

1. **Discriminator.** Decision-directed BPSK:
    `e = sign(Re P)·Im P / |P|`, which is `sin` of the phase error for a
    unit phasor.
1. **FLL assist**, when `bn_fll > 0` (order 2 only, §3.2). A
    cross-product frequency error of two data-wiped prompts, added straight
    into `lf`'s frequency estimate (`k_fll = 4·bn_fll`).
1. **Filter.** The active one (§3): the embedded
    `dp_loop_filter_state_t lf`, or at order 3 the `StateFilter` `sf`, at
    `t = 1` (one update), so `bn` is in cycles per update. `integ` is the
    frequency estimate in radians per update.
1. **Steer.** The NCO frequency is set to `integ / tsamps` radians per
    sample, and its phase steps by `kp·e`.
1. **Lock.** `|Re P|/|P|` feeds an exponential average
    (`COSTAS_LOCK_ALPHA`), and `dp_lockdet` decides `locked` on it.

The loop is the canonical one of loop-filter.md §2 (detector, filter,
accumulating oscillator). The proportional term is applied as a phase step
at the update, and the new frequency from the next update on, which over
one update is the canonical oscillator's `c = integ + kp·e`. So
`dp_state_filter_loop_bandwidth()` is Costas's bandwidth, at both orders,
for the phase at the detector instant. A phase read after the update, past
the `kp·e` step, has a smaller sum of squares, so a jitter figure states
where it reads the phase.

## 2. What it owns

| field                      | what                                                       | in the blob |
| -------------------------- | ---------------------------------------------------------- | ----------- |
| `nco`                      | the integer NCO: phase and frequency                       | yes         |
| `lf`                       | the second-order loop filter: gains, `bn`, `zeta`, `integ` | yes         |
| `sf`                       | the third-order filter: gains, `bn`, `integ`, `rate`       | yes         |
| `order`                    | the active filter, 2 or 3 (§3)                             | yes         |
| `tsamps`, `seed_norm_freq` | the update period and the frequency `reset` returns to     | yes         |
| `bn`, `zeta`               | the loop's retained configuration (equal to `lf`'s)        | yes         |
| `bn_fll`, `k_fll`          | the FLL assist                                             | yes         |
| `acc`, `acc_n`             | the integrate-and-dump in progress (block API)             | yes         |
| `prev`, `have_prev`        | the previous prompt, for the FLL                           | yes         |
| `lock_metric`, `lock`      | the lock average and its decision rule                     | yes         |
| `last_error`               | the last discriminator output                              | yes         |
| `tlm`                      | the telemetry attachment                                   | zeroed      |

The blob is the struct whole (`COSTAS` v4), with the telemetry zeroed and
restored live, and `set_state` refuses a snapshot `costas_state_ok()`
rejects. That check uses the same predicates as create and the setters
(#2022).

Embedded by value in `Despreader` (`car`), `DsssReceiver` (`car`) and
`AsyncDsssReceiver` (`car`, `car_held`, `car_frozen`). Each nests the
Costas blob as a child sub-blob.

**The all-zero Costas.** A receiver holds its spare loops all zero, never
initialised (`tsamps` 0, `order` 0), until it seeds them, and its blob
carries them that way. That one state is accepted as it is; every other
state has `order` 2 or 3.

## 3. The second loop order

The order-2 filter cannot hold a frequency ramp at a bandwidth narrow
enough for the noise. Its steady error on a ramp is `x/ki`, and `ki` falls
as `bn²`. At the asynchronous DSSS receiver's floor that is tens of
degrees ([loop-filter.md](loop-filter.md) §10, use case 1). A third-order
filter has none, so Costas holds one. `StateFilter`, its own certified
object, is the filter.

### 3.1 Shape: both filters held, one active

`dp_costas_state_t` holds a `dp_state_filter_state_t sf` and `order`,
which selects the active filter, at the end of the struct, after every
field the per-sample loop touches (`nco`, `tsamps`, `acc`, `acc_n`), so
the order-2 loop's memory layout is the one it has without them.
loop-filter.md §13 anticipates this embedder: acquire on one order, track
on the other.

- **`costas_update()`** steps the active filter and steers from its
    `integ` and `kp`.
- **The getters read the active filter:** `get_norm_freq`,
    `get_nco_freq`, `get_bn`, and the telemetry's `.freq` and `.nco`.
- **`lf` always carries the retained `bn` and `zeta`.** At order 3, `sf`
    runs at the same `bn`. `configure(bn, zeta)` retunes both, keeping the
    estimates, and refuses a `bn` outside either filter's domain. Order 3
    has no damping argument (loop-filter.md §11), so `zeta` only reaches
    `lf`.
- **`reset`** keeps the order, as it keeps `bn`, `zeta` and the lock
    thresholds. It rewinds the active filter's `integ` to the create-time
    seed and zeroes `rate`. `set_norm_freq` does the same at a new seed.

**Create and init do not change.** A loop is created at order 2, with
`sf` all zero.

### 3.2 The switch, the rate, and the FLL

`int dp_costas_set_order (dp_costas_state_t *s, int order, double bn)`
selects an order on a live loop. It returns `DP_OK`, or `DP_ERR_INVALID`
with nothing changed in these cases:

- the order is not 2 or 3;
- `bn` is outside `dp_loop_filter_params_ok` at the retained `zeta`, or at
    order 3 outside `dp_state_filter_params_ok` at `t = 1`;
- order 3 is asked while `bn_fll > 0`.

What it carries:

- **2 to 3:** `sf` is initialised at `bn`, takes `lf.integ`, and its
    `rate` starts at 0.
- **3 to 2:** `lf` takes `sf.integ`, and the rate is dropped.
- **Either way:** `lf` is retuned to `bn`, keeping its estimate. The NCO's
    phase and frequency, the lock average and decision, `prev` and the
    integrate-and-dump are untouched. So the NCO frequency on the update
    after a switch is the frequency the loop held before it.
- **To the same order:** a retune that keeps the estimates.

Costas does not decide when to switch. The embedder does (the receiver on
its lock event), because only it knows what a lock means for its signal.

**The rate has its own setter.** `dp_costas_set_norm_rate` seeds it, as
`set_norm_freq` seeds the frequency. It is in cycles per sample per
sample (`R / fs²` for `R` Hz/s), stored as `2π·val·tsamps²` radians per
update per update. It is refused at order 2, which holds no rate.
`dp_costas_get_norm_rate` reads it back, or 0 at order 2. One call, one
meaning: `set_order` does not also seed a rate, so the mutator gate sees
the rate through its own property.

**The FLL assist is refused at order 3.** The assist is a first-order
frequency pull into the filter's frequency estimate. With a rate
integrator behind that estimate it is a different loop, which StateFilter's
stability range was not derived for and which has not been measured.
`set_order(3)` refuses while `bn_fll > 0`, `set_bn_fll` refuses a positive
value at order 3, and the blob check refuses the combination. That costs the
asynchronous receiver nothing (it runs `bn_fll = 0`). `DsssReceiver`, which
runs 0.03, stays at order 2. The header says why.

### 3.3 The blob

`COSTAS` is v4, and its blob carries `sf` and `order`. The order is a mutator's
value, so it travels and is checked, not used as a reject key
(state-serialization.md, "What goes in the blob", rule 2). `costas_state_ok()`
adds three checks:

- the order is 2 or 3, or 0 in the all-zero Costas;
- at order 3, `sf` runs at the retained `bn` and the FLL assist is off;
- `sf` is either the all-zero filter (never selected) or one
    `dp_state_filter_state_ok()` accepts.

**The parents keep their versions and are refused by size.**
state-serialization.md: "A composition embeds each child as a self-contained
sub-blob — the child carries its own header, so it is independently
validatable, and migrating a leaf (which changes its size) propagates
automatically because the parent sums `child_state_bytes`." Its rule for a
bump, "a blob that gains a field bumps the object's `*_STATE_VERSION`", is the
child's, not the parent's. `test_despreader_core.c` pins it on the despreader:

- a despreader blob with a Costas v3 child, 64 bytes smaller, both headers
    so, is refused, with the despreader untouched;
- the same blob with only the size changed, its Costas header at v4, is
    refused too, so the parent's size refuses it and not only the child's
    version.

### 3.4 What each embedder pays

- **Size:** `sf` and `order` are 64 bytes, the 56 of `sf` plus the order
    word padded to 8. The receiver holds three Costas, and the pool holds
    three per receiver.
- **Time:** `costas_wipeoff()`, once a sample, has no order in it.
    `costas_update()` has one branch on `order`, once an update, beside a
    discriminator that divides and takes an absolute value. The bench
    rows `costas::steps` and `costas::steps[order=3]` time the two orders.

### 3.5 What is pinned

Costas has no certification yet
([#3138](https://github.com/doppler-dsp/doppler/issues/3138)); these are
its C pins.

**Order 2:**

- every existing `test_costas_core.c` pin, and the embedders' C tests;
- an **independent order-2 recurrence** in `test_costas_core.c` §10e,
    written from this page's §1 and not from `costas_update()`, which
    matches the library **to rounding**.
    - **How it is held:** the library and its tests build with
        `-ffast-math`, so a second translation unit may compute the float
        error `e` differently by a few roundings. The reference's filter and
        steer therefore run on the library's own prompt each update, and two
        quantities are held to bounds derived from the run's length `N`,
        not picked:
        - the NCO phase word: a count a sample, `N·(tsamps + 1)` counts;
        - the reference's own prompt: float accumulation,
            `2⁻²³·xmax·(tsamps + 2)`, plus `xmax` times the phase bound.
    - The frequency estimate is not held: its bound would rest on the
        few-roundings premise, which only an arm64 run checks, and it sees
        only one of the two sabotages the phase and the prompts catch.

Bit for bit is kept where both sides are the library, compared with
itself: the 2 to 3 to 2 switch against staying at order 2, and the
mid-stream blob split (§10b, §10d). Both pass under gcc and clang.

**Order 3, pinned and sabotaged on its own** (§10a to §10d):

- zero steady phase error on a frequency ramp, where order 2 holds its
    `x/ki` (`dp_state_filter_steady_error` is the expectation for both);
- the switch:
    - the NCO frequency and phase are unchanged across it;
    - `integ` carries both ways;
    - a seeded rate is held;
    - 2 to 3 to 2 with no update between is the order-2 loop, bit for bit;
- an order-3 blob split mid-stream resumes bit for bit;
- refusals that change nothing:
    - forged blobs: order 4, `sf.bn` not the retained `bn`, an FLL assist
        at order 3;
    - `set_order` outside each order's domain, and with the FLL on;
    - `set_bn_fll` at order 3, and `set_norm_rate` at order 2 or NaN.
- Each was sabotaged and went red: order 3 stepping `lf`; no carry 2 to 3;
    no carry 3 to 2; the order unchecked; the FLL allowed at order 3; the
    rate gain zeroed.

### 3.6 The Python face

In `objects/costas.toml`:

- `set_order(order, bn)`;
- a read-only `order`;
- a writable `norm_rate`, refused at order 2.

The serialization matrix has an order-3 row, `Costas (order 3)`, with a
rate seeded. Through it the mutator gate (`test_mutator_state.py`) shows
`norm_rate` travels in the blob, and the matrix round-trips order 3 from
Python.

### 3.7 Open

- **The order is the embedder's to choose** (§3.2); Costas does not switch
    on its own.
- **A Kalman flavor of the filter** is `StateFilter`'s (loop-filter.md
    §13.1); Costas holds whichever `StateFilter` that produces.
- **The FLL assist at order 3** is refused until it is measured
    ([#3138](https://github.com/doppler-dsp/doppler/issues/3138)).
- **How long order 3 takes to pull in** past the order-2 bound
    ([#3123](https://github.com/doppler-dsp/doppler/issues/3123)).
- **A C example**
    ([#3137](https://github.com/doppler-dsp/doppler/issues/3137)).
