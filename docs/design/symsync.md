---
description: "SymbolSync and the timing loop it shares with RateSync: who uses it, what it composes, the timing error's variance for each detector, and the bandwidth the loop SNR rule gives"
---

# SymbolSync

`SymbolSync` recovers symbol timing from a baseband stream: a timing
error detector (TED) measures how far the sampling instant is from the
symbol's centre, a loop filter turns that into a rate correction, and an
NCO moves the sampling instant. The same loop, with the same detectors
and the same lock quantity, is `ratesync_loop_t`, which `RateSync` and the
receivers' demodulators run. This page is the design of that loop. It
derives what the loop's timing error must be from the signal, and sizes
the loop from it.

## 1. Who uses it

| consumer                          | pulse                     | detector        | outputs a symbol   | bn (a symbol) |
| --------------------------------- | ------------------------- | --------------- | ------------------ | ------------- |
| `SymbolSync`                      | the caller's              | Gardner or DTTL | the caller's `sps` | the caller's  |
| `RateSync`                        | RRC or integrate-and-dump | Gardner or DTTL | `m`                | the caller's  |
| `MpskReceiver`                    | RRC or integrate-and-dump | Gardner         | 8                  | the caller's  |
| `AsyncDsssReceiver`'s demodulator | integrate-and-dump        | Gardner         | 8                  | 0.005         |
| `DsssReceiver`'s demodulator      | integrate-and-dump        | Gardner         | 8                  | 0.005         |

