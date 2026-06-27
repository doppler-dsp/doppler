# MPSK Receiver

**Status:** design / pre-implementation
**Scope:** a streaming **M-PSK receiver** (`track.MpskReceiver`, M = BPSK / QPSK
/ 8PSK) that demodulates pulse-shaped baseband by **composing existing
`doppler.track` primitives** plus two new ones. C-first: every block below is a
C core; the Python face is the jm-generated thin wrapper. This is the
architecture, the carrier-recovery design (the part most easily
miscommunicated), and the sequential build plan.

Related: [carrier loop theory](../gallery/carrier-mpsk.md) (the decision-directed
`CarrierMpsk` loop, already shipped), the async DSSS despreader
([design](async-symbol-despreader.md)) — DSSS-MPSK is the pipeline
`Dll(segments) → MpskReceiver`, not a fused object.

______________________________________________________________________

## 1. Architecture (option 1: pulse-shaped modem)

One **per-sample inline loop** (mirrors `channel_core.h`), not a block cascade —
a block cascade cannot feed the carrier error back per sample.

```mermaid
flowchart TB
    IN["rx (cf32)"]
    subgraph RX["MpskReceiver — one per-sample inline loop"]
        direction TB
        WIPE["carrier wipe-off<br/>per-sample integer NCO"]
        ARM["I/Q arm I&D<br/>(N× per symbol)"]
        NDA["NDA M-th-power disc<br/>z² → z⁴ → z⁸<br/>→ phase_error + lock"]
        MF["matched filter<br/>I&D default / RRC opt-in"]
        SS["symsync<br/>Gardner + Farrow<br/>(carrier-blind)"]
        DD["decision-directed disc<br/>e = Im(y·conj â)/|y|"]
        SEL{"carrier error<br/>opt-in auto-handover on lock"}
        LF["loop filter (PI)"]
        STEER["steer shared NCO<br/>(freq + phase)"]
        WIPE -->|"de-rotated d"| ARM
        WIPE -->|"de-rotated d"| MF
        ARM --> NDA
        MF --> SS
        SS -->|"symbol y_k"| DD
        NDA -->|"acquisition"| SEL
        DD -->|"tracking"| SEL
        SEL --> LF
        LF --> STEER
        STEER -.->|"feedback"| WIPE
    end
    IN --> WIPE
    SS -->|"y_k"| OUT["symbols() → cf32 y_k"]
    SS --> BITS["bits() → Gray bits<br/>(opt-in differential)"]
```

- **Matched filter:** **integrate-and-dump (boxcar) is the default**; RRC is
    **opt-in** (`beta`, `span`). Both are linear FIRs feeding `symsync`.
- **Timing** is carrier-blind (Gardner `|·|²`), so it settles in parallel with
    carrier acquisition and can lead it.
- **Carrier de-rotation is per-sample, before the matched filter** — see §2.
- **DSSS-MPSK** is the downstream pipeline `Dll(segments) → MpskReceiver`; the
    despreader removes the PN code and hands samples to this modem. Not fused.

______________________________________________________________________

## 2. Carrier recovery — the design that's easy to get wrong

The rule is **predetection de-rotation, postdetection discrimination**, with a
twist for cold start.

### 2.1 De-rotation is per-sample, always

The integer-NCO wipe-off runs on **every input sample, before anything else**.
A residual carrier rotating across an integration window costs sinc energy; the
window here is the matched filter (short for I&D ≈ `sps`, long for RRC ≈
`2·span·sps+1`), so per-sample de-rotation is the general-purpose "just works"
placement. It costs more compute than de-rotating symbols, and that is the
accepted trade — it is correct for every mode (I&D, RRC, large residual, and the
DSSS front-end) without special-casing.

### 2.2 Two discriminators share one NCO + loop filter

