# DsssReceiver Specifications

- Level: Any
- Nominal frequency: 2.5 GHz
- Frequency uncertainty: +/- 50 kHz
- Frequency rate of change: < 5 kHz/s
- Waveform: Continuous DSSS BPSK
- Waveform exemplary use-case:
    - Code: CCSDS Command link Gold Code 1023 chips repeating
    - Chip rate: 3.069 Mcps
    - Modulation: Asynchronous Rectangular BPSK @ 2700 bps
- Es/N0 >= 3 dB

## Derived: tracking loop bandwidths (all loops: code DLL, Costas/CarrierMpsk carrier, FLL-assist)

- Design target: loop SNR `rho >= 20 dB` at the Es/N0 floor (3 dB), using
  the standard PLL loop-SNR relation `rho(dB) = Es/N0(dB) - 10*log10(2*bn)`
  (`bn` = the loop's own noise bandwidth, normalised to its update rate --
  `doppler.track.LoopFilter`'s own `bn` convention, so the update rate
  cancels out of the relation entirely; it depends only on Es/N0 and `bn`).
- Solving at the floor: `bn <= 10^((EsN0_dB - rho_dB)/10) / 2`
  `= 10^((3 - 20)/10) / 2 = 10^-1.7 / 2 ~= 0.00998`
- **Default rule: `bn <= 0.01` for every tracking loop**, sized against
  the worst-case Es/N0 floor, not a comfortable/typical operating point.
  (Applies to the code loop directly; the code loop's own per-epoch SNR
  is Es/N0 scaled by `1/epochs_per_symbol` -- at this waveform's
  `epochs_per_symbol = (chip_rate/sf)/data_rate = 3000/2700 = 10/9 ~= 1.11`,
  within ~0.5 dB of Es/N0 itself, so the same `bn<=0.01` bound applies
  without a separate derivation.)

## Acquisition

- FFT bin spacing per code epoch (native unambiguous Doppler span,
  ANY coherent depth D -- a D-point slow-time FFT sampled at the
  epoch rate has a fixed +/-(epoch_rate/2) Nyquist range regardless
  of D; more bins only subdivide that SAME fixed range more finely,
  never widen it): `chip_rate / sf = 3.069 Mcps / 1023 = 3.000 kHz`
  exactly (half-span `chip_rate/(2*sf) = 1.500 kHz`).