The DSSS receivers are the demanding case. Their symbol clock is not tied
to the code, so after despreading the timing is recovered from data
transitions alone, at Es/N0 from 4 dB, through stretches with no
transitions (the 415.1-B-1 period's 275-symbol gap).

## 2. Design goals

1. **The timing loop is sized by the loop SNR rule**, as the code and
    carrier loops are (async-dsss-receiver.md §1.3): a loop SNR of at
    least 20 dB at the lowest Es/N0 the consumer states, counting the
    detector's noise-by-noise loss.
1. **The timing error's variance is derived for each detector and each
    pulse, and measured**, open loop and closed loop. The two must agree.
1. **A locked loop does not slip.** At a 20 dB loop SNR a whole-symbol
    slip is a rare event with a rate theory gives; the loop meets it.
1. **The rate state stays inside what the signal can have.** The symbol
    rate is known to the source's tolerance and the Doppler dilation;
    nothing in the loop may sit outside that.
1. **The lock quantity says whether the loop samples near the eye
    centre of a signal that has transitions.** It does not measure the
    timing error (§7); the timing error is measured at the strobe. Its
    threshold separates a locked loop from an unlocked one at the
    consumer's lowest Es/N0, with stated false-lock and false-unlock
    probabilities.

## 3. What it composes, and what each leaves to it

| object               | what it guarantees                                    | what it leaves to this loop                                                            |
| -------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------------- |
| `NCO`                | the phase word and its control port, exact to one LSB | "`nco_steer_scale` has no production caller"; steering is unbounded                    |
| `LoopFilter`         | `bn` is the loop's noise bandwidth, to a closed form  | "no anti-windup and no output bound"                                                   |
| Farrow / `Resampler` | the interpolant; `mu`, the sampling instant           | "a wrapping `mu` is a residual rate error, and one wrap is one input interval of slip" |
| `LockDet`            | the verify counts compound as documented              | which quantity it is stepped on, and its thresholds                                    |

Each of the three certified reports hands the bound to its caller. This
loop is that caller, so goals 3 and 4 are its to meet.

## 4. The detector

With `y_k` the matched-filter output at the symbol strobe and `y_mid,k`
the output half a symbol earlier:

```text
Gardner:  e_k = Re{ y_mid,k · conj(y_k − y_{k−1}) }
DTTL:     e_k = Re{ y_mid,k · conj(d_k − d_{k−1}) },  d = the hard decision
```

Its mean against a standing timing offset `τ` (in symbols) is the
S-curve `S(τ)`, with two zeros a symbol: a stable one at the eye centre
and an unstable one half a symbol away. The slope at the stable zero is
the detector gain `Kd = S′(0)`, per symbol of timing error. The loop
divides by it once, at construction (`dp_symsync_ted_slope()`), so a
correctly normalized detector has unit slope.

For data symbols `a_k` and the overall pulse `g(t)` (Nyquist, `g(0)` = 1):

```text
S(τ) = Σ_m g((m − ½)T + τ) · [ g(mT + τ) − g((m − 1)T + τ) ]
```

## 5. The timing error's variance

The loop is far narrower than the symbol rate, so it sees the detector's
noise only through its spectral density at zero frequency,
`S_e(0) = Σ_m R_e(m)`, the sum of the error's autocorrelation over all
lags. Adjacent errors share samples, so the lags matter: summing them is
what separates this from the variance of one error. For a loop of noise
bandwidth `bn` a symbol:

```text
σ_τ² = 2 · bn · S_e(0) / Kd²          (symbols²)
```

`S_e(0)` has three parts, each a function of the pulse alone:

```text
S_e(0) = S_self + S_sn / γ + S_nn / γ²,      γ = Es/N0
```

- `S_self`: self noise, the data's own contribution at zero timing
    error. It is the floor a noiseless loop rests on.
- `S_sn / γ`: signal by noise.
- `S_nn / γ²`: noise by noise. It is this detector's squaring loss.

### 5.1 Gardner on integrate-and-dump BPSK

This is the DSSS receivers' case, and it has a closed form. The pulse
after the integrator is the triangle `g(t) = 1 − |t|/T`. At zero timing
error a mid sample is `(a_k + a_{k−1})/2`, which is 0 on a transition and
±1 otherwise, and the difference `a_k − a_{k−1}` is 0 where the mid
sample is not. Their product is always 0:

- **`S_self` = 0.** This detector on this pulse has no self noise.
- **`Kd` = 3/2.** From `S(τ) = (3/2)τ − 3τ²` for i.i.d. data.
- **`S_e(0) = (3/4)(1/γ)(1 + 1/γ)`.** One error has variance
    `(3/2)/γ + 1/γ²`; the lag-1 correlation is `−(3/8)/γ − (1/8)/γ²`;
    further lags are 0.

So:

```text
σ_τ² = (2 · bn / 3) · (1/γ) · (1 + 1/γ)      (symbols²)
```

The factor `1 + 1/γ` is the noise-by-noise loss, `S_L = 1/(1 + 1/γ)`.
The result assumes a transition density of one half; with density `p_t`
both `Kd` and the signal-by-noise term scale with it.

### 5.2 Other pulses, and DTTL

For an RRC matched pair `g` is the raised cosine and `S_self` is not
zero: the mid sample at a transition carries the neighbours' tails. The
three coefficients are sums over `g` at half-symbol spacing and are
computed, not typed. For DTTL the difference is of decisions, so the
coefficients also depend on the decision error rate at `γ`; they are an
expectation over the decision, computed numerically. One function
returns `Kd`, `S_self`, `S_sn` and `S_nn` for a detector, a pulse and a
`γ`, beside `dp_symsync_ted_slope()`, which already returns the first.

## 6. The loop SNR rule, and the bandwidth it gives

The timing loop's SNR is taken on the symbol clock's phase,
`φ = 2π·τ`, as a carrier loop's is taken on the carrier's:

```text
ρ = 1 / σ_φ² = 1 / ( (2π)² · σ_τ² )
```

A 20 dB loop SNR is `σ_τ` = 0.0159 symbols rms. With §5.1:

```text
bn = 3 · γ · S_L / ( 2 · (2π)² · ρ ),      ρ = 100
```

| Es/N0 | `σ_τ` at bn 0.005 | loop SNR at bn 0.005 | bn for 20 dB |
| ----- | ----------------- | -------------------- | ------------ |
| 4 dB  | 0.043 symbols     | 11.4 dB              | 0.00068      |
| 5 dB  | 0.037 symbols     | 12.6 dB              | 0.00091      |
| 10 dB | 0.019 symbols     | 18.4 dB              | 0.0035       |

The DSSS receivers ship `bn` 0.005, chosen on steady-state EVM. By this
derivation that is about seven times wider than the rule allows at
Es/N0 4 dB. The gain a loop actually runs at also scales with the square
of the signal amplitude at the detector (Gardner's error carries `A²`),
so the level the demodulator's gain control delivers at low Es/N0 moves
the effective bandwidth further.

## 7. The lock quantity

```text
L_k = 2 · ( |y_k|² − |y_mid,k|² ) / ( |y_k|² + |y_mid,k|² )
```

averaged over a block. It is a signed measure of where the loop samples,
not a detection statistic with a noise-only null. On integrate-and-dump
data with no noise it is +2 on a transition and 0 otherwise at the eye
centre, mean +1; half a symbol off the two samples trade places and the
mean is −1; with no transitions it is 0. Noise pulls the magnitude
toward 0. With no noise, a transition at timing error `τ` (in symbols)
gives

```text
L(τ) = 2 · (1 − 4|τ|) / (1 − 4|τ| + 8τ²)  ≈  2 · (1 − 8τ²)   near τ = 0
```

So the quantity is not a measure of the timing error:

- **it is flat at the centre.** The 20 dB rule's `σ_τ` of 0.0159 symbols
    lowers the mean by 0.2%; it crosses zero only at a quarter symbol;
- **it is even in `τ`**, so it cannot say which way the loop is off;
- **its mean moves with `γ` and with the transition density** as much as
    with `τ`: a centred loop at low Es/N0 and an off-centre loop at high
    Es/N0 can read the same.

What it can do is tell a loop near the eye centre from one that is not,
or from no signal. Its mean and variance are computed from the same
model as §5, locked and unlocked, and:

- **its threshold sits between the locked mean at the consumer's lowest
    `γ` and the unlocked mean**, with the block length and counts set by
    the two variances and the stated error probabilities;
- **its sign is information.** A block mean below zero says the loop is
    nearer the unstable zero than the stable one.

## 8. Slips

A slip is the loop's phase leaving one stable zero for the next. Its
mean time is a function of `ρ` and `bn` that grows exponentially with
`ρ`; at 20 dB it is far longer than any pass. A loop that slips while
its flags are up is therefore running at a loop SNR well under the rule,
or is not yet settled. Two things can do that besides the bandwidth:
the rate state away from the signal's rate (goal 4), and a stretch with
no transitions, where the detector's output is noise alone and the loop
coasts on its rate state.

## 9. Unknowns

- **Which convention `bn` carries in `σ_τ² = 2·bn·S_e(0)/Kd²`.**
    `LoopFilter`'s report measures `bn` as the noise bandwidth; the factor
    of 2 here is the one-sided form. A measured closed-loop `σ_τ` settles
    it, and moves §6's table by at most 3 dB.
- **Whether the loop SNR is defined on the clock's phase**, as §6 takes
    it. It is the definition that makes 20 dB mean the same margin against
    a slip as it does for a carrier.
- **The coefficients for RRC and for DTTL**, and the lock quantity's mean
    and variance against `τ` and `γ`: derived in §5 and §7, computed by
    nothing yet.
- **The closed-loop `σ_τ`, measured.** It needs the strobe's input-sample
    time as telemetry (#3177).
- **The signal amplitude at the detector inside each receiver** at
    Es/N0 4 dB, and so the gain the loop really runs at.
- **The loop through a gap**: how far the phase coasts in 275 symbols
    with no transitions, from the rate state's error.
- **The rate state's bound**: the range the consumer's signal can have,
    and what the loop does at its edge.
- **The slip rate against `ρ`**, measured where it is not zero, to check
    §8 against theory.

## 10. The work, in order

1. The coefficient function of §5.2, held to §5.1's closed form and to an
    open-loop measurement of `S_e(0)` on the real detector.
1. The strobe-time probe (#3177), and the closed-loop `σ_τ` against
    §5's form, for both detectors and both pulses.
1. The lock quantity's mean and variance against `τ` and `γ`, computed
    and measured; its threshold re-derived from §7.
1. The rate state's bound, and the loop through a gap.
1. Each consumer's `bn` from §6 at its own lowest Es/N0, and the slip
    rate measured against §8.
1. The certification report (`tests/validation/symsync/`), which the
    object does not have.

## 11. Measured: the loop filter's output against time

One run of `validate_timing_loop_noise --trace`: both timing loops, each
closed-loop scenario of the harness, from the first strobe, three times
over. Started at the eye center the loop holds lock. Started a quarter
and a half symbol off it the loop acquires: half a symbol is the
detector's unstable zero, and a quarter is where its restoring force is
largest.

The table has every scenario. The figures are four of them: the case
the DSSS receivers run (the rectangle at Es/N0 4 dB) holding and
acquiring, and the two settings that move a loop's rate state (level
and clock offset).

A loop-filter panel is the output averaged over a block of symbols, with
its integrator, in parts per million of the symbol rate. A held scenario
shows all 60,000 symbols; an acquiring one shows its first `20 / bn`,
where the acquisition is. A timing panel is the strobe's distance from
the eye center, in symbols, from both starting offsets. An acquisition
ends at 0 or at −1, the next eye center.

The integrator's mean, the output's rms and the timing error's rms are
over the second half of each scenario, per symbol.

<!-- symsync:stamp:start -->

| run                                                                                                           | when                       | machine                                                     | commit             | toolchain                                                             | command               |
| ------------------------------------------------------------------------------------------------------------- | -------------------------- | ----------------------------------------------------------- | ------------------ | --------------------------------------------------------------------- | --------------------- |
| `validate_timing_loop_noise --trace`, 108 scenarios of 29,000 or 30,000 or 59,000 or 60,000 symbols, one seed | 2026-11-25 15:24 UTC, 13 s | `omni-x7358h`: Intel(R) Core(TM) Ultra X7 358H (16 threads) | `27745815d`, clean | GNU 15.2.0; glibc 2.43; build Release, flags -O3 -DNDEBUG, native OFF | `make symsync-record` |

<!-- symsync:stamp:end -->

<!-- symsync:scenarios:start -->

| loop       | pulse              | Es/N0 | bn      | level A | clock, ppm | started off center, symbols | ends at, symbols from center | inside 0.25 symbol for good after, symbols | integrator mean, ppm | output rms, ppm | timing error rms, symbols | figure                                                 |
| ---------- | ------------------ | ----- | ------- | ------- | ---------- | --------------------------- | ---------------------------- | ------------------------------------------ | -------------------- | --------------- | ------------------------- | ------------------------------------------------------ |
| RateSync   | rectangle          | 10 dB | 0.00068 | 1       | 0          | 0                           | -0.05                        | —                                          | +1.0                 | 486             | 0.0464                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.00068 | 1       | 0          | 0.25                        | -0.04                        | 0                                          | +5.5                 | 480             | 0.0363                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.00068 | 1       | 0          | 0.5                         | +0.02                        | 911                                        | -0.5                 | 483             | 0.0120                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.002   | 1       | 0          | 0                           | -0.04                        | —                                          | +2.7                 | 1430            | 0.0401                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.002   | 1       | 0          | 0.25                        | +0.00                        | 0                                          | -2.3                 | 1425            | 0.0391                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.002   | 1       | 0          | 0.5                         | -0.00                        | 351                                        | -0.4                 | 1435            | 0.0437                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.005   | 1       | 0          | 0                           | -0.03                        | —                                          | +12.4                | 3602            | 0.0418                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.005   | 1       | 0          | 0.25                        | -0.02                        | 0                                          | +13.7                | 3584            | 0.0411                    | —                                                      |
| RateSync   | rectangle          | 10 dB | 0.005   | 1       | 0          | 0.5                         | -0.02                        | 125                                        | +17.9                | 3602            | 0.0415                    | —                                                      |
| RateSync   | rectangle          | 4 dB  | 0.00068 | 1       | 0          | 0                           | -0.04                        | —                                          | +2.7                 | 1045            | 0.0395                    | [plot](symsync-measurements/hold_rectangle.png)        |
| RateSync   | rectangle          | 4 dB  | 0.00068 | 1       | 0          | 0.25                        | -0.03                        | 0                                          | +2.4                 | 1039            | 0.0336                    | [plot](symsync-measurements/acq_timing_rectangle.png)  |
| RateSync   | rectangle          | 4 dB  | 0.00068 | 1       | 0          | 0.5                         | -0.03                        | 776                                        | +7.1                 | 1049            | 0.0392                    | [plot](symsync-measurements/acq_output_rectangle.png)  |
| RateSync   | rectangle          | 4 dB  | 0.002   | 1       | 0          | 0                           | -0.03                        | —                                          | +9.2                 | 3070            | 0.0416                    | —                                                      |
| RateSync   | rectangle          | 4 dB  | 0.002   | 1       | 0          | 0.25                        | -0.01                        | 0                                          | +12.2                | 3053            | 0.0440                    | [plot](symsync-measurements/acq_timing_rectangle.png)  |
| RateSync   | rectangle          | 4 dB  | 0.002   | 1       | 0          | 0.5                         | -0.01                        | 359                                        | +13.0                | 3077            | 0.0412                    | [plot](symsync-measurements/acq_output_rectangle.png)  |
| RateSync   | rectangle          | 4 dB  | 0.005   | 1       | 0          | 0                           | -0.03                        | —                                          | +54.8                | 7668            | 0.0500                    | [plot](symsync-measurements/hold_rectangle.png)        |
| RateSync   | rectangle          | 4 dB  | 0.005   | 1       | 0          | 0.25                        | -0.02                        | 0                                          | +54.5                | 7596            | 0.0551                    | [plot](symsync-measurements/acq_timing_rectangle.png)  |
| RateSync   | rectangle          | 4 dB  | 0.005   | 1       | 0          | 0.5                         | -0.02                        | 113                                        | +55.0                | 7673            | 0.0485                    | [plot](symsync-measurements/acq_output_rectangle.png)  |
| RateSync   | rectangle          | none  | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | -3.7                 | 0               | 0.0319                    | [plot](symsync-measurements/hold_rectangle.png)        |
| RateSync   | rectangle          | none  | 0.005   | 1       | 0          | 0.25                        | -0.08                        | 0                                          | +0.0                 | 0               | 0.0000                    | —                                                      |
| RateSync   | rectangle          | none  | 0.005   | 1       | 0          | 0.5                         | -0.01                        | 143                                        | -3.7                 | 0               | 0.0159                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.00068 | 1       | 0          | 0                           | -0.00                        | —                                          | +0.3                 | 941             | 0.0055                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.00068 | 1       | 0          | 0.25                        | -0.00                        | 19                                         | -0.0                 | 954             | 0.0057                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.00068 | 1       | 0          | 0.5                         | -1.00                        | 1664                                       | -0.2                 | 947             | 0.0062                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 0.5     | 0          | 0                           | -0.00                        | —                                          | +0.0                 | 691             | 0.0071                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 0.5     | 0          | 0.25                        | -0.00                        | 19                                         | -0.7                 | 701             | 0.0073                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 0.5     | 0          | 0.5                         | -1.00                        | 1539                                       | -0.6                 | 696             | 0.0076                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 1       | 0          | 0                           | -0.00                        | —                                          | +4.6                 | 2777            | 0.0106                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 1       | 0          | 0.25                        | +0.00                        | 17                                         | +4.2                 | 2817            | 0.0105                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 1       | 0          | 0.5                         | -1.00                        | 618                                        | +4.6                 | 2797            | 0.0113                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 2       | 0          | 0                           | +0.00                        | —                                          | +103.5               | 11328           | 0.0213                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 2       | 0          | 0.25                        | +0.01                        | 15                                         | +110.3               | 11473           | 0.0230                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.002   | 2       | 0          | 0.5                         | -0.99                        | 260                                        | +111.3               | 11399           | 0.0227                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | +38.5                | 7006            | 0.0182                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.005   | 1       | 0          | 0.25                        | +0.00                        | 15                                         | +38.6                | 7100            | 0.0194                    | —                                                      |
| RateSync   | root-raised cosine | 10 dB | 0.005   | 1       | 0          | 0.5                         | -1.00                        | 258                                        | +37.2                | 7053            | 0.0196                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.00068 | 1       | 0          | 0                           | -0.00                        | —                                          | +1.7                 | 1692            | 0.0125                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.00068 | 1       | 0          | 0.25                        | +0.00                        | 24                                         | +0.8                 | 1707            | 0.0133                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.00068 | 1       | 0          | 0.5                         | -0.99                        | 1319                                       | +0.6                 | 1689            | 0.0145                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.002   | 1       | 0          | 0                           | +0.00                        | —                                          | +20.7                | 4991            | 0.0240                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.002   | 1       | 0          | 0.25                        | +0.01                        | 19                                         | +19.4                | 5040            | 0.0252                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.002   | 1       | 0          | 0.5                         | -0.99                        | 495                                        | +18.1                | 4989            | 0.0259                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | +141.2               | 12560           | 0.0400                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.005   | 1       | 0          | 0.25                        | +0.01                        | 19                                         | +141.5               | 12676           | 0.0449                    | —                                                      |
| RateSync   | root-raised cosine | 4 dB  | 0.005   | 1       | 0          | 0.5                         | -0.99                        | 140                                        | +141.6               | 12555           | 0.0435                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | -5.3                 | 4372            | 0.0044                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | 0          | 0.25                        | +0.00                        | 1                                          | -7.6                 | 4384            | 0.0044                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | 0          | 0.5                         | +0.00                        | 189                                        | -7.4                 | 4384            | 0.0044                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | -1000      | 0                           | -0.01                        | —                                          | +1019.0              | 4375            | 0.0043                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | -1000      | 0.25                        | -0.01                        | 0                                          | +1018.4              | 4387            | 0.0043                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | -1000      | 0.5                         | -0.01                        | 78                                         | +1018.4              | 4387            | 0.0043                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | +1000      | 0                           | +0.01                        | —                                          | -981.0               | 4369            | 0.0043                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | +1000      | 0.25                        | +0.01                        | 19                                         | -980.9               | 4381            | 0.0043                    | —                                                      |
| RateSync   | root-raised cosine | none  | 0.005   | 1       | +1000      | 0.5                         | -0.99                        | 107                                        | -980.9               | 4381            | 0.0043                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.00068 | 1       | 0          | 0                           | -0.00                        | —                                          | +2.4                 | 657             | 0.0084                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.00068 | 1       | 0          | 0.25                        | -0.00                        | 0                                          | +3.6                 | 661             | 0.0058                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.00068 | 1       | 0          | 0.5                         | -0.00                        | 820                                        | +3.0                 | 656             | 0.0083                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.002   | 1       | 0          | 0                           | -0.00                        | —                                          | +11.9                | 1939            | 0.0142                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.002   | 1       | 0          | 0.25                        | -0.00                        | 0                                          | +14.8                | 1950            | 0.0118                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.002   | 1       | 0          | 0.5                         | -0.00                        | 354                                        | +10.5                | 1935            | 0.0138                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.005   | 1       | 0          | 0                           | -0.00                        | —                                          | +49.9                | 4884            | 0.0221                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.005   | 1       | 0          | 0.25                        | -0.00                        | 0                                          | +51.4                | 4914            | 0.0212                    | —                                                      |
| SymbolSync | rectangle          | 10 dB | 0.005   | 1       | 0          | 0.5                         | -0.00                        | 179                                        | +47.0                | 4879            | 0.0242                    | —                                                      |
| SymbolSync | rectangle          | 4 dB  | 0.00068 | 1       | 0          | 0                           | -0.01                        | —                                          | +3.4                 | 1125            | 0.0171                    | [plot](symsync-measurements/hold_rectangle.png)        |
| SymbolSync | rectangle          | 4 dB  | 0.00068 | 1       | 0          | 0.25                        | -0.00                        | 0                                          | +6.0                 | 1128            | 0.0113                    | [plot](symsync-measurements/acq_timing_rectangle.png)  |
| SymbolSync | rectangle          | 4 dB  | 0.00068 | 1       | 0          | 0.5                         | -0.00                        | 917                                        | +5.5                 | 1121            | 0.0167                    | [plot](symsync-measurements/acq_output_rectangle.png)  |
| SymbolSync | rectangle          | 4 dB  | 0.002   | 1       | 0          | 0                           | -0.00                        | —                                          | +15.4                | 3325            | 0.0300                    | —                                                      |
| SymbolSync | rectangle          | 4 dB  | 0.002   | 1       | 0          | 0.25                        | -0.00                        | 0                                          | +26.1                | 3328            | 0.0230                    | [plot](symsync-measurements/acq_timing_rectangle.png)  |
| SymbolSync | rectangle          | 4 dB  | 0.002   | 1       | 0          | 0.5                         | -0.00                        | 354                                        | +18.5                | 3311            | 0.0265                    | [plot](symsync-measurements/acq_output_rectangle.png)  |
| SymbolSync | rectangle          | 4 dB  | 0.005   | 1       | 0          | 0                           | -0.01                        | —                                          | +70.7                | 8399            | 0.0469                    | [plot](symsync-measurements/hold_rectangle.png)        |
| SymbolSync | rectangle          | 4 dB  | 0.005   | 1       | 0          | 0.25                        | -0.00                        | 0                                          | +64.8                | 8412            | 0.0444                    | [plot](symsync-measurements/acq_timing_rectangle.png)  |
| SymbolSync | rectangle          | 4 dB  | 0.005   | 1       | 0          | 0.5                         | -1.00                        | 169                                        | +77.7                | 8401            | 0.0496                    | [plot](symsync-measurements/acq_output_rectangle.png)  |
| SymbolSync | rectangle          | none  | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | +0.0                 | 0               | 0.0000                    | [plot](symsync-measurements/hold_rectangle.png)        |
| SymbolSync | rectangle          | none  | 0.005   | 1       | 0          | 0.25                        | +0.00                        | 10                                         | +0.0                 | 0               | 0.0000                    | —                                                      |
| SymbolSync | rectangle          | none  | 0.005   | 1       | 0          | 0.5                         | +0.00                        | 150                                        | +0.0                 | 0               | 0.0000                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.00068 | 1       | 0          | 0                           | -0.00                        | —                                          | +6.7                 | 916             | 0.0064                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.00068 | 1       | 0          | 0.25                        | -0.00                        | 34                                         | +7.5                 | 925             | 0.0049                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.00068 | 1       | 0          | 0.5                         | -1.00                        | 1782                                       | +6.7                 | 923             | 0.0050                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 0.5     | 0          | 0                           | -0.00                        | —                                          | +27.1                | 2709            | 0.0109                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 0.5     | 0          | 0.25                        | +0.00                        | 35                                         | +26.8                | 2737            | 0.0110                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 0.5     | 0          | 0.5                         | -0.00                        | 784                                        | +30.3                | 2728            | 0.0098                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 1       | 0          | 0                           | -0.00                        | —                                          | +27.1                | 2709            | 0.0109                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 1       | 0          | 0.25                        | +0.00                        | 33                                         | +26.8                | 2737            | 0.0110                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 1       | 0          | 0.5                         | -0.00                        | 706                                        | +30.3                | 2728            | 0.0098                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 2       | 0          | 0                           | -0.00                        | —                                          | +27.1                | 2709            | 0.0109                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 2       | 0          | 0.25                        | +0.00                        | 29                                         | +26.8                | 2737            | 0.0110                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.002   | 2       | 0          | 0.5                         | -0.00                        | 404                                        | +30.3                | 2728            | 0.0098                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.005   | 1       | 0          | 0                           | -0.00                        | —                                          | +110.0               | 6849            | 0.0178                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.005   | 1       | 0          | 0.25                        | +0.00                        | 33                                         | +110.8               | 6918            | 0.0180                    | —                                                      |
| SymbolSync | root-raised cosine | 10 dB | 0.005   | 1       | 0          | 0.5                         | +0.00                        | 196                                        | +121.8               | 6903            | 0.0171                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.00068 | 1       | 0          | 0                           | -0.00                        | —                                          | +6.8                 | 1288            | 0.0126                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.00068 | 1       | 0          | 0.25                        | +0.00                        | 35                                         | +7.4                 | 1293            | 0.0107                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.00068 | 1       | 0          | 0.5                         | -1.00                        | 1881                                       | +6.3                 | 1292            | 0.0112                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.002   | 1       | 0          | 0                           | -0.00                        | —                                          | +27.5                | 3804            | 0.0232                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.002   | 1       | 0          | 0.25                        | +0.00                        | 33                                         | +31.4                | 3823            | 0.0226                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.002   | 1       | 0          | 0.5                         | -1.00                        | 551                                        | +22.9                | 3816            | 0.0217                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | +115.0               | 9591            | 0.0374                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.005   | 1       | 0          | 0.25                        | +0.01                        | 33                                         | +110.4               | 9647            | 0.0377                    | —                                                      |
| SymbolSync | root-raised cosine | 4 dB  | 0.005   | 1       | 0          | 0.5                         | +0.00                        | 394                                        | +124.3               | 9624            | 0.0354                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | 0          | 0                           | +0.00                        | —                                          | +202.3               | 4745            | 0.0049                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | 0          | 0.25                        | +0.00                        | 17                                         | +203.0               | 4758            | 0.0048                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | 0          | 0.5                         | +0.00                        | 121                                        | +203.0               | 4758            | 0.0048                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | -1000      | 0                           | +0.01                        | —                                          | +1026.1              | 4756            | 0.0050                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | -1000      | 0.25                        | +0.01                        | 17                                         | +1024.0              | 4769            | 0.0049                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | -1000      | 0.5                         | +0.01                        | 271                                        | +1024.0              | 4769            | 0.0049                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | +1000      | 0                           | -0.01                        | —                                          | -982.1               | 4748            | 0.0049                    | [plot](symsync-measurements/hold_level_and_offset.png) |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | +1000      | 0.25                        | -0.01                        | 35                                         | -981.7               | 4762            | 0.0049                    | —                                                      |
| SymbolSync | root-raised cosine | none  | 0.005   | 1       | +1000      | 0.5                         | -1.01                        | 154                                        | -981.7               | 4762            | 0.0049                    | —                                                      |

<!-- symsync:scenarios:end -->

![Both loops holding lock on the rectangle](symsync-measurements/hold_rectangle.png)

![Both loops acquiring on the rectangle: the timing error](symsync-measurements/acq_timing_rectangle.png)

![Both loops acquiring on the rectangle: the loop filter's output](symsync-measurements/acq_output_rectangle.png)

![Level and clock offset on the root-raised cosine](symsync-measurements/hold_level_and_offset.png)

See also: [RateSync's timing on a rate cascade](ratesync-timing.md),
[the lock statistic's sizing as shipped](timing_lock_detector.md),
[the loop filter](loop-filter.md), [the NCO](nco.md).