A decision-directed loop alone cannot cold-start: it needs symbol decisions,
which need timing lock and (for the error to mean anything) data. Many real
links must **acquire the carrier with no symbol timing and no data present**
(e.g. a bare/unmodulated carrier, or before timing settles). So the carrier loop
has **two error sources into one NCO**:

1. **Acquisition — non-data-aided (NDA) M-th-power discriminator** on the I/Q
    **arm integrate-and-dump at N× the symbol rate** (integrate `sps/N` samples,
    dump `N` times per symbol; **N is a config param, default 4**). It strips the
    M-PSK modulation by raising the arm sample to the Mth power, so it is
    independent of data and of symbol timing. This is the robust cold-start path.

1. **Tracking — decision-directed** `e = Im(y·conj(â))/|y|` on the full-SNR
    recovered symbol `y_k` (the `CarrierMpsk` discriminator, already shipped and
    validated). Low jitter once timing + lock are established.

### 2.3 The NDA discriminator + lock signal (canonical definition)

The M-th-power detector is computed efficiently by **repeated complex squaring**
of the arm sample `z = i + jq`: `z²` strips BPSK, `z⁴` strips QPSK, `z⁸` strips
8PSK. Each squaring level yields both a phase error and a lock signal; the
per-M `lock_scale` normalizes the discriminator/lock gain so the handover
threshold is M-independent.

```python
# i and q are arm filter outputs at N samples per symbol

bpsk_lock = i**2 - q**2          # Re(z^2)
bpsk_phase_error = 2 * i * q     # Im(z^2)

if mod == "BPSK":
    lock_scale = 1
    phase_error = bpsk_phase_error
    lock_signal = lock_scale * bpsk_lock
elif mod == "QPSK":
    lock_scale = 0.619
    phase_error = bpsk_phase_error * bpsk_lock                        # ~ Im(z^4)
    lock_signal = lock_scale * (bpsk_lock**2 - bpsk_phase_error**2)   # ~ Re(z^4)
else:  # 8PSK
    lock_scale = 0.412
    qpsk_phase_error = bpsk_phase_error * bpsk_lock
    qpsk_lock = bpsk_lock**2 - bpsk_phase_error**2
    phase_error = qpsk_phase_error * qpsk_lock                        # ~ Im(z^8)
    lock_signal = lock_scale * (qpsk_lock**2 - qpsk_phase_error**2)   # ~ Re(z^8)
```

- `phase_error` ≈ `Im(z^M)` (scaled) — a sawtooth S-curve of period `2π/M`, the
    M-fold phase ambiguity (consistent with the `CarrierMpsk` S-curve). It steers
    the NCO; the M-fold ambiguity is resolved downstream (differential demap or a
    sync word), same as the decision-directed loop.
- `lock_signal` ≈ `Re(z^M)` (scaled) — large and positive when phase-locked,
    ~0 with no carrier. It is **the lock metric that drives handover** and is the
    receiver's carrier lock indicator.

The squaring-loss/noise behaviour worsens with M (each squaring multiplies
noise), so the NDA loop is the *acquisition* aid; decision-directed tracking
gives the low-jitter steady state.

#### Derivation — the recursion *is* the M-th-power loop

Write the arm sample `z = i + jq`. The first level is literally `z²`:

```
bpsk_lock        = i² − q² = Re(z²)
bpsk_phase_error = 2iq     = Im(z²)
```

Each subsequent level squares the running pair and reads off its real/imaginary
parts, so `(lock, phase_error)` climbs the powers `z² → z⁴ → z⁸`. Verified
exactly (residual 0 over a full phase sweep):

| M   | `phase_error` | `lock_signal`                 |
| --- | ------------- | ----------------------------- |
| 2   | `Im(z²)`      | `Re(z²)`                      |
| 4   | `½·Im(z⁴)`    | `0.619·Re(z⁴)`                |
| 8   | `¼·Im(z⁸)`    | `0.412·(Re(z⁴)² − ¼·Im(z⁴)²)` |

