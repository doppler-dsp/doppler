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
1. **FLL assist**, when `bn_fll > 0`. A cross-product frequency error of
    two data-wiped prompts, added straight into the filter's frequency
    estimate (`k_fll = 4·bn_fll`).
1. **Filter.** The embedded `dp_loop_filter_state_t lf` at `t = 1` (one
    update), so `bn` is in cycles per update. `integ` is the frequency
    estimate in radians per update.
1. **Steer.** The NCO frequency is set to `integ / tsamps` radians per
    sample, and its phase steps by `kp·e`.
1. **Lock.** `|Re P|/|P|` feeds an exponential average
    (`COSTAS_LOCK_ALPHA`), and `dp_lockdet` decides `locked` on it.

The loop is the canonical one of loop-filter.md §2 (detector, filter,
accumulating oscillator), with one difference: the proportional term is
applied as an immediate phase step rather than added to the next update's
increment.

## 2. What it owns

| field                      | what                                                       | in the blob |
| -------------------------- | ---------------------------------------------------------- | ----------- |
| `nco`                      | the integer NCO: phase and frequency                       | yes         |
| `lf`                       | the second-order loop filter: gains, `bn`, `zeta`, `integ` | yes         |
| `tsamps`, `seed_norm_freq` | the update period and the frequency `reset` returns to     | yes         |
| `bn`, `zeta`               | the loop's retained configuration (equal to `lf`'s)        | yes         |
| `bn_fll`, `k_fll`          | the FLL assist                                             | yes         |
| `acc`, `acc_n`             | the integrate-and-dump in progress (block API)             | yes         |
| `prev`, `have_prev`        | the previous prompt, for the FLL                           | yes         |
| `lock_metric`, `lock`      | the lock average and its decision rule                     | yes         |
| `last_error`               | the last discriminator output                              | yes         |
| `tlm`                      | the telemetry attachment                                   | zeroed      |

The blob is the struct whole (`COSTAS` v3), with the telemetry zeroed and
restored live, and `set_state` refuses a snapshot `costas_state_ok()`
rejects. That check uses the same predicates as create and the setters
(#2022).

Embedded by value in `Despreader` (`car`), `DsssReceiver` (`car`) and
`AsyncDsssReceiver` (`car`, `car_held`, `car_frozen`). Each nests the
Costas blob as a child sub-blob.

## 3. The second loop order

**Proposed, not built.** The order-2 filter cannot hold a frequency ramp
at a bandwidth narrow enough for the noise. Its steady error on a ramp is
`x/ki`, and `ki` falls as `bn²`. At the asynchronous DSSS receiver's floor
that is tens of degrees ([loop-filter.md](loop-filter.md) §10, use case 1).
A third-order filter has none, so Costas gains it. `StateFilter`, its own
certified object, is the filter.

### 3.1 Shape: both filters held, one active

`dp_costas_state_t` holds a `dp_state_filter_state_t sf` beside `lf`, and
`int order`, 2 or 3, which selects the active one. loop-filter.md §13
anticipates this embedder: acquire on one order, track on the other.

- **`costas_update()`** steps the active filter and steers from its
    `integ` and `kp`. The FLL assist adds into the active filter's `integ`.
- **The getters read the active filter.** `get_norm_freq`,
    `get_nco_freq` and `get_bn` all do, and so do the telemetry's `.freq`
    and `.nco`. The inactive filter is held but not read.
- **`configure(bn, zeta)`** retunes the active filter, keeping its
    estimates. At order 3 `zeta` is accepted and ignored: order 3 has no
    damping argument (loop-filter.md §11).
- **`reset`** keeps the order, as it keeps `bn`, `zeta` and the lock
    thresholds. It rewinds the active filter's `integ` to the create-time
    seed and zeroes `rate`.

**Create and init do not change.** A loop is created at order 2. An
existing caller gets the same struct contents in `lf` and the same output,
bit for bit.

### 3.2 The switch

`int dp_costas_set_order (dp_costas_state_t *s, int order, double bn, double norm_rate)` selects an order on a live loop. It is `DP_OK`, or
`DP_ERR_INVALID` with nothing changed when the order is not 2 or 3, or
when `bn` is outside that order's domain (`dp_loop_filter_params_ok` at
the retained `zeta`; `dp_state_filter_params_ok` at `t = 1`).

- **2 to 3:**
    - `sf` is initialised at `bn` and takes `lf.integ`.
    - `rate` is `2π·norm_rate·tsamps²` radians per update per update, from
        `norm_rate` in cycles per sample per sample (`R / fs²` for `R` Hz/s).
        Zero means no rate is known, which is the default because a rate
        learned by a wide order-2 loop is not there to carry (loop-filter.md
        §13).
