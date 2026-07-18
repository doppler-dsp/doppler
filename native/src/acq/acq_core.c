/**
 * @file acq_core.c
 * @brief Streaming DSSS burst-acquisition engine implementation.
 *
 * Owns the full receive-side acquisition pipeline: a raw cf32 ring, the
 * slow-time Doppler FFT, a single-row-reference 2-D code correlator (corr2d),
 * and a CFAR threshold gate whose grid (coherent depth, threshold,
 * non-coherent looks) is auto-configured from the physics — C/N0, chip rate,
 * and a target (Pfa, Pd) — using the detection-theory functions.  The CFAR
 * helpers
 * (_noise_estimate, _ring_create) are reused header-only from det_private.h —
 * no link to detector_core.
 */

#include "acq/acq_core.h"
/* det_private.h needs det_noise_mode_t (from detector2d_core.h, included
 * above) and supplies the header-only _ring_create / _noise_estimate statics.
 */
#include "detector/det_private.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* sqrt(2/pi): maps the raw Rayleigh threshold eta into mean-estimator units,
 * since E|Rayleigh(1)| = sqrt(pi/2).  Its reciprocal recovers eta from the
 * mean-based test statistic for the per-sample SNR estimate. */
#define ACQ_SQRT_2_OVER_PI 0.7978845608028654f

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Converts a per-sample amplitude SNR back to an estimated C/N0 (dB-Hz) --
 * the exact inverse of acq_create()'s sizing transform
 * (snr = sqrt(10^(cn0_dbhz/10) / fs)) -- so the reported figure is directly
 * comparable to the caller's own cn0_dbhz argument regardless of chip_rate
 * or spc, unlike a raw per-sample or coherently-integrated ratio (both scale
 * with the frame geometry and so aren't portable across configurations).
 * amp_snr is always > 0 here: every call site sits behind a
 * test_stat > threshold gate, and threshold/eta_nc are always positive. */
static inline float
_cn0_dbhz_from_amp_snr (float amp_snr, double fs)
{
  return (float)(20.0 * log10 ((double)amp_snr) + 10.0 * log10 (fs));
}

/* Doppler-band mask: with a doppler_uncertainty prior the engine scans only
 * searched_bins rows centred on DC, so the peak search must match (else the
 * lowered Bonferroni threshold over-counts and realized Pfa exceeds target).
 * Returns 1 if the cell at flat index k is inside the scanned band.  The
 * folded distance |k_row| from DC (rows >doppler_bins/2 are negative Doppler)
 * must not exceed half = (searched_bins-1)/2.  When searched_bins ==
 * doppler_bins this admits every row (byte-identical to the full search). */
static inline int
_in_doppler_band (const acq_state_t *st, size_t k)
{
  /* Wideband mode always searches its whole n_freq_bins grid by
   * construction (there is no further-narrowing prior beyond the windows
   * n_freq_bins itself already tiles) -- every hypothesis is in-band.
   * Bypassing the native fold formula also sidesteps its even-D Nyquist-row
   * exclusion quirk (irrelevant here: n_freq_bins isn't a slow-time FFT
   * length, so that formula's assumptions don't apply to it anyway). */
  if (st->n_freq_bins > 1)
    return 1;
  const size_t row = k / st->code_bins;
  const size_t fold
      = (row <= st->doppler_bins - row) ? row : st->doppler_bins - row;
  return fold <= (st->searched_bins - 1) / 2;
}

/* Peak, CFAR noise, and test statistic from the coherent dump in out_buf. */
static void
_compute_stat (acq_state_t *st)
{
  const size_t n = st->n;

  for (size_t k = 0; k < n; k++)
    st->mag_buf[k] = cabsf (st->out_buf[k]);

  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (st->mag_buf[k] > st->mag_buf[peak] && _in_doppler_band (st, k))
      peak = k;

  st->peak_row = peak / st->code_bins;
  st->peak_col = peak % st->code_bins;
  st->peak_mag = st->mag_buf[peak];

  st->noise_est = _noise_estimate (st->mag_buf, st->noise_lo, st->noise_hi,
                                   st->noise_scratch, st->noise_mode);

  st->test_stat
      = (st->noise_est > 0.0f) ? (st->peak_mag / st->noise_est) : 0.0f;
}

/* Non-coherent CFAR: peak + normalized order-N_nc statistic from nc_surface
 * (the per-cell sum of |dump|^2 over n_noncoh coherent looks).
 *
 * Under H0 each look's |z|^2 = sigma^2 * chi2(2)/2, so nc_surface ~
 * (sigma^2/2)*chi2(2*n_noncoh).  Estimating the per-look power sigma^2 from
 * the mean reference-cell power (noise_pow / n_noncoh), the normalized
 * statistic R = sqrt(2 * n_noncoh * peak / noise_pow) has P(R > b) =
 * marcum_q(n_noncoh, 0, b) under H0 — exactly the order-N_nc threshold eta_nc.
 * Validated against Monte-Carlo to <1% (det_pd_noncoherent tests). */
static void
_compute_stat_nc (acq_state_t *st)
{
  const size_t n = st->n;

  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (st->nc_surface[k] > st->nc_surface[peak] && _in_doppler_band (st, k))
      peak = k;

  st->peak_row = peak / st->code_bins;
  st->peak_col = peak % st->code_bins;
  st->peak_mag = sqrtf (st->nc_surface[peak]);

  float noise_pow
      = _noise_estimate (st->nc_surface, st->noise_lo, st->noise_hi,
                         st->noise_scratch, st->noise_mode);
  st->noise_est = sqrtf (noise_pow);
  st->test_stat = (noise_pow > 0.0f)
                      ? sqrtf (2.0f * (float)st->n_noncoh
                               * st->nc_surface[peak] / noise_pow)
                      : 0.0f;
}

/* Scanned Doppler bins for a coherent depth D under a doppler_uncertainty
 * prior du (Hz, one-sided half-range).  The slow-time FFT always spans the
 * full +/- span = chip_rate/(2*sf); a tighter du restricts the search to the
 * central band, shrinking the Bonferroni cell count.  Returns an odd count
 * (2*half+1) centred on DC, capped at D; du <= 0 (or >= span) means full. */
static size_t
_searched_bins (size_t D, double du, double span)
{
  if (du <= 0.0 || du >= span)
    return D;
  double frac = du / span; /* fraction of half-range */
  size_t half = (size_t)ceil (0.5 * frac * (double)D);
  size_t sb   = 2 * half + 1;
  return (sb > D) ? D : sb;
}

/* Mean of sinc(u) = sin(pi*u)/(pi*u) over u in [0, umax] — equals
 * Si(pi*umax)/(pi*umax).  64-interval Simpson: setup path, and the
 * quadrature error (~1e-12 here) is far below the model's own fidelity. */
static double
_mean_sinc (double umax)
{
  if (umax <= 0.0)
    return 1.0;
  const int n = 64;
  double    h = umax / (double)n;
  double    s = 1.0; /* sinc(0) */
  for (int i = 1; i <= n; i++)
    {
      double u = h * (double)i;
      double v = sin (M_PI * u) / (M_PI * u);
      s += (i == n) ? v : ((i & 1) ? 4.0 : 2.0) * v;
    }
  return s * h / (3.0 * umax);
}

/* Mean amplitude derating of the correlation peak from grid straddle — the
 * gap between the on-grid best case det_pd() sees and the operating average
 * the Monte-Carlo characterization measures.  Three independent losses, each
 * averaged over a uniform prior across its straddle range:
 *
 *  - slow-time scalloping: a Doppler off the FFT bin centre by delta in
 *    [-1/2, 1/2] bins loses the Dirichlet factor, ~ sinc(delta) for the
 *    depths used here (absent when D == 1: no slow-time FFT, no bins);
 *  - intra-segment rotation: the slow-time FFT compensates rotation
 *    BETWEEN segments only; the residual rotation across one segment,
 *    u = f*code_bins cycles, loses sinc(u).  f spans the searched band, so
 *    u spans +/- sb/(2*D) — this is the band-edge loss that dominates a
 *    full-span search (sinc(1/2) = -3.9 dB at the edge);
 *  - code-phase straddle: the true code phase lands up to half a sample
 *    off the grid, 1/(2*spc) chips, losing the triangular-autocorrelation
 *    factor (1 - delta_chip); mean = 1 - 1/(4*spc).
 */
/* Intra-segment rotation range: the residual u = f*code_bins spans the
 * TIGHTER of the searched band (sb bins of width 2*span/D) and the
 * doppler_uncertainty prior itself — _searched_bins rounds the bin count
 * up (and clamps at D >= sb >= 1), so sb/D alone over-states the range
 * whenever the prior is tighter than the grid, structurally so at D == 1
 * where sb is pinned to 1 (up to ~1.2 dB of phantom loss otherwise). */
static double
_intra_umax (size_t D, size_t sb, double du, double span)
{
  double frac = (du > 0.0 && du < span) ? du / span : 1.0;
  double grid = (double)sb / (double)D;
  return 0.5 * (frac < grid ? frac : grid);
}

static double
_straddle_loss (size_t D, size_t sb, size_t spc, double du, double span)
{
  double l_scallop = (D > 1) ? _mean_sinc (0.5) : 1.0;
  double l_intra   = _mean_sinc (_intra_umax (D, sb, du, span));
  double l_code    = 1.0 - 1.0 / (4.0 * (double)spc);
  return l_scallop * l_intra * l_code;
}

static double
_sinc (double u)
{
  return (u == 0.0) ? 1.0 : sin (M_PI * u) / (M_PI * u);
}

/* Average Pd over the straddle priors — Pd(snr) is concave over the loss
 * range, so Pd at the MEAN amplitude overstates the mean Pd (Jensen; ~0.11
 * at a marginal design point). Midpoint quadrature over the three
 * independent uniform factors (8 x 8 x 4 = 256 det_pd evaluations, setup
 * path only; each factor is symmetric, so half-ranges suffice). nc == 1
 * evaluates the coherent path, nc > 1 the non-coherent one. */
static double
_mean_pd (double snr, size_t D, double umax, size_t spc, int n, double eta,
          int nc)
{
  const int nd = 8, nu = 8, nk = 4;
  double    acc = 0.0;
  for (int i = 0; i < nd; i++)
    {
      double ls = (D > 1) ? _sinc (0.5 * ((double)i + 0.5) / (double)nd) : 1.0;
      for (int j = 0; j < nu; j++)
        {
          double li = _sinc (umax * ((double)j + 0.5) / (double)nu);
          for (int k = 0; k < nk; k++)
            {
              double lc
                  = 1.0 - (0.5 / (double)spc) * ((double)k + 0.5) / (double)nk;
              double se = snr * ls * li * lc;
              acc += (nc > 1) ? det_pd_noncoherent (se, n, nc, eta)
                              : det_pd (se, n, eta);
            }
        }
    }
  return acc / (double)(nd * nu * nk);
}

/* ── Data-modulation self-cancellation model (symbol_rate > 0)
 * ────────────
 *
 * A continuous data-modulated signal's bit transitions land inside a
 * coherent window at a phase uniformly distributed relative to the window,
 * splitting it into segments that each carry one (unknown, i.i.d. +-1) data
 * sign -- a loss the Doppler/code-phase straddle model above knows nothing
 * about.  This ports the semi-analytical model validated in
 * src/doppler/examples/dsss_acq_async_data_demo.py's _theoretical_pd (see
 * its docstring for the derivation and the empirical cross-check: <0.01
 * max Pd error for the non-coherent case) into the sizing engine itself,
 * generalized from that script's two special cases (coherent-only,
 * non-coherent-of-single-epoch-looks) to an arbitrary D-epoch-look x
 * n_noncoh-looks grid.
 */

#define ACQ_DM_NPHI 8U              /* window-phase quadrature nodes */
#define ACQ_DM_EXACT_MAX_SYMBOLS 6U /* <=64 combos: exact sign enumeration */
#define ACQ_DM_MC_SAMPLES 64U       /* sign draws beyond the exact cap */

/* One run of a coherent window sharing a single (unknown) data symbol. */
typedef struct
{
  double len;   /* Fractional epochs this run spans. */
  size_t sym;   /* Which data symbol (0-based) it belongs to. */
  size_t epoch; /* Which epoch (0 .. depth-1) it belongs to. */
} _dm_run_t;

/* Segment a `depth`-epoch window starting at symbol-phase phi0 (elapsed
 * into the current data symbol, in epochs) into runs of (length, symbol,
 * epoch) -- mirrors _window_epoch_segments() in the gallery script.  Pure
 * geometry: independent of the (unknown) data signs, so it is computed
 * once per phi0 and reused across every sign combination tried at that
 * phase.  `runs` must have capacity >= max_runs (caller-bounded). */
static size_t
_dm_segment_window (double phi0, size_t depth, double epochs_per_symbol,
                    _dm_run_t *runs, size_t max_runs, size_t *n_symbols)
{
  double phase     = phi0;
  size_t symbol_id = 0;
  size_t nruns     = 0;
  for (size_t e = 0; e < depth; e++)
    {
      double remaining = 1.0;
      while (remaining > 1e-9)
        {
          double to_boundary = epochs_per_symbol - phase;
          double take = (to_boundary < remaining) ? to_boundary : remaining;
          if (nruns < max_runs)
            {
              runs[nruns].len   = take;
              runs[nruns].sym   = symbol_id;
              runs[nruns].epoch = e;
              nruns++;
            }
          remaining -= take;
          phase += take;
          if (phase >= epochs_per_symbol - 1e-9)
            {
              phase = 0.0;
              symbol_id++;
            }
        }
    }
  *n_symbols = symbol_id + 1;
  return nruns;
}

/* Fill v[0 .. depth-1] (real, signed per-epoch sums) from the precomputed
 * runs and a sign assignment (bit i of `bits` -> symbol i's sign). */
static void
_dm_apply_signs (const _dm_run_t *runs, size_t nruns, size_t depth,
                 uint64_t bits, double *v)
{
  for (size_t e = 0; e < depth; e++)
    v[e] = 0.0;
  for (size_t k = 0; k < nruns; k++)
    {
      double sign = ((bits >> runs[k].sym) & 1u) ? 1.0 : -1.0;
      v[runs[k].epoch] += sign * runs[k].len;
    }
}

/* Peak-of-D-point-DFT magnitude / D for a real length-D sequence -- the
 * coherent per-look gain fraction under a mid-window data transition.  The
 * engine's slow-time combine is a D-point Doppler FFT: a phase step
 * mid-window leaks energy into every bin, not just the near-zero one, and
 * detection only needs the peak bin to cross threshold.  Direct O(D^2) DFT
 * -- D is the coherent-depth search ceiling, physically small (tens at
 * most); this runs at construction time, never in a hot loop. */
static double
_dm_group_alpha (const double *v, size_t D)
{
  if (D == 1)
    return fabs (v[0]);
  double peak = 0.0;
  for (size_t k = 0; k < D; k++)
    {
      double re = 0.0, im = 0.0;
      for (size_t t = 0; t < D; t++)
        {
          double ang = -2.0 * M_PI * (double)k * (double)t / (double)D;
          re += v[t] * cos (ang);
          im += v[t] * sin (ang);
        }
      double mag = sqrt (re * re + im * im);
      if (mag > peak)
        peak = mag;
    }
  return peak / (double)D;
}

/* Deterministic xorshift64: a local, fixed-seed PRNG for the Monte-Carlo
 * sign-combo fallback (never the shared RNG state of anything else), so
 * acq_create()'s sizing decision stays reproducible. */
static uint64_t
_dm_xorshift64 (uint64_t *s)
{
  uint64_t x = *s;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *s = x;
  return x;
}

/* Semi-analytical Pd for a grid of n_noncoh non-coherently combined looks,
 * each a D-epoch coherent block (n_noncoh == 1: pure coherent), under a
 * uniformly-distributed data-bit transition relative to the D*n_noncoh
 * -epoch window.  Averages over window phase (quadrature) and the i.i.d.
 * +-1 data signs the window touches (exact enumeration for a small symbol
 * count, capped Monte Carlo beyond that).  Generalizes
 * dsss_acq_async_data_demo.py's _theoretical_pd (validated there only at
 * n_noncoh == 1 and D == 1) to an arbitrary (D, n_noncoh): each look's own
 * D-point-DFT-peak amplitude feeds a non-centrality sum across looks,
 * exactly like summing independent non-central chi-squared(2) terms into
 * marcum_q's non-central chi-squared(2*n_noncoh) -- reduces to the
 * validated D-only case at n_noncoh == 1 and to the validated
 * single-epoch-look case at D == 1. */
static double
_data_mod_pd (size_t D, size_t n_noncoh, double epochs_per_symbol, double snr,
              size_t cb, double eta)
{
  const size_t depth = D * n_noncoh;
  const size_t max_runs
      = (size_t)((double)depth / fmax (epochs_per_symbol, 1e-6)) + depth + 4;

  _dm_run_t *runs = (_dm_run_t *)malloc (max_runs * sizeof (*runs));
  double    *v    = (double *)malloc (depth * sizeof (*v));
  if (!runs || !v)
    {
      free (runs);
      free (v);
      return 0.0; /* allocation failure: treat as "doesn't meet pd" */
    }

  double   pd_acc = 0.0;
  uint64_t rng    = 0x9E3779B97F4A7C15ull; /* fixed seed: reproducible */
  for (size_t p = 0; p < ACQ_DM_NPHI; p++)
    {
      double phi0
          = ((double)p + 0.5) / (double)ACQ_DM_NPHI * epochs_per_symbol;
      size_t n_symbols = 0;
      size_t nruns = _dm_segment_window (phi0, depth, epochs_per_symbol, runs,
                                         max_runs, &n_symbols);

      int    exact    = n_symbols <= ACQ_DM_EXACT_MAX_SYMBOLS;
      size_t n_combos = exact ? ((size_t)1 << n_symbols) : ACQ_DM_MC_SAMPLES;

      double pd_sum = 0.0;
      for (size_t c = 0; c < n_combos; c++)
        {
          uint64_t bits = exact ? (uint64_t)c : _dm_xorshift64 (&rng);
          _dm_apply_signs (runs, nruns, depth, bits, v);

          if (n_noncoh <= 1)
            {
              double alpha = _dm_group_alpha (v, D);
              pd_sum += det_pd (snr * alpha, (int)(D * cb), eta);
            }
          else
            {
              double lam = 0.0;
              for (size_t g = 0; g < n_noncoh; g++)
                {
                  double alpha = _dm_group_alpha (v + g * D, D);
                  double a     = sqrt (2.0 * (double)(D * cb)) * snr * alpha;
                  lam += a * a;
                }
              pd_sum += marcum_q ((int)n_noncoh, sqrt (lam), eta);
            }
        }
      pd_acc += pd_sum / (double)n_combos;
    }

  free (runs);
  free (v);
  return pd_acc / (double)ACQ_DM_NPHI;
}

/* Derive and commit the threshold ladder (searched_bins / pfa_cell / eta /
 * eta_nc / threshold / straddle_loss / pd_predicted / underpowered) for the
 * grid already set on st (st->doppler_bins / st->n_noncoh), given the sizing
 * physics.  Shared by _auto_config (both its paths) and
 * acq_configure_search_raw, so a caller-pinned grid gets exactly the same
 * threshold derivation an auto-sized one would.  On the data-modulation path
 * (st->epochs_per_symbol > 0) Pd is evaluated via _data_mod_pd instead of
 * _mean_pd -- see _data_mod_pd's doc for why that model isn't folded into
 * the straddle-loss quadrature. */
static void
_commit_thresholds (acq_state_t *st, double pfa, double pd, double snr,
                    double du)
{
  const size_t cb   = st->code_bins;
  const double span = st->doppler_span_hz;
  const size_t D    = st->doppler_bins;
  const size_t nc   = st->n_noncoh;

  /* Wideband mode already tiles the FULL requested uncertainty with
   * n_freq_bins windows (no further within-mode narrowing prior exists), so
   * every window is "searched" by construction -- unlike the native
   * du-narrows-the-native-span case _searched_bins models. */
  st->searched_bins
      = (st->n_freq_bins > 1) ? st->n_freq_bins : _searched_bins (D, du, span);
  st->doppler_res_hz = st->chip_rate / ((double)st->sf * (double)D);
  st->pfa_cell = 1.0 - pow (1.0 - pfa, 1.0 / (double)(st->searched_bins * cb));
  double umax  = _intra_umax (D, st->searched_bins, du, span);
  st->straddle_loss = _straddle_loss (D, st->searched_bins, st->spc, du, span);
  st->eta           = (float)det_threshold (st->pfa_cell);

  if (nc > 1)
    {
      double e      = det_threshold_noncoherent (st->pfa_cell, (int)nc);
      st->eta_nc    = (float)e;
      st->threshold = 0.0f; /* coherent gate unused on the non-coherent path */
      st->pd_predicted
          = (st->epochs_per_symbol > 0.0)
                ? _data_mod_pd (D, nc, st->epochs_per_symbol, snr, cb, e)
                : _mean_pd (snr, D, umax, st->spc, (int)st->n, e, (int)nc);
    }
  else
    {
      st->eta_nc    = 0.0f;
      st->threshold = st->eta * ACQ_SQRT_2_OVER_PI;
      st->pd_predicted
          = (st->epochs_per_symbol > 0.0)
                ? _data_mod_pd (D, 1, st->epochs_per_symbol, snr, cb, st->eta)
                : _mean_pd (snr, D, umax, st->spc, (int)st->n, st->eta, 1);
    }
  st->underpowered = (uint8_t)(st->pd_predicted < pd);
}

/* Search the grid from the physics: WHICH (doppler_bins, n_noncoh) to use,
 * written to *out_d / *out_nc.  Deliberately does not touch st (not even
 * st->doppler_bins/st->n/st->n_noncoh) and does not allocate anything --
 * the caller commits the decision via _regrid() (which needs to see the
 * PRIOR grid to know what changed) and then _commit_thresholds() (which
 * needs the grid _regrid() just committed).  Search only; no side effects.
 *
 * st->epochs_per_symbol <= 0 (no known data-modulation clock): picks the
 * smallest coherent depth doppler_bins in [1, reps] whose doppler_bins*
 * code_bins coherent samples meet Pd at the (doppler_uncertainty-shrunk)
 * Bonferroni threshold; if the full coherent ceiling still falls short,
 * adds non-coherent looks (up to max_noncoh).  Unchanged from the original
 * single-axis search -- byte-identical sizing for every caller that doesn't
 * opt into symbol_rate.
 *
 * st->epochs_per_symbol > 0: a data-bit transition landing mid-coherent
 * -epoch splits the coherent sum (see _data_mod_pd) -- a loss the model
 * above can't see and can silently under-size for.  Jointly searches
 * doppler_bins in [1, reps] x non-coherent looks in [1, max_noncoh],
 * pricing that loss honestly.
 *
 * st->doppler_resolution <= 0 (default): picks the grid meeting Pd with the
 * fewest total epochs (ties favor smaller doppler_bins, which also lowers
 * mislock risk -- see docs/gallery/dsss-acq-async-data.md).  This is a full
 * reps x max_noncoh sweep of the O(doppler_bins^2) _data_mod_pd model --
 * fine at the small reps this path was designed for.
 *
 * st->doppler_resolution > 0: floors doppler_bins at
 * ceil(chip_rate/(sf*doppler_resolution)) (clipped to [1, reps]) and scans
 * UP from there, taking the first (doppler_bins, n_noncoh) that meets Pd
 * (smallest n_noncoh first, so still the cheapest grid at that depth) --
 * trading the fewest-total-epochs guarantee for a resolution guarantee.
 * This is the difference between an O(reps) and an O(reps^3) construction:
 * scanning the full range from 1 pays the O(D^2) DFT inside _data_mod_pd for
 * every D up to reps, so raising reps to reach a fine doppler_resolution
 * makes the unfloored sweep cubic in reps; starting at the floor and
 * stopping at the first success touches only a handful of D near it,
 * independent of reps.
 *
 * st->doppler_rate > 0: caps the coherent-depth ceiling (both the loop's
 * upper bound and doppler_resolution's floor) at the largest D whose
 * in-window Doppler drift (doppler_rate * D * epoch duration) stays under
 * one resolution bin (chip_rate/(sf*D)) -- solving for D gives
 * D < chip_rate/(sf*sqrt(doppler_rate)).  This only ever tightens the
 * search space beyond reps; it never widens it.
 *
 * du > span (wideband): short-circuits both paths above entirely -- see the
 * file doc comment's "Wideband mode" section. doppler_bins is forced to 1
 * (a coherent multi-epoch axis can't combine with the frequency-window
 * roll-bank -- and isn't safe under continuous async data regardless, the
 * whole reason this engine ever prefers a smaller D) and
 * *out_freq_bins = ceil(du/span) tiles the requested uncertainty. Only
 * n_noncoh needs sizing: ascends from 1, pricing the FULL n_freq_bins*cb
 * Bonferroni cell count, using whichever Pd model (data-modulation-aware or
 * plain) epochs_per_symbol already selects -- same physics, just evaluated
 * once at the fixed D=1/n_freq_bins grid instead of searched over D. */
static void
_auto_config (const acq_state_t *st, double pfa, double pd, double snr,
              double du, size_t *out_d, size_t *out_nc, size_t *out_freq_bins)
{
  const size_t cb   = st->code_bins;
  const double span = st->doppler_span_hz;

  if (du > span)
    {
      const size_t n_freq_bins = (size_t)ceil (du / span);
      const size_t sb          = n_freq_bins;
      const double umax        = _intra_umax (1, sb, du, span);
      const double pc = 1.0 - pow (1.0 - pfa, 1.0 / (double)(sb * cb));
      const double sloss = _straddle_loss (1, sb, st->spc, du, span);

      int    k = det_n_noncoh (snr * sloss, (int)cb, pd, pc,
                               (int)st->max_noncoh);
      size_t nc = (k > 0) ? (size_t)k : st->max_noncoh;
      while (nc < st->max_noncoh)
        {
          double e = (nc > 1) ? det_threshold_noncoherent (pc, (int)nc)
                              : det_threshold (pc);
          double pdd = (st->epochs_per_symbol > 0.0)
                          ? _data_mod_pd (1, nc, st->epochs_per_symbol, snr,
                                          cb, e)
                          : _mean_pd (snr, 1, umax, st->spc, (int)cb, e,
                                      (int)nc);
          if (pdd >= pd)
            break;
          nc++;
        }
      *out_d         = 1;
      *out_nc        = nc;
      *out_freq_bins = n_freq_bins;
      return;
    }
  *out_freq_bins = 1;

  if (st->epochs_per_symbol > 0.0)
    {
      /* doppler_rate caps the coherent depth from the opposite direction to
       * doppler_resolution's floor: past this D, in-window frequency drift
       * would smear the FFT peak across a resolution bin (see acq_create's
       * doc for the derivation), so it tightens -- never loosens -- reps. */
      size_t d_ceiling = st->reps;
      if (st->doppler_rate > 0.0)
        {
          double rate_ceiling_d = floor (
              st->chip_rate / ((double)st->sf * sqrt (st->doppler_rate)));
          if (rate_ceiling_d < 1.0)
            rate_ceiling_d = 1.0;
          if (rate_ceiling_d < (double)d_ceiling)
            d_ceiling = (size_t)rate_ceiling_d;
        }

      size_t d_start = 1;
      if (st->doppler_resolution > 0.0)
        {
          double floor_d = ceil (st->chip_rate
                                 / ((double)st->sf * st->doppler_resolution));
          d_start        = (floor_d < 1.0) ? 1 : (size_t)floor_d;
          if (d_start > d_ceiling)
            d_start = d_ceiling;
        }

      const int floored = (st->doppler_resolution > 0.0);
      size_t    best_d = 0, best_nc = 0, best_total = 0;
      for (size_t D = d_start; D <= d_ceiling; D++)
        {
          int found_at_d = 0;
          for (size_t nc = 1; nc <= st->max_noncoh; nc++)
            {
              size_t sb  = _searched_bins (D, du, span);
              double pc  = 1.0 - pow (1.0 - pfa, 1.0 / (double)(sb * cb));
              double eta = (nc > 1) ? det_threshold_noncoherent (pc, (int)nc)
                                    : det_threshold (pc);
              double pdd
                  = _data_mod_pd (D, nc, st->epochs_per_symbol, snr, cb, eta);
              if (pdd < pd)
                continue;
              size_t total = D * nc;
              if (best_d == 0 || total < best_total
                  || (total == best_total && D < best_d))
                {
                  best_d     = D;
                  best_nc    = nc;
                  best_total = total;
                }
              /* Floored search only: nc ascends, so the first hit at this D
               * is already its cheapest; stop growing D once any D succeeds
               * -- resolution, not total epochs, is the priority past the
               * floor.  Unfloored keeps the exhaustive scan (every nc, every
               * D) byte-identical to the original search. */
              if (floored)
                {
                  found_at_d = 1;
                  break;
                }
            }
          if (floored && found_at_d)
            break;
        }
      if (best_d == 0)
        {
          /* Best effort: nothing in the ceilings meets pd. */
          best_d  = d_ceiling;
          best_nc = st->max_noncoh;
        }
      *out_d  = best_d;
      *out_nc = best_nc;
      return;
    }

  /* Smallest coherent depth D meeting Pd (minimum latency for strong
   * signals); Bonferroni uses only the cells actually scanned at that
   * depth.  Sizing and prediction both use the straddle-derated SNR: the
   * on-grid best case would under-size the search (real Pd, averaged over
   * random Doppler/code phase, would miss the target — the gap the
   * Monte-Carlo characterization measures). */
  size_t best_d    = st->reps;
  int    coh_meets = 0;
  for (size_t D = 1; D <= st->reps; D++)
    {
      size_t sb   = _searched_bins (D, du, span);
      double umax = _intra_umax (D, sb, du, span);
      double pc   = 1.0 - pow (1.0 - pfa, 1.0 / (double)(sb * cb));
      double eta  = det_threshold (pc);
      if (_mean_pd (snr, D, umax, st->spc, (int)(D * cb), eta, 1) >= pd)
        {
          best_d    = D;
          coh_meets = 1;
          break;
        }
    }

  /* Non-coherent looks only if the coherent ceiling fell short and the
   * caller raised the cap above 1 (best effort if even max_noncoh is
   * infeasible). det_n_noncoh sizes at the mean straddle amplitude — a
   * fast initializer that Jensen makes slightly optimistic — then the
   * count escalates until the AVERAGED Pd meets the target. */
  size_t nc = 1;
  if (!coh_meets && st->max_noncoh > 1)
    {
      size_t sb    = _searched_bins (best_d, du, span);
      double umax  = _intra_umax (best_d, sb, du, span);
      double pc    = 1.0 - pow (1.0 - pfa, 1.0 / (double)(sb * cb));
      double sloss = _straddle_loss (best_d, sb, st->spc, du, span);
      int    k     = det_n_noncoh (snr * sloss, (int)(best_d * cb), pd, pc,
                                   (int)st->max_noncoh);
      nc           = (k > 0) ? (size_t)k : st->max_noncoh;
      while (nc < st->max_noncoh)
        {
          double e = det_threshold_noncoherent (pc, (int)nc);
          if (_mean_pd (snr, best_d, umax, st->spc, (int)(best_d * cb), e,
                        (int)nc)
              >= pd)
            break;
          nc++;
        }
    }
  *out_d  = best_d;
  *out_nc = nc;
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/* Rebuild every grid-dependent buffer/plan for (new_db, new_nc,
 * new_freq_bins): the slow-time FFT, the code correlator + its reference,
 * every per-frame scratch buffer (yframe/out_buf/colbuf/colout/mag_buf/
 * noise_scratch), the wideband roll-FFT buffers (wide_fwd/wide_inv/
 * wide_ref_spec/wide_spec/wide_prod — only when new_freq_bins > 1, see the
 * file doc comment's "Wideband mode" section), growing the ring if the new
 * frame no longer fits, and the non-coherent power accumulator if new_nc
 * crosses the 1 <-> >1 boundary.  Allocates the replacements FIRST and only
 * frees/adopts them once every allocation needed has succeeded, so a failure
 * leaves `st` fully untouched at its prior grid (the contract
 * acq_configure_search_raw() promises its caller).
 *
 * `code`/`code_len` are non-NULL only from acq_create()'s initial build
 * (fresh reference from the caller's chips); a later regrid
 * (acq_configure_search_raw) passes NULL and copies row 0 of the EXISTING
 * reference forward instead of rebuilding it from scratch -- row 0 is the
 * only nonzero row regardless of doppler_bins (see the comment below), so
 * it alone fully captures the code -- unaffected by new_freq_bins growing
 * new_n, since the fill loop below only ever touches indices [0, cb). */
static int
_regrid (acq_state_t *st, size_t new_db, size_t new_nc, size_t new_freq_bins,
         const uint8_t *code, size_t code_len)
{
  const size_t cb    = st->code_bins;
  const size_t new_n = new_db * new_freq_bins * cb;
  const int    grid_changed = (new_db != st->doppler_bins) || (new_n != st->n)
                           || (new_freq_bins != st->n_freq_bins);
  const size_t new_frame_n = (new_freq_bins > 1) ? cb : new_n;

  corr2d_state_t *new_corr         = NULL;
  fft_state_t    *new_fft          = NULL;
  float complex  *new_ref          = NULL;
  float complex  *new_yframe       = NULL;
  float complex  *new_out          = NULL;
  float complex  *new_colbuf       = NULL;
  float complex  *new_colout       = NULL;
  float          *new_mag          = NULL;
  float          *new_scratch      = NULL;
  float          *new_ncsurf       = NULL;
  dp_f32_t       *new_ring         = NULL;
  size_t          new_ring_cap     = st->ring_cap;
  fft_state_t    *new_wide_fwd     = NULL;
  fft_state_t    *new_wide_inv     = NULL;
  float complex  *new_wide_ref_spec = NULL;
  float complex  *new_wide_spec    = NULL;
  float complex  *new_wide_prod    = NULL;

  if (grid_changed)
    {
      /* Single-row oversampled BPSK reference: row 0 (indices [0, cb))
       * carries the replica (chip 0 -> +1, chip 1 -> -1, each held for spc
       * samples), the rest of the (possibly wideband-enlarged) buffer
       * zero. */
      new_ref = (float complex *)calloc (new_n, sizeof (float complex));
      if (!new_ref)
        goto fail;
      if (code)
        for (size_t c = 0; c < code_len; c++)
          {
            float sign = (code[c] & 1u) ? -1.0f : 1.0f;
            for (size_t s = 0; s < st->spc; s++)
              new_ref[c * st->spc + s] = sign;
          }
      else
        memcpy (new_ref, st->ref, cb * sizeof (float complex));

      new_corr = corr2d_create (new_ref, new_db, cb, 1, 1, 0, 0);
      if (!new_corr)
        goto fail;
      new_fft = fft_create (new_db, -1, 1);
      if (!new_fft)
        goto fail;

      new_yframe  = (float complex *)malloc (new_n * sizeof (float complex));
      new_out     = (float complex *)malloc (new_n * sizeof (float complex));
      new_colbuf  = (float complex *)malloc (new_db * sizeof (float complex));
      new_colout  = (float complex *)malloc (new_db * sizeof (float complex));
      new_mag     = (float *)malloc (new_n * sizeof (float));
      new_scratch = (float *)malloc (new_n * sizeof (float));
      if (!new_yframe || !new_out || !new_colbuf || !new_colout || !new_mag
          || !new_scratch)
        goto fail;

      if (new_freq_bins > 1)
        {
          new_wide_fwd = fft_create (cb, -1, 1);
          new_wide_inv = fft_create (cb, +1, 1);
          new_wide_ref_spec
              = (float complex *)malloc (cb * sizeof (float complex));
          new_wide_spec
              = (float complex *)malloc (cb * sizeof (float complex));
          new_wide_prod
              = (float complex *)malloc (cb * sizeof (float complex));
          if (!new_wide_fwd || !new_wide_inv || !new_wide_ref_spec
              || !new_wide_spec || !new_wide_prod)
            goto fail;
          /* Precompute conj(FFT(replica)) once -- reused every epoch. */
          fft_execute_cf32 (new_wide_fwd, new_ref, cb, new_wide_ref_spec);
          for (size_t j = 0; j < cb; j++)
            new_wide_ref_spec[j] = conjf (new_wide_ref_spec[j]);
        }
    }

  if (new_frame_n > new_ring_cap)
    {
      new_ring = _ring_create (new_frame_n > 512 ? new_frame_n : 512);
      if (!new_ring)
        goto fail;
      new_ring_cap = new_ring->capacity;
    }

  /* Non-coherent power accumulator — only on the N_nc > 1 path. */
  if (new_nc > 1)
    {
      new_ncsurf = (float *)calloc (new_n, sizeof (float));
      if (!new_ncsurf)
        goto fail;
    }

  /* Every allocation needed succeeded — commit: free the old, adopt the
   * new. */
  if (grid_changed)
    {
      if (st->corr)
        corr2d_destroy (st->corr);
      if (st->slow_fft)
        fft_destroy (st->slow_fft);
      if (st->wide_fwd)
        fft_destroy (st->wide_fwd);
      if (st->wide_inv)
        fft_destroy (st->wide_inv);
      free (st->ref);
      free (st->yframe);
      free (st->out_buf);
      free (st->colbuf);
      free (st->colout);
      free (st->mag_buf);
      free (st->noise_scratch);
      free (st->wide_ref_spec);
      free (st->wide_spec);
      free (st->wide_prod);
      st->corr          = new_corr;
      st->slow_fft      = new_fft;
      st->ref           = new_ref;
      st->yframe        = new_yframe;
      st->out_buf       = new_out;
      st->colbuf        = new_colbuf;
      st->colout        = new_colout;
      st->mag_buf       = new_mag;
      st->noise_scratch = new_scratch;
      st->wide_fwd      = new_wide_fwd;
      st->wide_inv      = new_wide_inv;
      st->wide_ref_spec = new_wide_ref_spec;
      st->wide_spec     = new_wide_spec;
      st->wide_prod     = new_wide_prod;
    }
  if (new_ring)
    {
      if (st->ring)
        dp_f32_destroy (st->ring);
      st->ring     = new_ring;
      st->ring_cap = new_ring_cap;
    }
  free (st->nc_surface);
  st->nc_surface = new_ncsurf;

  st->doppler_bins = new_db;
  st->n_freq_bins  = new_freq_bins;
  st->n_noncoh     = new_nc;
  st->n            = new_n;
  st->frame_n      = new_frame_n;
  st->noise_lo     = 0;
  st->noise_hi     = new_n - 1;
  return 0;

fail:
  if (new_corr)
    corr2d_destroy (new_corr);
  if (new_fft)
    fft_destroy (new_fft);
  if (new_wide_fwd)
    fft_destroy (new_wide_fwd);
  if (new_wide_inv)
    fft_destroy (new_wide_inv);
  free (new_ref);
  free (new_yframe);
  free (new_out);
  free (new_colbuf);
  free (new_colout);
  free (new_mag);
  free (new_scratch);
  free (new_ncsurf);
  free (new_wide_ref_spec);
  free (new_wide_spec);
  free (new_wide_prod);
  if (new_ring)
    dp_f32_destroy (new_ring);
  return -1;
}

acq_state_t *
acq_create (const uint8_t *code, size_t code_len, size_t reps, size_t spc,
            double chip_rate, double cn0_dbhz, double doppler_uncertainty,
            double pfa, double pd, int noise_mode, size_t max_noncoh,
            double symbol_rate, double doppler_resolution, double doppler_rate)
{
  /* Validate: bad arguments yield NULL (the binding maps this to a clear
   * MemoryError) rather than undefined behaviour downstream.  chip_rate /
   * cn0_dbhz > 0 act as the required sentinels (their toml placeholder
   * defaults are valid, but an explicit 0 is rejected). */
  if (max_noncoh < 1)
    max_noncoh = 1;
  const size_t sf   = code_len; /* sf is inferred from the code length */
  const double span = (sf > 0) ? chip_rate / (2.0 * (double)sf) : 0.0;
  /* doppler_uncertainty > span is no longer rejected: it engages wideband
   * mode (see the file doc comment's "Wideband mode" section) instead of
   * being an out-of-range error. */
  if (!code || code_len < 1 || spc < 1 || reps < 1 || !(chip_rate > 0.0)
      || !(cn0_dbhz > 0.0) || !isfinite (cn0_dbhz) || !(pfa > 0.0 && pfa < 1.0)
      || !(pd > 0.0 && pd < 1.0) || doppler_uncertainty < 0.0
      || doppler_resolution < 0.0 || doppler_rate < 0.0)
    return NULL;

  acq_state_t *st = (acq_state_t *)calloc (1, sizeof (*st));
  if (!st)
    return NULL;

  st->sf                  = sf;
  st->spc                 = spc;
  st->reps                = reps;
  st->code_bins           = sf * spc;
  st->chip_rate           = chip_rate;
  st->fs                  = chip_rate * (double)spc;
  st->cn0_dbhz            = cn0_dbhz;
  st->doppler_span_hz     = span;
  st->pd                  = pd;
  st->pfa                 = pfa;
  st->doppler_uncertainty = doppler_uncertainty;
  st->max_noncoh          = max_noncoh;
  st->noise_mode          = (det_noise_mode_t)noise_mode;
  st->symbol_rate         = (symbol_rate > 0.0) ? symbol_rate : 0.0;
  st->epochs_per_symbol   = (st->symbol_rate > 0.0)
                                ? (chip_rate / (double)sf) / st->symbol_rate
                                : 0.0;
  st->doppler_resolution
      = (doppler_resolution > 0.0) ? doppler_resolution : 0.0;
  st->doppler_rate = (doppler_rate > 0.0) ? doppler_rate : 0.0;

  /* C/N0 (dB-Hz) -> per-sample amplitude SNR: power SNR = (C/N0)/fs, and the
   * detection model's non-centrality is a = sqrt(2M)*snr (amplitude). */
  const double snr    = sqrt (pow (10.0, cn0_dbhz / 10.0) / st->fs);
  size_t       best_d = 0, best_nc = 0, best_freq_bins = 1;
  _auto_config (st, pfa, pd, snr, doppler_uncertainty, &best_d, &best_nc,
               &best_freq_bins);

  if (_regrid (st, best_d, best_nc, best_freq_bins, code, code_len) != 0)
    goto fail;
  _commit_thresholds (st, pfa, pd, snr, doppler_uncertainty);

  return st;

fail:
  acq_destroy (st);
  return NULL;
}

int
acq_configure_search_raw (acq_state_t *st, size_t doppler_bins,
                          size_t n_noncoh)
{
  if (doppler_bins < 1 || doppler_bins > st->reps || n_noncoh < 1
      || n_noncoh > st->max_noncoh)
    return -1;

  /* This escape hatch always pins the NATIVE (doppler_bins, n_noncoh) grid
   * -- it exits wideband mode (n_freq_bins back to 1) if that was active,
   * matching its documented contract of pinning the classic grid directly. */
  if (_regrid (st, doppler_bins, n_noncoh, 1, NULL, 0) != 0)
    return -1;

  const double snr = sqrt (pow (10.0, st->cn0_dbhz / 10.0) / st->fs);
  _commit_thresholds (st, st->pfa, st->pd, snr, st->doppler_uncertainty);
  acq_reset (st);
  return 0;
}

void
acq_destroy (acq_state_t *st)
{
  if (!st)
    return;
  if (st->corr)
    corr2d_destroy (st->corr);
  if (st->slow_fft)
    fft_destroy (st->slow_fft);
  if (st->wide_fwd)
    fft_destroy (st->wide_fwd);
  if (st->wide_inv)
    fft_destroy (st->wide_inv);
  if (st->ring)
    dp_f32_destroy (st->ring);
  free (st->ref);
  free (st->yframe);
  free (st->out_buf);
  free (st->colbuf);
  free (st->colout);
  free (st->mag_buf);
  free (st->noise_scratch);
  free (st->nc_surface);
  free (st->wide_ref_spec);
  free (st->wide_spec);
  free (st->wide_prod);
  free (st);
}

void
acq_reset (acq_state_t *st)
{
  DP_STORE_REL (&st->ring->head, 0);
  DP_STORE_REL (&st->ring->tail, 0);
  corr2d_reset (st->corr);
  if (st->nc_surface)
    memset (st->nc_surface, 0, st->n * sizeof (float));
  st->nc_count         = 0;
  st->samples_consumed = 0;
  st->peak_row = st->peak_col = 0;
  st->peak_mag = st->noise_est = st->test_stat = 0.0f;
}

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t
acq_push (acq_state_t *st, const float complex *in, size_t n_in,
          acq_result_t *result, size_t max_results)
{
  size_t       ndet     = 0;
  size_t       off      = 0;
  const size_t n        = st->n;
  const size_t frame_n  = st->frame_n;
  const size_t ny       = st->doppler_bins;
  const size_t nx       = st->code_bins;
  const size_t wideband = st->n_freq_bins > 1;

  while (off < n_in && ndet < max_results)
    {
      size_t head     = DP_LOAD_RLX (&st->ring->head);
      size_t tail     = DP_LOAD_ACQ (&st->ring->tail);
      size_t space    = st->ring->capacity - (head - tail);
      size_t to_write = n_in - off;
      if (to_write > space)
        to_write = space;

      if (to_write > 0)
        {
          dp_f32_write (st->ring, (const float *)(in + off), to_write);
          off += to_write;
        }

      while (ndet < max_results)
        {
          size_t h = DP_LOAD_ACQ (&st->ring->head);
          size_t t = DP_LOAD_RLX (&st->ring->tail);
          if (h - t < frame_n)
            break;

          const float complex *frame
              = (const float complex *)(st->ring->data
                                        + (t & st->ring->mask) * 2);

          size_t n_out;
          if (wideband)
            {
              /* Wideband: one shared forward FFT of this epoch; roll its
               * spectrum per hypothesis against the fixed replica spectrum,
               * one inverse FFT per hypothesis, filling out_buf's
               * n_freq_bins rows -- see the file doc comment's "Wideband
               * mode" section and prototypes/async_despreader/
               * bench_freq_bank.py (the benchmark this reuses, roll-FFT
               * over a tuned-mixer bank).  Row r's REPORTED index uses the
               * same native FFT-bin convention as the doppler_bins axis
               * (0 = DC, ascending positive to n_freq_bins/2, then
               * wrapping negative) so doppler_bin/doppler_res_hz compose
               * identically in either mode; the ACTUAL roll amount fed to
               * the (j+roll)%nx indexing wraps modulo nx (the full
               * code_bins-point transform), not modulo n_freq_bins, so a
               * "negative" row correctly represents a negative-frequency
               * roll instead of silently aliasing back into the positive
               * side (n_freq_bins is always << nx: n_freq_bins tiles the
               * requested uncertainty, nx = sf*spc is the full code
               * length). */
              fft_execute_cf32 (st->wide_fwd, frame, nx, st->wide_spec);
              for (size_t r = 0; r < st->n_freq_bins; r++)
                {
                  long signed_r = (r <= st->n_freq_bins / 2)
                                      ? (long)r
                                      : (long)r - (long)st->n_freq_bins;
                  long   wrapped = ((signed_r % (long)nx) + (long)nx)
                                   % (long)nx;
                  size_t roll    = (size_t)wrapped;
                  for (size_t j = 0; j < nx; j++)
                    st->wide_prod[j] = st->wide_spec[(j + roll) % nx]
                                       * st->wide_ref_spec[j];
                  fft_execute_cf32 (st->wide_inv, st->wide_prod, nx,
                                    st->out_buf + r * nx);
                }
              n_out = n;
            }
          else
            {
              /* Slow-time Doppler FFT: FFT along the ny segment axis, per
               * column. Unnormalised (matches numpy fft); corr2d supplies
               * the only 1/n. */
              for (size_t j = 0; j < nx; j++)
                {
                  for (size_t i = 0; i < ny; i++)
                    st->colbuf[i] = frame[i * nx + j];
                  fft_execute_cf32 (st->slow_fft, st->colbuf, ny, st->colout);
                  for (size_t i = 0; i < ny; i++)
                    st->yframe[i * nx + j] = st->colout[i];
                }
              n_out = corr2d_execute (st->corr, st->yframe, n, st->out_buf);
            }
          dp_f32_consume (st->ring, frame_n);
          st->samples_consumed += frame_n;

          if (n_out == 0)
            continue; /* mid-dwell — coherent accumulation, no dump yet */

          if (st->n_noncoh <= 1)
            {
              /* Coherent path (unchanged): amplitude mean-CFAR per dump. */
              _compute_stat (st);
              if (st->test_stat > st->threshold)
                {
                  float amp_snr = st->test_stat / ACQ_SQRT_2_OVER_PI
                                  / sqrtf (2.0f * (float)frame_n);
                  float cn0_dbhz_est
                      = _cn0_dbhz_from_amp_snr (amp_snr, st->fs);
                  result[ndet++]
                      = (acq_result_t){ st->peak_row,  st->peak_col,
                                        st->peak_mag,  st->noise_est,
                                        st->test_stat, cn0_dbhz_est };
                }
              continue;
            }

          /* Non-coherent path: magnitude-square accumulate each coherent look;
           * gate the order-N_nc statistic once n_noncoh looks are in. */
          for (size_t k = 0; k < n; k++)
            {
              float m = cabsf (st->out_buf[k]);
              st->nc_surface[k] += m * m;
            }
          if (++st->nc_count < st->n_noncoh)
            continue; /* still accumulating looks */

          _compute_stat_nc (st);
          if (st->test_stat > st->eta_nc)
            {
              float amp_snr
                  = st->test_stat
                    / sqrtf (2.0f * (float)frame_n * (float)st->n_noncoh);
              float cn0_dbhz_est = _cn0_dbhz_from_amp_snr (amp_snr, st->fs);
              result[ndet++] = (acq_result_t){ st->peak_row,  st->peak_col,
                                               st->peak_mag,  st->noise_est,
                                               st->test_stat, cn0_dbhz_est };
            }
          memset (st->nc_surface, 0, n * sizeof (float));
          st->nc_count = 0;
        }

      if (to_write == 0)
        break;
    }

  return ndet;
}

/* ── Serializable state — the pure-transducer face ─────────────────────────
 *
 * Fixed flat layout (offsets depend only on the ring capacity), so the state
 * blob is portable POD:
 *   [hdr][ float complex unconsumed[ring_cap] ][ float nc_surface[n] (if nc) ]
 * Only the first hdr.n_unconsumed of the unconsumed region holds data; that
 * may exceed n (undrained full frames from a max_results-saturated run,
 * preserved so the resume processes them).
 */

/* Offset to the ring-samples region: standard envelope + acq's extra header.
 */
#define ACQ_BODY_OFF (sizeof (dp_state_hdr_t) + sizeof (acq_extra_t))

static float complex *
_state_samples (void *blob)
{
  return (float complex *)((char *)blob + ACQ_BODY_OFF);
}

static float *
_state_nc (void *blob, size_t ring_cap)
{
  return (float *)((char *)blob + ACQ_BODY_OFF
                   + ring_cap * sizeof (float complex));
}

size_t
acq_state_bytes (const acq_state_t *st)
{
  size_t b = ACQ_BODY_OFF + st->ring_cap * sizeof (float complex);
  if (st->n_noncoh > 1)
    b += st->n * sizeof (float);
  return b;
}

void
acq_get_state (const acq_state_t *st, void *blob)
{
  const size_t n   = st->n;
  size_t       h   = DP_LOAD_ACQ (&st->ring->head);
  size_t       t   = DP_LOAD_RLX (&st->ring->tail);
  size_t       nun = h - t;

  dp_writer_t w = dp_writer_init (blob, acq_state_bytes (st));
  dp_w_hdr (&w, ACQ_STATE_MAGIC, ACQ_STATE_VERSION, acq_state_bytes (st));
  acq_extra_t ex = { .has_nc           = (uint16_t)(st->n_noncoh > 1),
                     ._pad             = 0,
                     .n_noncoh         = (uint32_t)st->n_noncoh,
                     .n                = (uint64_t)n,
                     .samples_consumed = st->samples_consumed,
                     .nc_count         = (uint32_t)st->nc_count,
                     .n_unconsumed     = (uint32_t)nun };
  dp_w_bytes (&w, &ex, sizeof ex);

  float complex *dst = _state_samples (blob);
  for (size_t i = 0; i < nun; i++)
    {
      size_t idx = (t + i) & st->ring->mask;
      dst[i]     = st->ring->data[idx * 2] + I * st->ring->data[idx * 2 + 1];
    }
  if (ex.has_nc)
    memcpy (_state_nc (blob, st->ring_cap), st->nc_surface,
            n * sizeof (float));
}

int
acq_set_state (acq_state_t *st, const void *blob)
{
  int rc = dp_state_validate (blob, acq_state_bytes (st), ACQ_STATE_MAGIC,
                              ACQ_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  acq_extra_t ex;
  memcpy (&ex, (const char *)blob + sizeof (dp_state_hdr_t), sizeof ex);
  if (ex.n != (uint64_t)st->n || ex.n_noncoh != (uint32_t)st->n_noncoh
      || ex.n_unconsumed > (uint32_t)st->ring_cap)
    return DP_ERR_INVALID;

  /* Reset the live state, then replay the blob's buffered samples + nc. */
  DP_STORE_REL (&st->ring->head, 0);
  DP_STORE_REL (&st->ring->tail, 0);
  corr2d_reset (st->corr);
  st->samples_consumed = ex.samples_consumed;
  st->nc_count         = ex.nc_count;

  const float complex *src = _state_samples ((void *)blob);
  if (ex.n_unconsumed > 0)
    dp_f32_write (st->ring, (const float *)src, ex.n_unconsumed);

  if (st->nc_surface)
    {
      if (ex.has_nc)
        memcpy (st->nc_surface, _state_nc ((void *)blob, st->ring_cap),
                st->n * sizeof (float));
      else
        memset (st->nc_surface, 0, st->n * sizeof (float));
    }
  return DP_OK;
}

size_t
acq_run (acq_state_t *st, const void *state_in, void *state_out,
         const float complex *in, size_t n_in, acq_result_t *result,
         size_t max_results)
{
  if (state_in)
    {
      if (acq_set_state (st, state_in) != 0)
        return 0;
    }
  else
    acq_reset (st);

  size_t ndet = acq_push (st, in, n_in, result, max_results);

  if (state_out)
    acq_get_state (st, state_out);
  return ndet;
}
