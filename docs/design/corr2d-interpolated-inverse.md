# Corr2D: decoupled (interpolated) inverse length

**Status:** shipped (PR #241) — `ny_out`/`nx_out`, zero-pad interpolation,
and frequency-domain dwell accumulation are all in
`native/src/corr2d/corr2d_core.c`. §6 (downstream `detector2d`/`acq` wiring)
remains a separate, additive follow-up, not yet done. §9 (single-row-
reference fast path) is a later, independent addition — shipped.
**Scope:** add an optional larger, independently-chosen **inverse** transform
size to `Corr2D` (`native/src/corr2d/corr2d_core.c`) — a pffft-friendly,
malloc-free inverse plus free band-limited interpolation of the correlation
surface — without touching the forward transform. Motivated by the
[DSSS acquisition design](dsss-acquisition.md) §7 (the engine is 2-D-FFT bound,
and real DSSS code lengths are `2ⁿ−1` → the native size is prime).

______________________________________________________________________

## 1. Why

`Corr2D` computes, per frame, `S = FFT2(x)` → `P = S · conj(FFT2(ref))` →
`R = IFFT2(P)/(ny·nx)`. The forward `FFT2` must stay at the code-period size
`(ny, nx)` (the correlation is **circular**; you cannot zero-pad the signal in
time). But the **inverse length is free**: zero-padding the product `P` in the
frequency domain and inverting at any `(ny_out, nx_out) ≥ (ny, nx)` yields the
**band-limited (Dirichlet) interpolation** of the same circular correlation —
peak and value preserved, on a finer grid.

Two wins, no loss:

- **pffft-friendly, malloc-free inverse.** Pick `(ny_out, nx_out)` each a
    multiple of 16 and 5-smooth, so the inverse `FFT2` runs on pffft (pre-allocated
    SIMD buffers) instead of the vendored-pocketfft fallback (which mallocs/frees
    per 1-D transform). Removes ~half the unfriendly FFT work and the inverse's
    per-call allocation.
- **Sub-bin resolution for free.** The output is the correlation on a finer code
    phase (`nx_out`) and Doppler (`ny_out`) grid — sub-chip delay, sub-bin
    Doppler — at no extra forward cost.

It does **not** fix the forward prime-length FFT — that is P2 sub-block's job.
This feature is independent of, and composes with, P2.

______________________________________________________________________

## 2. The math

Frequency-domain zero-padding is exact band-limited interpolation. For the
product `P` of shape `(ny, nx)` and targets `(ny_out, nx_out)`:

```
out = unnorm_IFFT2_{ny_out,nx_out}( zeropad2d(P, ny_out, nx_out) ) / (ny·nx)
```

- **Normalization is the native `1/(ny·nx)`**, *not* `1/(ny_out·nx_out)` — keep
    today's scale so the interpolated peak equals the native peak. (`fft2d_execute`
    is the unnormalized inverse, so the wrapper applies the single `1/(ny·nx)`.)
- **`zeropad2d`** pads each axis independently: keep the low half `[0 … n/2]`,
    insert `n_out − n` zeros at the high (Nyquist) frequencies, then the high half
    `[n/2+1 … n−1]`. **For even `n`, split the Nyquist bin** (`X[n/2] *= 0.5`,
    copy to `X[n_out − n/2]`) — required for minimum-error interpolation.

**Verified** (numpy, 2-D, `nx=31` prime): this matches `scipy.signal.resample`
along both axes to `~1e-13`; `out = (ny, nx)` reproduces the native surface
bit-for-value; the peak lands at `(di·ny_out/ny, dj·nx_out/nx)` with the value
preserved (sub-bin scalloping only when the finer grid straddles the integer
lag).

```
spectrum layout per axis (n -> n_out):

   [ low freqs 0..n/2 ][      zeros (n_out - n)      ][ high freqs n/2+1..n-1 ]
                    ^ split this bin for even n  ----------------^ (its copy)
```

______________________________________________________________________

## 3. API