- **3 to 2:** `lf` is configured at `bn` and takes `sf.integ`. `rate` is
    dropped, and `norm_rate` is ignored.
- **Either way:** the NCO's phase and frequency, the lock average and
    decision, `prev` and the integrate-and-dump are untouched. So the NCO
    frequency on the update after a switch is the frequency the loop held
    before it, and the switch steps nothing.
- **To the same order:** a retune by `configure` that keeps the estimates,
    except that 3 to 3 also seeds `rate` when `norm_rate` is not zero.

Costas does not decide when to switch. The embedder does (the receiver on
its lock event), because only it knows what a lock means for its signal.

### 3.3 The blob

`COSTAS` goes from v3 to v4, because the struct grows by `sf` and `order`.
The order is a mutator's value, so it travels and is checked, not used as
a reject key (state-serialization.md, "What goes in the blob", rule 2).
`costas_state_ok()` adds three checks:

- the order is 2 or 3;
- the retained `bn` equals the active filter's;
- `sf` is either the all-zero filter (never selected) or one
    `dp_state_filter_state_ok()` accepts.

Every parent nests Costas as a child sub-blob. Its size changes and its
version does not: a composition sums `child_state_bytes`, and Costas's own
envelope refuses an old blob.

| blob                           | version on main | with this work |
| ------------------------------ | --------------- | -------------- |
| `COSTAS` (`CSTS`)              | v3              | v4             |
| `DESPREADER` (`DSPR`)          | v5              | v5, larger     |
| `DSSS_RECEIVER` (`DSRX`)       | v4              | v4, larger     |
| `ASYNC_DSSS_RECEIVER` (`ADRX`) | v17             | v17, larger    |
| `ASYNC_DSSS_POOL` (`ADPL`)     | v7              | v7, larger     |

### 3.4 What each embedder pays

- **Size:** the struct grows by 64 bytes, the 56 of `sf` plus the order
    word padded to 8. The receiver holds three Costas, and the pool holds
    three per receiver.
- **Time:** `costas_wipeoff()`, once a sample, does not change.
    `costas_update()` gains one branch on `order`, once an update, against a
    discriminator that already divides and takes an absolute value.
- **The proof:** `make bench-ab A=<main> B=<branch>` on the `costas`,
    `despreader`, `dsss_receiver` and `async_dsss_receiver` rows, all at
    order 2, must tie.
- **If they do not tie,** the fallback is two update functions,
    `costas_update()` unchanged and an order-3 sibling, with the embedder
    choosing. That costs a second steering path, so it is taken only on a
    measurement.

### 3.5 Certification

**Order 2, unchanged and shown unchanged:**

- every existing `test_costas_core.c` pin;
- `validate_costas_pullin --check` and `validate_costas_jitter --check`;
- the bench rows above;
- and one new pin: the order-2 output is bit-identical with `sf` and
    `order` present.

**Order 3, pinned and sabotaged on its own:**

- zero steady phase error on a frequency ramp, where order 2 holds its
    `x/ki` (`dp_state_filter_steady_error` is the expectation for both);
- the switch:
    - the NCO frequency is continuous across it;
    - `integ` is equal across it;
    - a seeded `rate` is held;
    - 2 to 3 to 2 with `rate` zero is the order-2 loop with the same
        `integ`;
- the blob round trip at each order, mid-stream;
- refusals:
    - a blob with order 4;
    - a blob whose retained `bn` is not the active filter's;
    - `set_order` outside each order's domain, with nothing changed.

**The validators:**

- `costas_pullin` gains order 3, with the same assertions in `--check` and
    the sweep: it acquires at and inside `bn/m` on both signs, and the bound
    scales with `bn`.
- `costas_jitter` gains order 3, with its noise gain from
    `dp_state_filter_loop_bandwidth` (`G = 2·Bn` cycles per update, the
    Parseval sum the order-2 rows compute from `kp` and `ki`). The same
    linearization-factor band and threshold assertions apply at each order.
- Whether order 3's pull-in reaches `bn/m` at all is open
    ([#3123](https://github.com/doppler-dsp/doppler/issues/3123)). If it
    does not, the bound the validator asserts for order 3 is the one it
    measures, stated as such, and not `bn/m` relabeled.

### 3.6 The Python face

`set_order(order, bn, norm_rate=0.0)` and a read-only `order` property, in
`objects/costas.toml`. The order and both filters are in the blob, so the
mutator gate (`test_mutator_state.py`) holds them like every other setter.

### 3.7 Not in this work

- **Choosing the order automatically:** it is the embedder's.
- **A Kalman flavor of the filter:** that is `StateFilter`'s (loop-filter.md
    §13.1). Costas holds whichever `StateFilter` that work produces.
- **Moving `bn_fll` into a third filter:** it stays an addition into the
    active filter's frequency estimate.