So **`phase_error` is exactly the M-th-power discriminator** `Im(z^M)`, scaled by
`1, ½, ¼`. That scale is not arbitrary — it **normalizes the phase-detector gain
across M**. The S-curve slope at lock is `(slope of Im(z^M)) × scale = M × scale = 2·1, 4·½, 8·¼ = 2` for every M, so one loop-filter `bn` behaves identically for
BPSK / QPSK / 8PSK. (This is why the recursion carries `ab = Im(z⁴)/2` rather
than the full `2ab` into the next squaring.)

The **`lock_signal` is `Re(z^M)` exactly for M = 2, 4** (up to `lock_scale`). For
**M = 8 it is *not* literally `Re(z⁸)`**: carrying the ½-scaled imaginary arm up
one more level gives `Re(z⁴)² − ¼·Im(z⁴)²` instead of `Re(z⁸) = Re(z⁴)² − Im(z⁴)²`. The two coincide at lock (`Im(z⁴) → 0` → both peak), so it remains a
faithful, monotone lock detector — it is simply not the literal 8th-power real
part. Making it exact would require doubling the carried imaginary term, which
would break the constant-gain property above, so for a thresholded handover
detector the form as written is the right trade.

### 2.4 Opt-in auto-handover

Handover from the NDA acquisition discriminator to the decision-directed tracker
is **opt-in** (a config flag, default off → the loop stays in NDA acquisition
mode unless enabled). When enabled, it is **automatic on lock**: once
`lock_signal` holds above a threshold (timing also settled), the loop switches
the NCO's error source from the NDA discriminator to the decision-directed
`Im(y·conj(â))/|y|`. The shared NCO + loop filter state carries across the
switch (no frequency/phase discontinuity); only the error source changes.

______________________________________________________________________

## 3. The new NDA carrier-loop primitive (reusable)

The NDA M-th-power carrier loop is a **standalone reusable C primitive** (not
buried in the receiver) — a complete non-data-aided carrier-recovery loop usable
on its own for any M-PSK / unmodulated carrier:

- **Owns** an integer `lo` NCO + a `loop_filter` (by value), the I/Q arm I&D
    accumulator, and the M-th-power discriminator + lock signal.
- **Per sample:** wipe-off (inline `*_wipeoff`), accumulate into the arm I&D;
    every `sps/N` samples dump the arm, run the discriminator, filter, steer the
    NCO. Inline composition API (`*_wipeoff` / `*_arm_step`) mirrors `lo_step` /
    `dll_accumulate` / `symsync_step`.
- **Exposes** `norm_freq`, `lock_signal`/lock metric, `m`, `n` (arm dumps/symbol),
    loop `bn`/`zeta` — and its NCO so a composing receiver can drive the **same**
    NCO with a decision-directed error on handover.
- **Config:** `m` (2/4/8), `sps`, `n` (default 4), `bn`, `zeta`, seed
    `init_norm_freq`. All params default + keyword-capable (no forced positionals).

Working name **`track.CarrierNda`** (non-data-aided). Naming review: there are
now three carrier loops — `Costas` (BPSK decision-directed), `CarrierMpsk`
(M-PSK decision-directed), `CarrierNda` (M-PSK non-data-aided). This revives the
earlier `track.Carrier.*` namespace idea; deferred (the jm-owned `__init__.py`
makes a nested namespace a drift/`.so`-is-API concern — see
[so-is-the-api]) — flat names for now.

`MpskReceiver` then embeds `CarrierNda` as its wipe-off NCO + acquisition loop,
and applies the decision-directed update (the `CarrierMpsk` discriminator math)
to the same NCO once handover engages.

______________________________________________________________________

## 4. Matched filter (I&D default, RRC opt-in)

A per-sample FIR feeding `symsync`:

- **I&D / boxcar (default):** unit-gain length-`sps` moving average — the matched
    filter for a rectangular NRZ symbol pulse (and the natural front for a despread
    chip stream).
