# MPSK Receiver

**Status:** implemented — `track.CarrierNda` (#276) + `track.MpskReceiver`
shipped; the NDA carrier loop reworked to a raw M-th-power discriminator + AGC
(§2.3); **the receiver's engine rebuilt on the matched DDC cascade (§1.1)**, and
a real-input twin `track.MpskReceiverR` added alongside it.
**Scope:** a streaming **M-PSK receiver** (`track.MpskReceiver` for complex
baseband, `track.MpskReceiverR` for a real IF, M = BPSK / QPSK / 8PSK) that
demodulates pulse-shaped signals by **composing existing `doppler.track` and
`doppler.resample` primitives**. C-first: every block below is a C core; the
Python face is the jm-generated thin wrapper. This is the architecture, the
carrier-recovery design (the part most easily miscommunicated), and the record
of how the engine changed.

Related: [carrier loop theory](../gallery/carrier-mpsk.md) (the decision-directed
`CarrierMpsk` loop, already shipped), [RateSync](../gallery/ratesync.md) (whose
timing loop this receiver now literally reuses), the
[matched rate converter](../gallery/rate-converter.md), the async DSSS
despreader ([design](async-symbol-despreader.md)) — DSSS-MPSK is the pipeline
`Dll(segments) → MpskReceiver`, not a fused object.

______________________________________________________________________

## 1. Architecture — composition, not machinery

The receiver owns **no filter, no NCO and no interpolator of its own**. It is a
matched down-converter with two loops closed around its two control ports:

```mermaid
flowchart TB
    IN["rx (cf32 baseband, or f32 real IF)"]
    subgraph RX["MpskReceiver — a matched DDC and two loops"]
        direction TB
        FE["MatchedDDC / MatchedDdcr<br/>LO mix · CIC/HB cascade ·<br/>terminal polyphase stage<br/>(the bank IS the matched filter,<br/>the arm IS the fractional delay)"]
        STROBE{"m_out outputs/symbol<br/>on-time strobe? gate?"}
        TED["Gardner TED<br/>(carrier-blind, |·|²)"]
        NDA["NDA M-th-power disc<br/>z² → z⁴ → z⁸<br/>→ phase_error + lock"]
        DD["decision-directed disc<br/>e = Im(y·conj â)/|y|"]
        SEL{"carrier error<br/>opt-in handover on lock"}
        TLF["timing loop filter (PI)"]
        CLF["carrier loop filter (PI)"]
        FE --> STROBE
        STROBE -->|"every output"| TED
        STROBE -->|"strobe only"| NDA
        STROBE -->|"strobe → y_k"| DD
        TED --> TLF
        NDA -->|"acquisition"| SEL
        DD -->|"tracking"| SEL
        SEL --> CLF
        TLF -.->|"rate_ctrl"| FE
        CLF -.->|"freq_ctrl"| FE
    end
    IN --> FE
    STROBE -->|"y_k"| OUT["steps() → cf32 y_k"]
    STROBE --> BITS["bits() → Gray bits<br/>(opt-in differential)"]
```

- **The matched filter is the cascade's terminal polyphase stage.** It is not a
    separate FIR: the bank the down-converter was already evaluating carries the
    pulse-matched taps, so the match costs the dot product it was doing anyway.
- **The interpolator is the bank arm.** Selecting arm `p` of `P` *is* the
    fractional symbol-timing delay, to `1/num_phases` of an output period. There
    is no Farrow and no second timing mechanism — see the "NCO alone controls
    timing" rule.
- **Two control ports, one per loop, and they are duals:** the timing loop
    steers the terminal accumulator (`rate_ctrl`), the carrier loop steers the LO
    phase accumulator (`freq_ctrl`).
- **The timing loop is `ratesync_loop_t` — literally
    [RateSync](../gallery/ratesync.md)'s loop**, factored out for reuse, not a
    copy of it. Timing is carrier-blind (Gardner `|·|²`), so it settles in
    parallel with carrier acquisition and can lead it.
- **`m_out` outputs per symbol** come out of the terminal stage. Gardner takes
    every `m_out`-th as the **on-time strobe** and the one `m_out/2` back as the
    transition gate, so the oversampled matched-filtered stream falls out free.
- **DSSS-MPSK** is still the downstream pipeline `Dll(segments) → MpskReceiver`;
    the despreader removes the PN code and hands symbols to this modem. Not fused.

### 1.1 Why the engine changed — and what it cost

The original build (§§3–5 below, kept as the record) put a dense matched-filter
FIR and a `symsync` Gardner+Farrow loop downstream of a per-sample wipe-off, with
a *separate* boxcar "arm" feeding the NDA discriminator. That works, but it pays
for the same samples several times and it does not scale in `sps`: a single-stage
matched filter at `sps = 256` needs ~4225 taps per arm.

Rebuilding on the matched DDC replaces all four pieces with one cascade:

| Was                               | Is now                                      |
| --------------------------------- | ------------------------------------------- |
| per-sample integer-NCO wipe-off   | the DDC's LO, driven by `freq_ctrl`         |
| separate boxcar arm + its own AGC | the terminal stage's own outputs            |
| dense matched-filter FIR          | the terminal polyphase stage's bank         |
| `symsync` Gardner + Farrow        | `ratesync_loop_t` on `rate_ctrl`, bank arms |

The payoff is that `sps` becomes a **double** and the front end plans itself: at
`sps = 8` the plan is a halfband or two plus a terminal stage; at `sps = 256` it
is a CIC in front of the *same* terminal stage. The matched filter costs ~34
taps/arm at both ends of a 64× span of input rates. An irrational `sps` — a
free-running ADC clock against the symbol clock — is no harder than an integer
one, because the terminal accumulator is a double and the loop only has to steer
the strobe.

!!! warning "This is a compatibility break, deliberately taken"

    **Outputs are no longer bit-identical to releases before the rebuild.** The
    matched filter became a polyphase bank instead of a dense FIR and the
    interpolator became a bank arm instead of a Farrow, so recovered symbols move
    at the float level. Detection performance is unchanged — the fused matched
    filter measures on the Es/N0 bound — but exact-output pins are not.

    **`bn_carrier` also changed units.** It is now normalised to the **symbol
    rate**, like `bn_timing`, rather than to the input sample rate. At the old
    default `sps = 8` the same number is now an 8× wider loop, and the usable
    range is correspondingly narrower: values around `0.005`–`0.01` are the
    working band where `0.02`–`0.03` used to be. A carrier loop here closes
    around the matched filter, so its dead time is that filter's group delay —
    keeping `bn_carrier` a small fraction of the symbol rate is what a real
    receiver does anyway, and the new normalisation makes that the natural
    reading of the number.

### 1.2 Two types, not one — and why

`MpskReceiver` takes complex baseband; `MpskReceiverR` takes a real IF. They are
**separate types rather than one type with a flavor**, by the same rule the
down-converters follow: a difference in *constructor* is a flavor (a jm view), a
difference in *method signature* is a separate type. `steps(x)` takes `cf32` on
one and `f32` on the other, so a shared view is not available — one class would
have to name the dtype in a method name.

Everything behind the front end is genuinely shared, not duplicated:
`mpsk_rx_loops_t` (both loops, both discriminators, the handover, the demapper)
is one implementation that both types embed. `MpskReceiverR` adds only the front
end (`MatchedDdcr` instead of `MatchedDDC`) and the two rate conversions its
halfband forces — its LO runs at the *intermediate* rate `fs/2`, and the R2C
halfband has an `fs/4` shift baked in, so tuning a real tone at input-normalised
`f_c` to DC is `norm_freq = -(2·f_c + 0.5)`, and a frequency readback at the
intermediate rate is **half** as many cycles/sample at the input rate. That also
sets its one extra constraint: `sps > 2·m_out`, because the cascade behind the
halfband runs at twice the overall rate.

______________________________________________________________________

## 2. Carrier recovery — the design that's easy to get wrong

The rule is **predetection de-rotation, postdetection discrimination**, with a
twist for cold start.

### 2.1 De-rotation is per-sample, always

The LO wipe-off runs on **every input sample, before anything else**. A residual
carrier rotating across an integration window costs sinc energy; the window here
is the matched filter (short for I&D ≈ `sps`, long for RRC ≈ `2·span·sps+1`), so
per-sample de-rotation is the general-purpose "just works" placement. It costs
more compute than de-rotating symbols, and that is the accepted trade — it is
correct for every mode (I&D, RRC, large residual, and the DSSS front-end)
without special-casing.

Since the rebuild this holds **structurally rather than by convention**: the LO
is the first stage of the front end, so predetection de-rotation is where the
signal path puts it, not something the receiver has to remember to do first.

!!! danger "The sign convention differs between the two sides"

    The DDC mixes `x · lo_step_ctrl(...)` while `carrier_nda_disc` consumes
    `x · conj(...)`. The loop therefore **negates** the filtered error before it
    reaches `freq_ctrl`. Get this wrong and the failure does not look like a sign
    error: timing, rate and symbol count all stay perfect, and the lock metric
    sits at a steady *negative* value (−0.48 for QPSK where +0.62 is real lock)
    with every symbol parked on a decision boundary. Read the lock metric's
    **sign**, not just its magnitude.

### 2.2 Two discriminators share one NCO + loop filter

A decision-directed loop alone cannot cold-start: it needs symbol decisions,
which need timing lock and (for the error to mean anything) data. Many real
links must **acquire the carrier with no symbol timing and no data present**
(e.g. a bare/unmodulated carrier, or before timing settles). So the carrier loop
has **two error sources into one NCO**:

1. **Acquisition — non-data-aided (NDA) M-th-power discriminator** on the
    matched-filtered **on-time strobe**. It strips the M-PSK modulation by
    raising the strobe to the Mth power, so it is independent of *data*.

1. **Tracking — decision-directed** `e = Im(y·conj(â))/|y|` on the full-SNR
    recovered symbol `y_k` (the `CarrierMpsk` discriminator, already shipped and
    validated). Low jitter once timing + lock are established.

Both now run at the **symbol rate**, on the same strobe, which is why
`bn_carrier` is symbol-rate normalised (§1.1) and why the handover is a plain
discriminator swap with no rate change to reconcile.

!!! info "Carrier acquisition and symbol timing — the coupling, and the way out"

    The original design's headline property was that the NDA path acquires with
    no data **and no symbol timing** — true of the old free-running boxcar arm,
    which was timing-independent by construction. Reading the on-time strobe
    gave that up: until the timing loop converges the strobe is not a symbol, so
    its M-th power is nothing in particular.

    Measured, QPSK at `sps = 8` from a cold start: the timing loop takes ~130
    symbols to declare, and throughout that window the carrier lock statistic
    reads **0.9 → 1.7 against a QPSK ceiling of 0.62** — the unambiguous sign
    that the discriminator's input is not a valid constellation. A type-2 loop
    steering on that integrates a bias for 130 symbols; whichever way it was
    pushed decided the run, and pushed past the M-fold boundary the integrator
    simply held it there. About a third of data seeds never recovered, and
    **widening `bn_carrier` made it worse** — more garbage integrated over the
    same fixed transient — which is how you can tell it was a transient problem
    and not a pull-in-range one.

    Two things were tried, and only one of them survives:

    - **The other taps do not have the problem at all.** `mf_all` and `lo_arm`
        (§2.2.1) are timing-independent by construction. This is the answer: the
        tap point is a declared, caller-visible choice, and a link whose carrier
        must acquire before its timing does should not be reading the strobe.

    - **Gating the strobe tap on timing lock — tried, measured, removed.** The
        steer, the AGC seed and the handover were gated on the timing loop's own
        `lockdet` (which declares at a data-independent symbol 132). At the
        operating point that motivated it — `sps=8`, `m_out=4` — it worked:
        cold acquisition went from a coin flip to 6/6 seeds and the pull-in
        curve became monotone in `bn_carrier` again.

        It is no longer the default, for three reasons. Across a 24-cell sweep
        (sps × `m_out` × `bn_carrier`) removing it changed **exactly one cell**,
        `sps=8, m_out=4, bn_carrier=0.04`, which went to 5/24 — every other
        cell is identical gated or not. `m_out` now defaults to 8, so that cell
        is off the default path. And what the gate mainly bought was
        *measurability*: with the steer frozen until timing declares, the
        carrier transient starts at a known instant, which is convenient for
        instrumenting an acquisition and is not a property of a working
        receiver.

        The deeper objection is structural. A tap that needs timing it cannot
        wait for is a reason to choose a different tap; resolving that inside
        the receiver hid a real trade behind a coupling the caller could
        neither see nor override, and made the default receiver's cold-start
        behaviour depend on a second loop's lock detector. If cold acquisition
        fails at `m_out=4`, reach for `nda_tap="mf_all"` or `"lo_arm"`.

    The AGC seed mattered as much as the steer: seeded off a pre-lock strobe the
    gain latches on a non-symbol and can land far too **low** as easily as too
    high — measured `lock` = 4.9e-19, a denormal, on a receiver decoding every
    bit correctly, so the handover never fired.

### 2.2.1 `nda_tap` — the discriminator's tap point IS the pull-in range

An M-th-power discriminator updating at rate `F` is unambiguous only while its
M-th-power phase advances less than π per update, `|M·Δf| < F/2`. So *where* the
discriminator reads decides how much frequency error it can even see, and that
is a real design axis rather than an implementation detail — which is why it is
a construction parameter, `nda_tap`, and not a hidden policy.

| `nda_tap`          | Reads                                  | Update rate | Unambiguous `\|Δf\|` | Needs timing? |
| ------------------ | -------------------------------------- | ----------- | -------------------- | ------------- |
| `strobe` (default) | the on-time strobe                     | `Rs`        | `Rs/(2M)`            | **yes**       |
| `mf_all`           | every terminal output                  | `m_out·Rs`  | `m_out·Rs/(2M)`      | no            |
| `lo_arm`           | post-LO, through a free-running boxcar | LO rate     | `f_lo/(2M)`          | no            |

Measured, QPSK at `sps = 8` — the largest unaided frequency error each tap can
still acquire, at its own best `bn_carrier`. `m_out` is on this axis, so the
default's move from 4 to 8 (§4, the coherent-bound default) moves two of the
three numbers:

| tap      | `m_out = 4` | `m_out = 8` (default) |
| -------- | ----------- | --------------------- |
| `strobe` | `0.010·Rs`  | **`0.050·Rs`**        |
| `mf_all` | `0.015·Rs`  | `0.033·Rs`            |
| `lo_arm` | `0.090·Rs`  | `0.090·Rs`            |

`lo_arm` is the one number `m_out` cannot move, and that is the check on the
mechanism rather than a curiosity: it taps *ahead* of the cascade, so the
terminal rate is not in its path at all. At `m_out = 4` it is **9× the
strobe**, near the `sps` factor the theory predicts (`fs/Rs = 8` here) — that
tap reaches its own Nyquist bound rather than stalling at whatever the loop
bandwidth allows. At `m_out = 8` the strobe closes most of that gap *without
its update rate changing at all*: a sharper matched filter is a quieter
discriminator, which is what raises the largest stable `bn_carrier` (0.01 →
0.05), and an unaided strobe's pull-in follows the loop bandwidth, not its
Nyquist bound. It is the same coupling from the other side — the reason a
too-wide `bn_carrier` used to *lose* acquisitions (gh#536).

!!! note "`bn_carrier` means the same thing at every tap"

    It stays normalised to the **symbol rate**, so one setting is one loop. What
    the tap changes is the filter's *update period*; that does not widen the loop
    by itself, it widens what the discriminator can see and improves the
    stability margin — which is what then lets you raise `bn_carrier` on purpose.
    At a *fixed* `bn_carrier` all three taps measure the same `0.01·Rs`, exactly
    as they should. The table above is "each at its own best `bn`".

!!! warning "`mf_all` needs `m_out = 8`; the gain collapse lands on it, not on `lo_arm`"

    §2.3's `Σ g_k^M` gain-collapse result — the raw M-th-power coherent gain over a
    set of taps, made fatal by M = 8 — applies to the tap that sums the M-th power
    over **all `m_out` matched-filter outputs**, badly-timed ones included. Measured
    at Es/N0 20 dB, `sps = 8`, `bn_carrier = 0.005`, median over 5 seeds, both with
    and without `acq_to_track`:

    | tap      | `m_out = 8` (default), M ∈ {2,4,8} | `m_out = 4`, QPSK | `m_out = 4`, 8PSK       |
    | -------- | ---------------------------------- | ----------------- | ----------------------- |
    | `strobe` | SER 0, EVM −19.7 dB                | SER 0, −15.9 dB   | SER 0.002, −15.9 dB     |
    | `mf_all` | SER 0, EVM −19.7 dB                | SER 0, −16.0 dB   | **SER 0.851, −11.9 dB** |
    | `lo_arm` | SER 0, EVM −19.7 dB                | SER 0, −16.0 dB   | SER 0.001, −16.0 dB     |

    **At the default `m_out = 8` every tap decodes every order cleanly**, so the
    warning is conditional on `m_out`, not on the tap alone. At `m_out = 4` the arms
    cover half as much of each symbol, and at 8th power that is enough to put
    `mf_all` at chance. `lo_arm` shows no failure at any setting measured here.

    `mf_all`/8PSK at `m_out = 4` used to fail *while reporting a healthy lock*
    (+0.94, and +3.90 at `bn_carrier = 0.05`) — a false lock. Since the lock
    statistic was limited it reports **−0.069** on the same failure, i.e. it now
    correctly says "not locked" on a receiver that is not decoding, and the two
    downstream consequences went with it: `mf_all` + `acq_to_track` at *QPSK*
    recovered from 2/5 decodes (SER 0.295) to **5/5** (SER 0.0000), because the
    handover no longer fires off a bogus statistic; and the `bn_carrier = 0.05`
    failures read +0.175 / +0.045 instead of +3.90. The decode failure itself is
    unchanged — that is the `Σ g_k^M` collapse, not a detector problem.

    `lo_arm` + `acq_to_track` costs EVM without costing SER (−19.7 → −15.4 dB at
    `m_out = 8`), which is the decision-directed error running on a
    non-matched-filtered arm.

    **Correction.** This box named `lo_arm` as the failing tap until 2026-07-27. Its
    numbers came from a lag search clipped to ±30 and an SER window that started
    inside the settling transient — the two defects fixed in `30c76c6d`; both report
    chance SER on a decode that is in fact error-free.

!!! danger "Stable false lock at `Δf = k·Rs/M`, whatever the tap"

    `Rs/M` is exactly where a symbol-rate M-th power aliases onto zero, so the
    **M-fold ambiguity is a frequency ambiguity as well as a phase one**.
    Measured on QPSK at `sps = 8` with an initial error of `Rs/4`: the loop never
    moves, ending precisely where it started (tracked frequency 2e-6 against a true
    0.03125), and reports a lock statistic of **+0.83** against the ≈ 1.0 a real
    lock reads.

    Nothing self-referenced detects this. The constellation is stationary, so a
    self-referenced EVM looks clean and blind M2M4 looks clean; the lock metric
    looks healthy. It takes an **external** frequency reference, or a sync word /
    known preamble. A faster tap moves the alias out proportionally (`F/M`, not
    `Rs/M`), which is another reason the wide taps are worth having.

If you need more range than any tap gives, the answer is a **coarse frequency
estimate ahead of the loop** — an FFT sweep, or the `ppe` 2-D rate×freq
estimator — handed in via `init_norm_freq`. That is what the parameter is for,
and no loop bandwidth substitutes for it.

### 2.3 The NDA discriminator + lock signal (canonical definition)

The M-th-power detector is computed efficiently by **repeated complex squaring**
of the arm sample `z = i + jq`: `z²` strips BPSK, `z⁴` strips QPSK, `z⁸` strips
8PSK. Each squaring level yields both a phase error and a lock signal; the
phase-error scale normalizes the discriminator gain so one `bn` behaves
identically across M, and the lock signal is left **unscaled** so that it reads
~1.0 at lock for every M -- which is what actually makes the handover threshold
M-independent.

**Normalization — an AGC, not a per-sample limiter.** The discriminator input
`z` is driven to unit average power by an embedded log-domain AGC (`agc_core`)
before the discriminator, with a 10 dB square clip on the AGC output. This is
deliberate. The discriminator is the **raw** M-th-power form `Im(z^M)` (the
"conventional Costas" / linear-arm form), which is optimal for a
constant-modulus signal — the DSSS target. A per-sample unit-magnitude
normalization `z/|z|` is Yuen's "polarity-type" hard limiter, the *worst*
nonlinearity (≈2.5–4 dB extra squaring loss, and non-monotone in SNR). The AGC
provides amplitude invariance (so the loop gain does not scale with input level)
*without* the limiter's penalty; the clip bounds the peak (constructive-ISI)
samples that would otherwise dominate the `|z|^M` weighting. **Input average
power is required to be at or below unity** — the normal convention for
captured/scaled baseband (and the DSSS despreader's known correlation gain).

!!! warning "Since the rebuild the AGC is seeded, and its bandwidth is absolute"

    Two things about this AGC changed with the engine, and both were real bugs
    before they were fixed:

    - **It is seeded from the first strobe.** The bank is unit-*energy*, so its
        output sits roughly `√(pulse_sps)` above the constellation the AGC expects.
        Left to converge from a cold gain, the loop ran 16× hot at QPSK and 256× at
        8PSK — the measured lock statistic reached 5.2 and 26.4 (in the units of the
        time, against ceilings of 0.62 and 0.41), which was the fingerprint to
        recognise: a lock metric far above a real lock's value meant a gain fault,
        not a good lock.

        **That fingerprint no longer exists, and this is what limiting the lock
        signal cost.** `Re((z/|z|)^M)` is amplitude-blind by construction and
        bounded in ±1, so a hot AGC cannot inflate it and a reading of 5.2 is now
        unreachable. The diagnostic was real and is gone; the bug it pointed at is
        fixed and pinned by the seeding test, so nothing regresses silently, but a
        *future* gain fault will have to be caught by the AGC's own gain rather
        than by this statistic. Retires the "above-ceiling lock statistic is a free
        diagnostic" idea in §6.

    - **Its bandwidth is `MPSK_RX_AGC_BW = 0.002` outright**, not `0.01·bn`. The
        old proportional rule was a *per-sample* convention; on a discriminator now
        running at the symbol rate it spans thousands of symbols and never settles.

    The AGC still absorbs residual/slow variation only: a cold input >~10 dB above
    unity remains out of spec (clip on → false lock, clip off → `|z|^M` gain
    blow-up).

```python
# osr = sample_rate // symbol_rate   # input oversampling, typ. 4
# The arm is a free-running half-symbol boxcar moving average (no rate
# conversion); i, q are its per-sample outputs (one per input sample),
# AGC-normalized to unit average power.  esno = symbol energy-to-noise-density
# ratio, dB.
#
# Squaring loss S_L (Lindsey-Simon / Yuen): the SNR penalty of the M-th-power
# nonlinearity; S_L <= 1 always, so sq_loss_dB <= 0.  Measured from the loop it
# is  slope**2 / var(phase_error) / rd  using the ACTUAL S-curve slope at lock
# (= 2 only for a constant-modulus arm; it collapses on a pulse-shaped arm,
# see below) — NOT a hardcoded 4.

import numpy as np

mod = "BPSK"                       # one of "BPSK", "QPSK", "8PSK"
esno = 10.0                        # symbol Es/No, dB
i = np.array([0.71, -0.62, 0.80])  # example half-symbol boxcar arm outputs
q = np.array([0.10, -0.21, 0.05])  # (unit-power, one pair per input sample)

rd = 10 ** (esno / 10.0)

bpsk_lock = i**2 - q**2          # Re(z^2)
bpsk_phase_error = 2 * i * q     # Im(z^2)

if mod == "BPSK":
    phase_error = bpsk_phase_error
    lock_signal = bpsk_lock                                           # Re(z^2)
    # Yuen Eq. 8-19 (passive arm-filter Costas loop), half-symbol boxcar arm.
    # Verified moments: K2 = 5/6, K4 = 23/30, KL = 2/3, Bi/R = 2 (z = Bi/R/2 = 1).
    K2, K4, KL, z = 5 / 6, 23 / 30, 2 / 3, 1
    S_L = K2**2 / (K4 + KL * z / rd)   # high-SNR floor K2**2 / K4 = 0.906 = -0.43 dB
    sq_loss_dB = 10 * np.log10(S_L)
elif mod == "QPSK":
    phase_error = bpsk_phase_error * bpsk_lock                        # ~ Im(z^4)
    lock_signal = bpsk_lock**2 - bpsk_phase_error**2                  # ~ Re(z^4)
    # No clean closed form for the M-th-power passive loop; empirical fit (dB).
    sq_loss_dB = -0.0564724 * esno**2 + 1.90284531 * esno - 15.65792221
else:  # 8PSK
    qpsk_phase_error = bpsk_phase_error * bpsk_lock
    qpsk_lock = bpsk_lock**2 - bpsk_phase_error**2
    phase_error = qpsk_phase_error * qpsk_lock                        # ~ Im(z^8)
    # The 4 is load-bearing: qpsk_phase_error is HALF of Im(z^4), so squaring it
    # needs the 2 squared back for this to be Re(z^8). Without it the statistic
    # is Re(z^4)**2 - Im(z^4)**2/4 -- equal to Re(z^8) at phi=0 and nowhere else,
    # and biased positive on noise where Re(z^8) is zero-mean.
    # NB the shipped lock signal LIMITS first (divides by |z|^M) so its H0
    # variance is 1/2 at every M; see "the lock statistic" below. Only the
    # phase error keeps the raw |z|^M weighting.
    lock_signal = qpsk_lock**2 - 4 * qpsk_phase_error**2              # Re(z^8)
    sq_loss_dB = -0.14285557 * esno**2 + 5.70706958 * esno - 58.13670891

# Coherent (data x squaring) gain of the free-running half-symbol boxcar arm:
#   Re E[z_d^M] = 1/2 + 1/(M+1)   ->  M=2: 5/6,  M=4: 7/10,  M=8: 11/18.
# For BPSK this is exactly K2 = 5/6;  mod_loss_dB = 10 * log10(1 / K2).
mod_loss_dB = 10 * np.log10(1 / (5 / 6))
```

- `phase_error` ≈ `Im(z^M)` (scaled) — a sawtooth S-curve of period `2π/M`, the
    M-fold phase ambiguity (consistent with the `CarrierMpsk` S-curve). It steers
    the NCO; the M-fold ambiguity is resolved downstream (differential demap or a
    sync word), same as the decision-directed loop.
- `lock_signal` = `Re((z/|z|)^M)` — the M-th power of a **limited** sample: ≈ 1
    when phase-locked, zero-mean with no carrier, bounded in ±1, and with an H0
    variance of ½ at **every** M. It is **the lock metric that drives handover**
    and the receiver's carrier lock indicator; that M-independence is what makes
    one threshold one Pfa (see "Limiting" below).

The squaring-loss/noise behaviour worsens with M (each squaring multiplies
noise), so the NDA loop is the *acquisition* aid; decision-directed tracking
gives the low-jitter steady state.

**Gain collapse on a pulse-shaped arm — the loop still locks.** The raw
M-th-power coherent gain is `Σ_k g_k^M` over the arm/pulse taps `g`. For a
constant-modulus arm this yields the S-curve slope 2; for a pulse-shaped (e.g.
pre-matched-filter RRC) arm `Σ g_k⁴` is minuscule and the slope collapses ~80×.
Because the loop filter is **type-2 (PI)**, the steady-state frequency error is
still driven to zero — the loop **locks on RRC as well as constant-modulus**
(`native/validation/carrier_nda_step_response.c` validates both) — only pull-in
is slower and jitter higher. The pulse-shaped *symbol* error rate is then set by
the downstream matched filter + timing, not by the carrier loop.

#### Derivation — the recursion *is* the M-th-power loop

Write the arm sample `z = i + jq`. The first level is literally `z²`:

```
bpsk_lock        = i² − q² = Re(z²)
bpsk_phase_error = 2iq     = Im(z²)
```

Each subsequent level squares the running pair and reads off its real/imaginary
parts, so `(lock, phase_error)` climbs the powers `z² → z⁴ → z⁸`. Verified
exactly (residual 0 over a full phase sweep):

| M   | `phase_error` | `lock_signal` (before limiting) |
| --- | ------------- | ------------------------------- |
| 2   | `Im(z²)`      | `Re(z²)`                        |
| 4   | `½·Im(z⁴)`    | `Re(z⁴)`                        |
| 8   | `¼·Im(z⁸)`    | `Re(z⁴)² − 4·(½·Im(z⁴))²`       |

So **`phase_error` is exactly the M-th-power discriminator** `Im(z^M)`, scaled by
`1, ½, ¼`. That scale is not arbitrary — it **normalizes the phase-detector gain
across M**. The S-curve slope at lock is `(slope of Im(z^M)) × scale = M × scale = 2·1, 4·½, 8·¼ = 2` for every M, so one loop-filter `bn` behaves identically for
BPSK / QPSK / 8PSK. (This is why the recursion carries `ab = Im(z⁴)/2` rather
than the full `2ab` into the next squaring.)

The **`lock_signal` is `Re(z^M)` at every M**, and the recursion's ½ has to be
undone to get there. Because the carried imaginary term is `qe = ½·Im(z⁴)`, the
8th-power real part is `Re(z⁸) = Re(z⁴)² − Im(z⁴)² = ql² − (2·qe)²`, so the lock
expression needs the **factor of 4** written out explicitly.

!!! bug "This was wrong until 2026-07-27, and the reasoning that kept it wrong"

    The lock signal read `ql² − qe²` — i.e. `Re(z⁴)² − ¼·Im(z⁴)²` — and this
    section argued the shortfall was an acceptable trade: *"the two coincide at
    lock, so it remains a faithful monotone lock detector; making it exact would
    require doubling the carried imaginary term, which would break the
    constant-gain property."*

    The premise is false. Making it exact requires multiplying by 4 **inside the
    lock expression**, which does not touch the carried term and so cannot affect
    the phase-detector gain at all. The two quantities are computed on separate
    lines from the same pair; there was never a trade to make.

    And "coincides at lock" was the wrong test. Both forms are exactly +1.0000 at
    `φ = 0`, so every locked measurement agreed and the error was invisible where
    anyone looked. It differed everywhere *else* — including on noise, where
    `Re(z⁸)` is zero-mean but `Re(z⁴)² − ¼·Im(z⁴)²` is not, since
    `E[Re(z⁴)²] = E[Im(z⁴)²]` leaves a positive residual of `¾·E[Im(z⁴)²]`.
    Measured on unit-power complex Gaussian noise: mean **+8.94** instead of
    **−0.11**. A lock detector is thresholded against its noise-only
    distribution, so the one region the form was wrong in was the only region
    that sets its false-alarm rate. `carrier_nda_scurve.c` had encoded the
    conclusion as `if (m <= 4)` around its `|lk − Re(z^M)| < 1e-6` check, so the
    validator was excused from the one order that was broken.

#### Limiting — what makes the threshold a Pfa

`Re(z^M)` on a raw sample is unbounded and its noise variance grows with M: at
M = 8, `|z|⁸` on Gaussian noise gives it an sd of **137 per look** against a
value of 1.0 at lock. The shipped lock signal therefore **limits first**:

```text
lock_signal = Re((z / |z|)^M)
```

Under H0 the phase is uniform, so `Var[Re(e^{jMθ})] = ½` for **every** M — the
statistic is bounded in ±1 and its H0 law is M-independent, which is precisely
what lets a single `lock_thresh` mean a single false-alarm probability at every
constellation order. With `α = det_ema_alpha(0, 15.9) = 0.05` (`N_eff = 39`
looks), `σ_H0 = sqrt(½·α/(2−α)) = 0.1132` analytically and 0.1132 measured, so
the default threshold of 0.5 is 4.42 σ — a per-look Pfa of 5e-6. Measured
end to end with `acq_to_track=1`, 100 noise-only runs × 20 000 symbols:
**0/100 false declares at every M**.

This costs H1 — limiting discards the `|z|^M` weighting that helps at low SNR —
and is still a large net win at every order, because H0's variance falls by much
more than H1's mean does. Detectability `d' = (μ_H1 − μ_H0)/σ_H0` at
Es/N0 = 10 / 20 dB, raw → limited: BPSK 5.70/6.21 → 7.95/8.75, QPSK
1.50/1.78 → 5.81/8.47, 8PSK 0.02/0.04 → 1.76/7.52. With the raw form only BPSK
ever cleared a 1e-3 Pfa, so for M ≥ 4 there was no Pfa-derived threshold
available at all.

Only the **lock** path is limited. The `phase_error` keeps its raw `|z|^M`
weighting, which is the natural matched weighting on a pulse-shaped signal and
must not be flattened (see the AGC note in §2.3).

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

- **Owns** an integer `lo` NCO + a `loop_filter` (by value), the I/Q arm
    **boxcar moving average** (embedded `boxcar` primitive) + its **per-sample
    AGC** (embedded `agc`), and the M-th-power discriminator + lock signal.
- **Per sample:** wipe-off (inline `*_wipeoff`), slide the boxcar arm one sample,
    AGC-normalize, run the discriminator, filter, steer the NCO — **one update
    per input sample** (no dumping). Inline composition API (`*_wipeoff` /
    `*_arm_step`) mirrors `lo_step` / `dll_accumulate` / `symsync_step`.
- **Exposes** `norm_freq`, `lock_signal`/lock metric, `m`, `n` (boxcar window
    divisor: window = `sps/n`), loop `bn`/`zeta` — and its NCO so a composing
    receiver can drive the **same** NCO with a decision-directed error on handover.
- **Config:** `m` (2/4/8), `sps`, `n` (default 4), `bn`, `zeta`, seed
    `init_norm_freq`. All params default + keyword-capable (no forced positionals).

Working name **`track.CarrierNda`** (non-data-aided). Naming review: there are
now three carrier loops — `Costas` (BPSK decision-directed), `CarrierMpsk`
(M-PSK decision-directed), `CarrierNda` (M-PSK non-data-aided). This revives the
earlier `track.Carrier.*` namespace idea; deferred (the jm-owned `__init__.py`
makes a nested namespace a drift / `.so`-is-the-API concern) — flat names
for now.

**Since the rebuild, `MpskReceiver` no longer embeds `CarrierNda` as a whole.**
It cannot: `CarrierNda` owns an NCO and a boxcar arm, and the receiver's NCO is
now the DDC's LO and its "arm" is the cascade's own output. What the receiver
reuses is the part that matters and that must never fork — the **discriminator**
(`carrier_nda_disc`), whose lock signal is normalised by construction — it
reads ~1.0 at lock for every M, so a threshold means one thing everywhere.
`CarrierNda` remains a
first-class standalone object for anyone who wants the complete loop.

______________________________________________________________________

## 4. Matched filter (I&D default, RRC opt-in)

**As built, this is the cascade's terminal polyphase stage, not a separate FIR**
(§1.1) — but the two shapes and their meaning are unchanged:

- **I&D / boxcar (default):** the matched filter for a rectangular NRZ symbol
    pulse (and the natural front for a despread chip stream).
- **RRC (opt-in):** `rrc_taps(beta, sps, span)` — matched to an RRC-shaped
    transmitter.

!!! tip "Use `m_out >= 4` with `pulse="iandd"`"

    The rectangle is one symbol wide, so at `m_out = 2` its matched filter
    degenerates to a two-tap sum and the eye barely opens — measured lock
    statistic **−0.34 at `m_out = 2` against +0.95 at 4** on the same NRZ stream.
    The matched converter reports this itself: `narrow_pulse` is a real property
    and construction raises a `UserWarning`, rather than the condition being
    documented and left for the caller to rediscover.

> *Historical:* the original build used a per-sample `fir_step` on a dense
> matched-filter FIR owned by pointer. `fir_step` still exists and is still the
> right primitive for a per-sample FIR; this receiver no longer needs one.

> **No receiver-level AGC.** The decision-directed *tracking* path is already
> amplitude-invariant — the nearest-point slice and the `|y|`-normalized
> discriminator both ignore scale — so no front-end AGC is added here. The
> *acquisition* path is different: the raw M-th-power NDA discriminator is not
> amplitude-invariant, so `CarrierNda` carries its **own** internal arm AGC that
> normalizes the arm sample to unit power before the detector (§2.3). That AGC is
> internal to the acquisition loop, not a receiver component or a config param.

______________________________________________________________________

## 5. Symbol timing

The receiver embeds **`ratesync_loop_t` — [RateSync](../gallery/ratesync.md)'s
timing loop itself**, factored out of `ratesync_state_t` so the cascade and the
loop are separable. That split was made bit-exact deliberately (`ratesync`'s
`take_output` touched zero cascade fields, so it moved unchanged), and it is what
keeps a single implementation of Gardner + the PI filter + the lock statistic
serving both objects. Timing stays modulation-agnostic (`|·|²`), so it settles in
parallel with carrier acquisition.

The strobe geometry is the terminal stage's: every `m_out`-th output is the
on-time strobe, and the one `m_out/2` back is the transition gate. **There is no
second timing mechanism** — the terminal accumulator alone controls timing, and
the polyphase arm is that phase's fractional read-out.

`SymbolSync`'s second selectable TED (`ted="dttl"`, a decision-directed sign-sign
DTTL) is likewise reachable here (`RATESYNC_TED_DTTL`), but the receiver stays
hardcoded to Gardner: DTTL's hard-decision device is only valid for
constellations with independent, rectangular I/Q boundaries (BPSK/QPSK), not the
8PSK this receiver also supports, so exposing it is a follow-up, not a drop-in
default.

> *Historical:* the original build reused `track.SymbolSync` (Gardner + Farrow)
> as a whole, via an added by-value `symsync_init`. `SymbolSync` is unchanged and
> remains the standalone object; the receiver simply no longer interpolates with
> a Farrow.

______________________________________________________________________

## 6. Component reuse

**As built** (post-rebuild). Everything here is reused, not reimplemented:

| Piece                                                               | Verdict                                              |
| ------------------------------------------------------------------- | ---------------------------------------------------- |
| `MatchedDDC` / `MatchedDdcr`                                        | the whole front end — mix, decimate, match           |
| `RateConverter` terminal polyphase stage                            | matched filter **and** fractional delay, fused       |
| `ratesync_loop_t`                                                   | timing loop — RateSync's own, factored out for reuse |
| `carrier_nda_disc`                                                  | NDA acquisition math — shared with `CarrierNda` (§3) |
| `CarrierMpsk` decision-directed discriminator                       | tracking-path math — reuse the update                |
| `loop_filter` PI                                                    | every loop embeds it by value — as-is                |
| `lockdet`                                                           | verify-counted two-way handover gate — as-is         |
| `agc`                                                               | discriminator input normalisation (§2.3)             |
| `mpsk` slicer/demap (`mpsk_slice`, `mpsk_demap`, `mpsk_diff_demap`) | decision + bits + differential — as-is               |
| `mpsk_rx_loops_t`                                                   | **shared verbatim** by both receiver types (§1.2)    |
| `Dll(segments)`                                                     | optional DSSS front-end — pipeline, not fused        |

Retired from this receiver by the rebuild (all still first-class objects in their
own right): `lo` driven directly, `boxcar` as an NDA arm, `SymbolSync`'s
Gardner+Farrow loop, and a dense `fir` matched filter.

______________________________________________________________________

## 7. Build plan (sequential, each rock-solid first) — **complete**

Steps 1–3 shipped as written; step 4 is the rebuild this document now describes.

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

## 8. Resolved / open review points

- **NDA discriminator form** — *resolved.* Raw M-th-power via repeated squaring
    (§2.3) on an AGC-normalized arm, with the lock signal left UNSCALED so it
    reads ~1.0 at lock for every M. It used to carry a per-M `lock_scale` of
    1 / 0.619 / 0.412, which made the statistic's ceiling M-dependent and the
    default handover threshold of 0.5 **unreachable at 8PSK** (ceiling 0.412):
    `car.locked` could never be set on a perfectly working 8PSK receiver, and
    every call site that needed a meaningful threshold multiplied the scale
    back in by hand. Squaring-loss equations corrected and Yuen-grounded
    (§2.3).
- **Arm normalization** — *resolved.* Internal `agc_core` AGC + 10 dB square
    clip, not a per-sample limiter, with the loop-filter command decimated via
    `gain_update_period` (§2.3). The **bandwidth rule differs by object, and
    both are current**: `CarrierNda` locks its arm AGC to `0.01·bn`
    (`CARRIER_NDA_AGC_BW_RATIO`), the per-sample convention its own boxcar arm
    still runs on, while `MpskReceiver` uses the absolute
    `MPSK_RX_AGC_BW = 0.002` — because its discriminator runs at the *symbol*
    rate, where a proportional rule spans thousands of symbols and never
    settles (§2.3). Reading the `0.01·bn` rule as the receiver's is a mistake
    this bullet used to invite by naming only one of the two.
- **Naming** — `CarrierNda` / flat vs a `Carrier.*` namespace (deferred).
- **Handover threshold** — *resolved, shipped.* `mpsk_receiver_configure_lock()`
    exposes it as a real config call: a `lockdet_state_t handover` gate plus
    `MPSK_RX_HANDOVER_DOWN`/`N_UP`/`N_DOWN` debounce counters
    (`native/inc/mpsk_receiver/mpsk_receiver_core.h`), landed in the lock-detector
    consistency pass. `CarrierNda`/`MpskReceiver` also both expose telemetry
    probes (`.lock`/`.tracking`) from that same pass, not just this threshold.
- **`n` → `m_out`** — *resolved by the rebuild.* The old `n` sized a separate NDA
    arm (window = `sps/n`); there is no arm any more, so the parameter it was
    replaced by means something different: `m_out` is the terminal stage's
    **outputs per symbol** (even, 2–8, default 8). It sets the Gardner strobe/gate
    geometry and how much oversampled matched-filtered signal the caller gets, not
    a loop rate. See the `m_out >= 4` caveat for I&D in §4.
- **`bn_carrier` normalisation** — *changed by the rebuild.* Symbol-rate, not
    sample-rate (§1.1). This is the one silent break: old code keeps running and
    simply has a much wider carrier loop than it asks for.
- **Pulse-shaped (RRC) SER** — *resolved by the rebuild.* The matched filter is
    now the terminal bank itself, so the RRC path measures on the Es/N0 bound
    rather than carrying a residual matched-filter/timing loss.
- **Real-input support** — *resolved, shipped.* `track.MpskReceiverR` (§1.2), a
    separate type sharing every loop with the complex one.
- **Cold carrier pull-in on the strobe tap** — *open, by choice.* The
    strobe-only discriminator makes carrier acquisition quality depend on symbol
    timing, so on a cold start the loop integrates an invalid discriminator
    output through the timing transient; at `sps=8, m_out=4` that cost roughly a
    third of data seeds. Seeded operation is unaffected. Gating the steer on the
    timing loop's `lockdet` fixes that operating point and was implemented, then
    removed as a default — it changed one cell of a 24-cell sweep and mostly
    bought measurability (§2.2). The remedy is `nda_tap`: `mf_all` and `lo_arm`
    are timing-independent by construction. See §2.2 and
    [doppler-dsp/doppler#536](https://github.com/doppler-dsp/doppler/issues/536).
- **An above-ceiling lock statistic is a free diagnostic** — *retired,
    2026-07-27, by limiting the lock signal.* The idea was that `lock > 1` is
    impossible for a valid constellation, so it detects "discriminator input is
    garbage" (unsettled timing, or a gain fault) with no new computation. Limiting
    made `Re((z/|z|)^M)` amplitude-blind and hard-bounded in ±1, so the condition
    can no longer arise — the diagnostic is gone, deliberately, in exchange for a
    threshold that maps to a false-alarm probability at every M (§2.3). Worth
    knowing this was a real cost: a *future* AGC gain fault has to be caught from
    the AGC's own gain, not from this statistic.
- **DSSS re-measurement** — *open.* Both DSSS receivers sit downstream of this
    engine and have not been re-tuned for it. This is **not** a retune: the
    localisation says the DSSS fault is unrecovered carrier phase in the
    pre-despread Costas loop (clean amplitudes, scattered EVM, chance BER), which
    is that chain's own carrier loop, not this receiver's. Tracked as
    [doppler-dsp/doppler#535](https://github.com/doppler-dsp/doppler/issues/535).
    That localisation is only trustworthy because it was **not** made from a bit
    error rate alone: BER is truth-referenced and needs a lag search, so every
    measurement here pairs it with two truth-free validators — self-referenced EVM
    and blind M2M4 SNR (`native/tests/dp_sym_test.h`). Their *disagreement* is
    what carries the diagnosis: M2M4 uses only `|x|²`/`|x|⁴` moments and so is
    rotation-blind, meaning a healthy M2M4 beside a collapsed EVM says "the
    amplitudes are fine, the phase is not" — which no error rate could have told
    us.

______________________________________________________________________

## 9. Parameter surface — which knobs are design axes

**Proposed; none of this is implemented, and none of it is decided.**
`mpsk_receiver_create` takes sixteen parameters. Most are not design axes —
they are a constant, a false-alarm probability, or a settling time, and the
receiver already holds everything needed to derive them. A caller supplying
them by hand is being asked to re-derive what the object knows.

There are three separable proposals here, and each can land without the
others:

1. **§9.1** — say the rates physically (`sample_rate_hz`, `symbol_rate_hz`,
    `carrier_hz`) instead of as the ratio `sps` and a normalised frequency.
1. **§9.2** — derive the six parameters that are not design axes, leaving the
    surface at the signal description, the two loop bandwidths, and the
    carrier seed.
1. **§9.4** — make the readbacks speak the same language as the constructor.

§9.3 records the one parameter, `nda_tap`, that resists all three.

### 9.1 Physical units — the rates and the carrier

`sps` is a ratio the caller computes and the object then un-computes. A
floating-point samples-per-symbol is a poor thing to ask anyone for, and a
worse thing to ask someone who is not a modem engineer: `17.33389` is a
perfectly valid value here (§1.1), which makes the parameter look like a
mistake precisely when it is correct. The proposal is `sample_rate_hz` +
`symbol_rate_hz`, with `init_norm_freq` becoming `carrier_hz`.

This receiver is the outlier in its own library:

| object               | spells its rates                    |
| -------------------- | ----------------------------------- |
| `CarrierAcquisition` | `sample_rate_hz` / `symbol_rate_hz` |
| `AsyncDsssReceiver`  | `chip_rate` / `symbol_rate`         |
| `BurstDemod`         | `chip_rate` / `carrier_hz`          |
| **`MpskReceiver`**   | **`sps` / `init_norm_freq`**        |

**The float does not disappear, it moves.** `sps = fs/Rs` stays a double
internally — that is what makes an irrational samples-per-symbol work at all,
and it is load-bearing (§1.1). What changes is that nobody types it. The
capability becomes more reachable, not less.

**Where the units stop, and why.** `bn_carrier` and `bn_timing` stay
normalised to the symbol rate. §1.1 moved them *to* that normalisation
deliberately, and it is the property that makes one setting mean one loop at
every rate; in Hz, a caller who retunes `symbol_rate_hz` would silently
change the loop's relative bandwidth. So the constructor is mixed-unit **on
purpose** — rates and carrier physical, bandwidths dimensionless — and §9.4
closes that gap on the readback side instead, where it costs nothing.

**What the combination makes checkable.** Three guards cannot be stated while
the object knows only a ratio, and all three are things a non-expert gets
wrong:

- `|carrier_hz| < sample_rate_hz/2` — plain Nyquist. As a normalised
    frequency this was implicit in the number `0.5` and went unvalidated.
- `sample_rate_hz >= m_out · symbol_rate_hz` replaces `sps >= m_out`, so the
    rejection message stops naming `m_out` — which under §9.2 the caller no
    longer supplies — and becomes "sample_rate_hz must be at least 8×
    symbol_rate_hz".
- The real twin's usable IF band (≈ `0.06…0.44·fs`) can be checked against
    the **occupied** band rather than the centre. A rectangular pulse spans
    `carrier_hz ± symbol_rate_hz` to its first null, so a centre comfortably
    inside the band can still put its skirt on the fold — the failure already
    pinned by `test_usable_band_is_the_input_constraint`. The information was
    always there; what the combination adds is the ability to *say* it in the
    caller's own units.

**Cost, measured rather than estimated.** 90 files reference these types: 40
Python and documentation call sites pass `sps=`, 21 C call sites call a
`*_create`, and 20 of the 90 are generated `docs/c-api` pages that regenerate
rather than being edited. This is a public API break, the second deliberate
one after the engine rebuild (§1.1).

### 9.2 The six derivable parameters

Rules below are written in `sps` because that is today's parameter; under
§9.1 only `m_out`'s changes shape, to `min(8, 2·floor(fs/(2·Rs)))`. The rest
are dimensionless and read identically either way.

| Param          | Rule                                                                                                                                                                                                                                          | Status                                                                 |
| -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| `m_out`        | `min(8, 2·floor(sps/2))`. The real twin needs `min(8, 2·floor(sps/4))`, since its cascade sits behind the halfband (§1.2). `iandd` under 4 keeps the existing `narrow_pulse` warning (§4) rather than becoming a rejection.                   | firm                                                                   |
| `zeta`         | `1/√2` — a constant, not a computation. Nothing else in the receiver moves the optimal damping, and both loops already share one value.                                                                                                       | firm                                                                   |
| `acq_to_track` | `1`, always. At the default `m_out = 8` every tap decodes every order both with and without it (§2.2.1); the one measured cost is `lo_arm` trading EVM at equal SER.                                                                          | verify                                                                 |
| `lock_thresh`  | `σ_H0 · η(Pfa)` at a fixed `Pfa = 5e-6`: `0.1132 × 4.4159 = 0.4999`, today's 0.5. The limited statistic reads ~1.0 at lock for **every** M (§2.3), so no per-M correction is carried — that was the `lock_scale` bug, already resolved in §8. | firm                                                                   |
| `warmup_syms`  | `max(1/MPSK_RX_AGC_BW, 5/bn_carrier + 5/bn_timing on the strobe tap)` → 1000 at the defaults, against a shipped 100. The AGC floor is 500 symbols outright (§2.3); the timing term is why §2.4 wants timing settled before handover.          | firm in form                                                           |
| `num_phases`   | Timing quantisation is `1/(P·m_out)` symbols; budgeting its EVM contribution at −40 dB gives `P ≥ 100/m_out` → 16 at the default, against a shipped 1024.                                                                                     | **TBD** — needs a measured EVM-vs-`P` sweep; nothing here defends 1024 |
| `nda_tap`      | Not derivable — see §9.3.                                                                                                                                                                                                                     | open                                                                   |

**The rows are not equally well founded, and the weakest one is worth naming.**
Five rest on measurements recorded elsewhere in this document — `m_out` on EVM
against the coherent bound and per-M SER at each anchor, `lock_thresh` on an
analytic H0 sd confirmed by 0/100 false declares at every M, `acq_to_track` on
§2.2.1's tap × order table, `warmup_syms` on a stated settling rule plus a
measured AGC floor, and `zeta` on the fact that nothing anywhere varies it.
`num_phases` rests on **nothing in either direction**: neither the shipped
1024 nor the proposed 16 has a measurement behind it. It is the one row that
should be settled by experiment before it is written down as a rule, and it is
also the only row with a performance argument attached — the bank is
`P × ntaps × 4` bytes, so the difference is 139 KB against 2 KB, and the same
factor in create-time tap generation.

### 9.3 Why `nda_tap` is not on that list

Two reasons, and the second is the one that bites.

**It is a declared design axis.** §2.2.1 makes the tap a construction
parameter precisely *because* it is a real trade rather than an
implementation detail, and §2.2 records the one attempt to resolve the same
coupling inside the receiver — gating the strobe steer on timing lock —
being implemented, measured across a 24-cell sweep, and then removed as a
default: it changed exactly one cell, and it hid a trade the caller could
neither see nor override.

**A frequency-uncertainty input would not replace it.** The obvious
substitution is to take the carrier uncertainty the caller genuinely knows
and pick the narrowest tap covering it — and under §9.1 that uncertainty is
naturally in Hz, which is the form it would have to take. But §2.2.1's
pull-in table is measured with each tap *at its own best* `bn_carrier`; at a
**fixed** `bn_carrier` all three taps measure the same `0.01·Rs`. The tap
only buys range if the receiver sets `bn_carrier` as well — and `bn_carrier`
is one of the parameters this design deliberately leaves with the caller
(§9.1). Deriving the tap from an uncertainty means deriving both, as one
joint decision, which is a larger change than the other six rows put
together.

Note what §9.1 *does* settle here. The objection above has two halves — the
receiver cannot choose the tap, and deriving `m_out` moves the capture range
invisibly — and the second half dissolves once the object knows an absolute
symbol rate. The pull-in numbers are already fractions of `Rs`, so they
become computable in Hz and reportable; the receiver does not choose the tap,
it says what the chosen tap can catch (§9.4).

Two consequences worth stating even if `nda_tap` never moves:

- **`m_out` moves the pull-in range**, so deriving it moves a number the
    caller never touched — `strobe` goes `0.010·Rs → 0.050·Rs` between
    `m_out = 4` and 8. A receiver at four samples per symbol then gets a 5×
    narrower unaided capture than one at eight, for no reason visible at the
    call site. That is an argument for the `capture_hz` readback in §9.4, not
    against deriving `m_out`.
- **`mf_all` is dominated wherever `m_out` derives to 8**: it captures
    `0.033·Rs` against `strobe`'s `0.050` and `lo_arm`'s `0.090`, so at the
    derived `m_out` it is never the right pick and the axis is effectively two
    taps wide.

### 9.4 Readbacks — the back door speaks the same language

Two readbacks are already inconsistent with §9.1, and one is missing
entirely. None of this depends on the constructor changing; the third item is
worth having either way.

- **`timing_rate` returns smoothed tracked samples per symbol** — a float
    near the nominal, departing from it by exactly the sample-clock offset the
    timing loop is tracking. That is the same floating-point
    samples-per-symbol §9.1 removes from the constructor, left sitting on the
    readback. Its natural faces are the tracked `symbol_rate_hz`, or the clock
    offset in **ppm**, which is what a reader of this number actually wants.
- **`norm_freq` reads back in cycles/sample.** If the seed is `carrier_hz`
    then the tracked carrier should be Hz too, or the object takes one unit
    and returns another.
- **Nothing reports what the receiver can catch.** §2.2.1's pull-in range is
    a property of `(nda_tap, m_out, m)` and a fraction of `Rs`, so with an
    absolute symbol rate in scope it is computable — a read-only `capture_hz`.
    This follows the rule that a condition the code already computes should be
    exposed as a value rather than described in prose, and it is what makes a
    derived `m_out` honest (§9.3).

`bn_carrier` and `bn_timing` can gain the same treatment for free: reporting
`bn · symbol_rate_hz` alongside the dimensionless setting closes §9.1's
mixed-unit gap on the diagnostic side, without touching the input contract
that §1.1 established.