Add two **optional** output dimensions to the constructor; `0` means "native"
(bit-exact, today's behaviour).

C:

<!-- docs-snippet: skip=API signature sketch (design spec), not a compilable usage example -->

```c
/* ny_out/nx_out: inverse/output size; 0 => use ny/nx. Must be >= ny/nx. */
corr2d_state_t *corr2d_create(const float complex *ref, size_t ny, size_t nx,
                              size_t ny_out, size_t nx_out,
                              size_t dwell, int nthreads);
size_t corr2d_execute(corr2d_state_t *state, const float complex *in,
                      size_t n_in, float complex *out);   /* writes ny_out*nx_out */
size_t corr2d_execute_max_out(corr2d_state_t *state);     /* == ny_out*nx_out */
```

Manifest (`objects/corr2d.toml`) — two new init params after `nx`, default `0`:

```toml
[[corr2d.init_params]]
name = "ny_out"
type = "size_t"
default = "0"   # 0 => ny (native)
[[corr2d.init_params]]
name = "nx_out"
type = "size_t"
default = "0"   # 0 => nx (native)
```

`execute` is already `variable_output`, so the binding sizes the returned array
to `corr2d_execute_max_out` = `ny_out*nx_out` automatically. New read-only
properties `ny_out`, `nx_out`. Python: `Corr2D(ref, dwell=1, ny_out=0, nx_out=0, nthreads=1)` — the `(ny, nx)`-shaped `execute` input is unchanged; the returned
surface is `(ny_out, nx_out)`.

______________________________________________________________________

## 4. State + algorithm

**Frequency-domain accumulation** (✅ implemented) and **the larger,
decoupled inverse** (✅ also implemented — `ny_out`/`nx_out`): accumulate the
product `P` over `dwell` frames, then (zero-pad +) invert **once per dump**
instead of inverting every frame.

**When this is valid.** The deferral relies on the inverse DFT being **linear**,
`Σₖ IFFT(Pₖ) = IFFT(Σₖ Pₖ)`, with the single `1/n` applied once either way. The
load-bearing requirement is that the per-dump combination is **coherent — a
complex (linear) sum**. A **non-coherent** dump (`Σₖ |IFFT(Pₖ)|²`, a
magnitude/energy sum) is *nonlinear* and **cannot** defer the inverse: it must
transform each frame and accumulate magnitudes. So this optimization is specific
to corr2d's coherent `dwell`; any future non-coherent integration (the
acquisition `N_noncoh`) inverts per frame. A single inverse plan + normalization
across the dwell is the only other condition (trivially met — the grid and `1/n`
are constant). Equivalence is exact in real arithmetic; in cf32 it differs from
the per-frame sum only by accumulation-order rounding (~1e-5 relative).

It also composes with the interpolated inverse: zero-padding is linear too, so
`zeropad(Σ Pₖ)` then one inverse is the natural home for the pad.

State (sizes): `fwd` plan `(ny,nx)`; **`inv` plan `(ny_out,nx_out)`**; `ref_spec`,
`work_fft`, `accum_P` all `(ny,nx)`; **`work_pad`, `work_ifft` `(ny_out,nx_out)`**;
`n = ny·nx`, `n_out = ny_out·nx_out`.

```
corr2d_execute(in):
    FFT2_{ny,nx}(in)        -> work_fft          # forward (native, unchanged)
    work_fft *= ref_spec                          # product P            (ny,nx)
    accum_P  += work_fft ;  count++               # coherent accum in FREQ domain
    if count == dwell:
        zeropad2d(accum_P, ny_out, nx_out) -> work_pad     # §2, Nyquist-split
        IFFT2_{ny_out,nx_out}(work_pad)    -> work_ifft    # inverse (friendly)
        out[k] = work_ifft[k] / (ny·nx)           # native normalization
        clear accum_P ; count = 0 ; return n_out
    return 0
```

`corr2d_create`: `ny_out = ny_out ? ny_out : ny` (same for `nx_out`); validate
`ny_out ≥ ny`, `nx_out ≥ nx`; build `inv` at `(ny_out, nx_out)`; allocate the
`n_out` buffers. `corr2d_reset` clears `accum_P`/`count`. The `zeropad2d` helper
(per-axis low/zeros/high with even-`n` Nyquist split) is a small internal static.