- **The big one**: required uncertainty is +/-50 kHz = 100 kHz total,
  ~33.3x the 3 kHz native span -> **34 non-overlapping native windows
  needed to cover it** (`ceil(100/3) = 34`). `Acquisition`'s own
  `doppler_uncertainty` parameter cannot help here -- it only NARROWS
  the search within one native span (`doppler_uncertainty <= span` is
  an enforced precondition), it can't widen coverage beyond it.
  Covering the full uncertainty therefore needs 34 independent
  alias-window searches (each its own 2D correlation, `code_bins =
  sf*spc = 2046` at `spc=2`) -- "34 x 2046" -- run either:
  1. **Sequentially** (34x acquisition latency -- almost certainly
     fails the "FAST" requirement), or
  2. **In parallel** (34x compute/hardware, full latency preserved), or
  3. **Behind a DDC channelizer bank** (`doppler.ddc`/`RateConverter`,
     already in the codebase -- reuse, don't reimplement): split the
     +/-50 kHz input into a SMALLER number of wider sub-channels
     first, so each channel's own residual uncertainty fits a cheaper
     per-channel search, trading DDC channelizer cost against fewer
     parallel/sequential full correlation engines.
- **CORRECTED architecture, per direct user redirect ("slow-time
  doesn't work for this case"): pure code-phase search, `D=1`, no
  coherent Doppler-axis combining at all.** The first baseline number
  below used `Acquisition`'s `symbol_rate`-aware auto-config, which
  picked `doppler_bins=31` -- exactly the coherent multi-epoch
  combining this whole story already proved unsafe under continuous
  async data (data-modulation aliasing across the Doppler-bin axis,
  `docs/design/dsss-acquisition.md`'s "ceiling (b) fails hard").
  **`D=1` sidesteps this entirely** -- with only one epoch, there is no
  multi-epoch axis for the data's own spectrum to alias across.
  Detection SNR margin instead comes from **non-coherent accumulation**
  (`n_noncoh`, magnitude-squared summing) across independent epochs --
  provably immune to data-modulation sign flips (already established
  earlier in this story), unlike coherent combining.
- Real measured `pd_predicted` sweep at `D=1`, this waveform's exact
  `cn0_dbhz=37.31`: crosses the 0.9 target around **`n_noncoh=96`**
  (0.917), comfortable margin at **`n_noncoh=128`** (0.965) or
  **`n_noncoh=192`** (0.994). (`pd_predicted` turns non-monotonic and
  unreliable past `n_noncoh~256` in this exact config -- a modeling
  edge case at very large `nc`, not a real detection cliff; stay
  stay well under it -- `n_noncoh<=192` is comfortably clear of it.)
- **Per-frequency-bin dwell time, measured throughput (33.2 MSa/s),
  `code_bins=2046` per epoch**: `n_noncoh=96` -> **5.9 ms**;
  `n_noncoh=128` -> **7.9 ms**; `n_noncoh=192` -> **11.8 ms** -- ALL
  faster than the (now-superseded) D=31 slow-time baseline's 15.3 ms,
  AND with none of its aliasing risk.
- **Architecture: 34 of these `D=1` pure-code-phase searches, one per
  3 kHz-spaced candidate frequency bin spanning +/-50 kHz, run in
  PARALLEL for "fast as possible"** -- total acquisition latency ≈ ONE
  bin's dwell time (**~6-12 ms** depending on the `n_noncoh` margin
  chosen), not 34x it. Sequential would cost 34x (`~200-400 ms`) and is
  ruled out by the "fast as possible" directive. Each of the 34 bins
  needs its own frequency-shifted (down-converted) copy of the input
  feeding an independent code-phase correlator -- realizing that bank
  of 34 parallel down-conversions efficiently (a DDC/mixer bank,
  `doppler.ddc`/`RateConverter`, reuse not reimplement) is the
  remaining open engineering question, not a way to reduce the count
  of 34 -- the count is fixed by `+/-50kHz / 3kHz` regardless.
- **Resolved: how to realize the 34-bin frequency grid per epoch**
  (`bench_freq_bank.py`, real `doppler.spectral.FFT`, real wall-clock
  timing, not just operation-count theory). Two candidates: (A) one
  forward FFT of the received epoch, then roll its spectrum by k bins
  per hypothesis (exact -- the 3 kHz hypothesis spacing IS this
  N=2046-sample epoch's own FFT bin spacing) against one fixed
  precomputed replica spectrum, 1 fwd + 34 inverse FFTs; vs. (B) a
  tuned mixer bank, 34 independent down-conversions each needing its
  own forward FFT, 34 fwd + 34 inverse FFTs. Both cross-checked
  bit-exact-cell correct first (identical injected true k/code-phase
  recovered by both). **(A) wins empirically, consistently, across
  repeated runs: ~0.6-0.8 ms/epoch vs. (B)'s ~0.9-1.0 ms/epoch, a
  1.2x-1.55x speedup** -- directionally confirms the op-count theory
  (35 vs. 68 FFT-equivalents, ~1.94x) but the *measured* margin is
  smaller, because (A) pays an extra O(N) `np.roll` memory copy per
  hypothesis that the theory didn't count, and Python-level per-call
  overhead partially masks the underlying FFT-count gap. **Settled
  architecture: (A), roll the replica/received spectrum, not a tuned
  mixer bank** -- a hybrid (C) was considered but has no analytical
  basis here (code-phase correlation needs the full N=2046-sample
  resolution regardless of how the frequency search is realized, so
  there's no reduced-rate sub-problem for a DDC front-end to help
  with).
- **Open, not yet resolved**: this benchmark measures the per-epoch
  cost of forming the 34-bin correlation grid, not the full
  non-coherent-accumulation search (`n_noncoh` consecutive epochs per
  bin, 34 bins in parallel) end to end in C: real code, real 34-way
  parallel execution, real wall-clock latency at the `n_noncoh=96/128/
  192` operating points above. Also still open: confirm `n_noncoh`
  choice against the occasional epoch that straddles a data-bit
  transition (a graceful per-epoch SNR loss for SOME of the `nc`
  epochs, not a structural mislock, since non-coherent summing doesn't
  alias -- but not yet separately quantified here).