- **RRC (opt-in):** `rrc_taps(beta, sps, span)` — matched to an RRC-shaped
    transmitter.

`fir_core.h` currently exposes only block `fir_execute`; the per-sample inline
loop needs a single-sample `fir_step` (additive C composition API, mirrors the
other `*_step`s; reuses the existing delay line, no struct change). The MF FIR is
owned by pointer (variable-length taps), like `channel`'s code copy.

> **AGC dropped.** For PSK the nearest-point slice and the `|y|`-normalized
> discriminator are both amplitude-invariant, so an AGC adds a component and a
> param for no functional gain — omitted (no abstraction beyond the task).

______________________________________________________________________

## 5. Symbol timing

Reuse `track.SymbolSync` (Gardner TED + Farrow, shipped) **as-is** — timing is
modulation-agnostic (`|·|²`). It needs a by-value `symsync_init` (additive;
`symsync_state_t` is already fully by-value — `nco`/`farrow`/`loop_filter` — so
this is just an in-place init mirroring `costas_init`/`dll_init`). The receiver
embeds `symsync_state_t` and drives it with the inline `symsync_step`.

______________________________________________________________________

## 6. Component reuse

| Piece                                                               | Verdict                                       |
| ------------------------------------------------------------------- | --------------------------------------------- |
| `lo` integer NCO + `lo_step`/`lo_init`                              | carrier wipe-off — as-is                      |
| `loop_filter` PI                                                    | every loop embeds it by value — as-is         |
| `CarrierMpsk` decision-directed discriminator                       | tracking-path math — reuse the update         |
| `SymbolSync` (Gardner + Farrow)                                     | timing — as-is (+ add `symsync_init`)         |
| `rrc_taps` + a per-sample FIR                                       | matched filter — reuse (+ add `fir_step`)     |
| `mpsk` slicer/demap (`mpsk_slice`, `mpsk_demap`, `mpsk_diff_demap`) | decision + bits + differential — as-is        |
| **NDA M-th-power carrier loop**                                     | **NEW reusable primitive** (§3)               |
| `Dll(segments)`                                                     | optional DSSS front-end — pipeline, not fused |

______________________________________________________________________

## 7. Build plan (sequential, each rock-solid first)

1. **`track.CarrierNda`** — the NDA M-th-power carrier loop primitive (§3).
    Validate: open-loop S-curve `phase_error(φ)` = the period-`2π/M` sawtooth per
    M; `lock_signal` vs phase/SNR; cold-start frequency pull-in on an *unmodulated*
    carrier and on modulated data with **no timing**; jitter vs bn. Gallery.
1. **`fir_step` + `symsync_init`** — the additive inline composition APIs (tiny,
    byte-identical to the block paths; their own parity tests).
1. **`track.MpskReceiver`** — the composition (§1). Validate end-to-end BER vs
    Es/N0 per M within ~1–2 dB of the MPSK bound, with a carrier offset + timing
    offset + pulse shaping; opt-in auto-handover engages and holds; I&D and RRC
    modes; reset-reproducible; block-size invariant (independent output per call,
    the gh-219 rule). DSSS-MPSK example chaining `Dll(segments) → MpskReceiver`.
    Gallery: constellation pull-in (cloud → M clusters), carrier + timing locks,
    BER table.

______________________________________________________________________

## 8. Open review points

- **NDA discriminator form** — confirmed M-th-power via repeated squaring (the
    canonical block in §2.3); `lock_scale` = 1 / 0.619 / 0.412 for M = 2 / 4 / 8.
- **Naming** — `CarrierNda` / flat vs a `Carrier.*` namespace (deferred).
- **Handover threshold** — on `lock_signal` (+ timing-settled gate); tune in
    Step 3 validation.
- **N default 4** — arm I&D dumps per symbol; revisit if pull-in/jitter trade
    argues otherwise.