______________________________________________________________________

## 5. Constraints / gotchas

- **`(ny_out, nx_out) ≥ (ny, nx)`.** Downsizing is not interpolation; reject it.
- **Pick pffft-friendly out dims** (16-multiple, 5-smooth) or the feature buys
    nothing — the whole point is to land the inverse on pffft. A non-friendly
    `ny_out`/`nx_out` just makes a bigger fallback FFT.
- **Forward is still native.** This fixes only the inverse; the `2ⁿ−1` forward FFT
    remains on the fallback until P2 sub-block.
- **Even-`n` Nyquist split** is mandatory (§2) — skipping it adds a small
    interpolation bias.
- **Memory** grows from `n` to `n_out` for `work_pad`/`work_ifft`/output (still
    tiny: `n_out·8 B`).
- **Output indexing:** peak `(row, col)` is on the `(ny_out, nx_out)` grid →
    Doppler `= row·ny/ny_out`, code phase `= col·nx/nx_out` (sub-bin).

______________________________________________________________________

## 6. Downstream (detector2d / acq)

`detector2d` and `acq` own a `corr2d` and run argmax + CFAR on its output. When
they pass through `ny_out`/`nx_out`:

- the surface, magnitude buffer, CFAR scratch, and peak decomposition use
    `(ny_out, nx_out)`;
- the reported `(doppler_bin, code_phase)` are on the finer grid (map back with
    `ny/ny_out`, `nx/nx_out`), giving sub-chip / sub-bin acquisition estimates;
- `acq`'s `carrier_for_bin` and the expected-hit math scale by the grid ratio.

This is a separate, additive change (own follow-up); the `corr2d` feature lands
first and is useful on its own (e.g. interpolated correlation peaks).

______________________________________________________________________

## 7. Tests / acceptance

- **Bit-exact native:** `ny_out=nx_out=0` (or `=ny,nx`) reproduces today's output
    on the existing `corr2d` C + Python tests.
- **Interpolation correctness:** for a known 2-D circular shift, the interpolated
    peak matches `scipy.signal.resample` of the native surface to `~1e-12`, with
    the peak at `(di·ny_out/ny, dj·nx_out/nx)`.
- **pffft engaged:** with friendly out dims the inverse takes the pffft path —
    assert the malloc-free, faster transform (no per-call allocation; `bench_corr2d`
    shows the friendly-inverse throughput vs the native-prime baseline).
- **Freq-domain accumulation = time-domain:** `dwell>1` output identical to a
    reference that inverts every frame and sums.

______________________________________________________________________

## 8. Phasing

A **P0/P1 baseline kernel feature** — clean, loss-free, independent of the
sub-block work. It makes the inverse friendly and hands the engine sub-bin
resolution; **P2 sub-block** then makes the *forward* friendly for `2ⁿ−1` codes.
Together: both transforms pffft, interpolated output.

______________________________________________________________________

## 9. Single-row-reference fast path (shipped)

**Status:** shipped. **Scope:** skip the row axis of the 2-D FFT pair
entirely when it provably contributes nothing — a `corr2d`-level
optimization, independent of §1-8's forward/inverse split and orthogonal to
P2 (prime-length forward FFT).

**Why.** Both real callers of `corr2d` (`acq_core.c`, `detector2d_core.c`)
build a reference with energy only in row 0 (`build_ref`'s own docstring:
"the flat-in-slow-time row spectrum ... turns the row axis into a
pass-through" — `acq_core.c` already relies on this, doing its own
Doppler-axis FFT by hand before calling `corr2d_execute`). For such a
reference, `ref_spec[u,v] = conj(FFT_nx(ref_row0))[v]` is independent of the
row-frequency index `u`. DFT orthogonality then makes the row axis of the
forward-accumulate-inverse round trip an **exact identity** for *any* row
content — not an approximation, not specific to the impulse/delta cases
the existing test suite happened to check:

```
R(i,j) = (1/nx) * IFFT_nx( FFT_nx(row_i_of_input) * conj(FFT_nx(ref_row0)) )(j)
```

i.e. `corr2d_execute`, for a single-row reference with `ny_out == ny` (no
Doppler-axis interpolation — see below), reduces to `ny` independent
length-`nx` circular cross-correlations. The general 2-D path was silently
paying for a full `(ny,nx)` FFT2 forward *and* inverse every frame when
only the `nx`-axis (code) transform does real work.

**Eligibility** (decided once, at `corr2d_create`, fixed for the object's
lifetime): reference nonzero only in row 0, **and** `ny_out == ny`. The
`ny_out == ny` condition is required because the identity relies on the
forward and inverse row-axis transforms being the *same length* — a
caller requesting Doppler-axis interpolation (`ny_out > ny`, a documented
`Corr2D` feature, unused by any caller today) genuinely needs the row
axis's content and must fall back to the general path. `nx_out != nx`
(code-axis interpolation) composes fine with the fast path — it's a pure
per-row zero-pad (`_zeropad_1d`, reused unchanged) before each row's
inverse.

**Implementation** (`native/src/corr2d/corr2d_core.c`): `_is_single_row_ref`
detects eligibility at `create`/`set_ref` time; the fast branch replaces
the `(ny,nx)` `fft2d_state_t` plans with a pair of length-`nx`/`nx_out`
`fft_state_t` 1-D plans and a length-`nx` `row_ref_spec` (replacing the
full `(ny,nx)` `ref_spec` — smaller, not larger). `corr2d_execute` branches
on `state->fast_path` at the top; the general path is completely
untouched. `corr2d_set_ref` now returns `int` (0/-1): on a fast-path
object it rejects a subsequently-supplied non-single-row reference rather
than silently truncating it — mode is fixed for the object's lifetime, not
re-derived per call.

**Serialization is unaffected.** `accum`/`work_fft` keep their existing
`(ny,nx)`-sized allocation in both modes — the fast path just reinterprets
them as `ny` independent length-`nx` row spectra instead of one flat 2-D
spectrum. Same byte count, same layout, so `corr2d_state_bytes`/
`get_state`/`set_state` needed zero changes.

**Measured** (see `native/benchmarks/bench_corr2d_core.c`,
`ny=16, nx=2046` — the acquisition grid from §1/`dsss-acquisition.md` §7):
single-row (fast path) ~86 MSa/s vs. multi-row (general path) ~54 MSa/s at
that grid. The much larger end-to-end win is in the small-grid regime
`acq_push` actually runs at (`ny=16, nx=14`): the composed `DsssReceiver`'s
"search" (acquisition) benchmark went from ~28-59 MSa/s to ~133 MSa/s,
now roughly matching its "track" regime (~135 MSa/s) instead of trailing
it 3-5x — `corr2d_execute` was ~75% of `acq_push`'s per-frame cost, split
almost evenly between the forward and inverse FFT2, confirming the row
axis really was paying for a full transform pair every frame.

**Tests** (`native/tests/test_corr2d_core.c`): every pre-existing test
already used a single-row reference, so they all exercise the fast path
automatically (free regression coverage). Added: a brute-force dense-signal
correctness check for *both* paths (existing coverage was impulse/shift-
only, which passes through almost any correlator trivially); fast path +
`nx_out` interpolation; the `ny_out > ny` fallback (asserts `fast_path==0`
even for a single-row reference); `corr2d_set_ref` accept/reject; a
fast-path state round-trip. Sanity-break-and-revert confirmed both eligibility
conditions are load-bearing: forcing `fast_path=0` unconditionally left
`acq`/`detector2d`/`DsssReceiver`'s own correctness tests passing (they
don't depend on which path runs); forcing `fast_path=1` on a genuinely
multi-row reference made the new dense-signal test fail cleanly (no crash,
no silent corruption).

## See also

- [DSSS acquisition design](dsss-acquisition.md) — §7 (FFT-bound, the forward vs
    inverse split this implements).
- [`Corr2D` / 2-D Acquisition gallery](../gallery/detection2d.md).
