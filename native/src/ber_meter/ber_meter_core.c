/*
 * ber_meter_core.c — the error-rate accumulator: the transmitted reference,
 * the running counters, the marker-based alignment detector and the exact
 * confidence interval. See ber_meter/ber_meter_core.h for the state struct and
 * ber/ber_core.h for the three gates a measurement has to pass.
 */
#include "ber_meter/ber_meter_core.h"
#include "ber/ber_core.h"
#include "detection/detection_core.h"
#include "mpsk/mpsk_core.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── the exact interval ─────────────────────────────────────────────────── */

ber_interval_t
ber_confidence (size_t errors, size_t symbols, double conf)
{
  ber_interval_t c;
  double         alpha, eta;
  int            r;

  if (!(conf > 0.0) || !(conf < 1.0))
    conf = BER_CONF;
  alpha     = 1.0 - conf;
  c.p_hat   = NAN;
  c.lo      = 0.0;
  c.hi      = INFINITY;
  c.rel     = INFINITY;
  c.conf    = conf;
  c.errors  = errors;
  c.symbols = symbols;
  if (symbols == 0)
    return c;
  if (errors == 0)
    {
      /* Exact one-sided: P(0 errors) = (1-p)^N <= alpha <=> p <= -ln(a)/N,
         which is det_threshold(alpha)^2 / 2 over N. */
      eta     = det_threshold (alpha);
      c.p_hat = 0.0;
      c.hi    = 0.5 * eta * eta / (double)symbols;
      return c;
    }
  r     = (errors > (size_t)INT_MAX) ? INT_MAX : (int)errors;
  eta   = det_threshold_noncoherent (1.0 - 0.5 * alpha, r);
  c.lo  = 0.5 * eta * eta / (double)symbols;
  eta   = det_threshold_noncoherent (0.5 * alpha, r);
  c.hi  = 0.5 * eta * eta / (double)symbols;
  c.rel = 1.0 / sqrt ((double)errors);
  /* The unbiased estimator degenerates to exactly 0 at r = 1, which would be
     actively misleading beside a non-zero interval; fall back to the naive
     rate there and let rel = 1.0 say how little it is worth. */
  c.p_hat = (errors >= 2 && symbols >= 2)
                ? (double)(errors - 1) / (double)(symbols - 1)
                : (double)errors / (double)symbols;
  return c;
}

/* ── the meter ──────────────────────────────────────────────────────────── */

ber_meter_state_t *
ber_meter_create (int m, size_t target_errors, double conf)
{
  ber_meter_state_t *s;
  if (m != 2 && m != 4 && m != 8)
    return NULL;
  if (conf != 0.0 && (conf <= 0.0 || conf >= 1.0))
    return NULL;
  s                = dp_xcalloc (1, sizeof *s);
  s->m             = m;
  s->bps           = mpsk_bps (m);
  s->target_errors = target_errors ? target_errors : BER_TARGET_ERRORS;
  s->conf          = (conf > 0.0) ? conf : BER_CONF;
  return s;
}

void
ber_meter_destroy (ber_meter_state_t *s)
{
  if (!s)
    return;
  free (s->truth);
  free (s);
}

void
ber_meter_reset (ber_meter_state_t *s)
{
  s->errors = s->symbols = s->bit_errors = s->bits = 0;
  s->skipped = s->bursts = 0;
}

int
ber_meter_set_truth (ber_meter_state_t *s, const uint8_t *truth,
                     size_t truth_len)
{
  if (!truth || truth_len == 0)
    return DP_ERR_INVALID;
  for (size_t i = 0; i < truth_len; i++)
    if (truth[i] >= (uint8_t)s->m)
      return DP_ERR_INVALID;
  free (s->truth);
  s->truth = dp_xmalloc (truth_len);
  memcpy (s->truth, truth, truth_len);
  s->truth_len = truth_len;
  return DP_OK;
}

/* 1 when truth index t is covered by a marker occurrence. Marker symbols are
 * KNOWN, so scoring them would flatter the rate with symbols that had no
 * chance of being wrong — and, in the blind case, with the very symbols that
 * fixed the alignment. */
static int
in_marker (size_t t, size_t t0, size_t n_marker, size_t period, size_t occ)
{
  size_t off;
  if (t < t0)
    return 0;
  off = t - t0;
  if (!period)
    return off < n_marker;
  if (off / period >= occ)
    return 0;
  return (off % period) < n_marker;
}

ber_align_t
ber_align_detect (const float complex *rx, size_t rx_len, const uint8_t *truth,
                  size_t truth_len, int m, size_t t0, size_t n_marker,
                  size_t period, int lag_span, double pfa)
{
  ber_align_t a;
  double      stat[BER_MAX_LAGS], phre[BER_MAX_LAGS], phim[BER_MAX_LAGS];
  size_t      occ[BER_MAX_LAGS];
  long        nl, best = -1;
  double      phi0 = mpsk_phi0 (m);
  size_t      cap;

  a.lag   = 0;
  a.phase = a.stat = 0.0;
  a.threshold      = INFINITY;
  a.margin_db      = -INFINITY;
  a.runner_db      = 0.0;
  a.occurrences = a.slips = 0;
  a.saturated = a.ok = 0;

  if (!rx || !truth)
    return a;
  if (!n_marker)
    n_marker = BER_SYNC_SYMS;
  if (lag_span <= 0)
    lag_span = BER_LAG_SPAN;
  if (2 * lag_span + 1 > BER_MAX_LAGS)
    lag_span = (BER_MAX_LAGS - 1) / 2;
  if (!(pfa > 0.0) || pfa >= 1.0)
    pfa = BER_SYNC_PFA;
  nl = 2 * (long)lag_span + 1;
  if (n_marker < 8 || t0 + n_marker > truth_len)
    return a;

  /* How many occurrences fit over the whole lag search. Capped so every lag
     combines the SAME number, or the statistic is not comparable across lags.
   */
  cap = 1;
  if (period >= n_marker && period > 0)
    {
      size_t room
          = (rx_len > (size_t)lag_span) ? rx_len - (size_t)lag_span : 0;
      cap = (room > t0 + n_marker) ? 1 + (room - t0 - n_marker) / period : 0;
      if (cap < 1)
        cap = 1;
    }

  for (long li = 0; li < nl; li++)
    {
      long   lag = li - lag_span;
      double acc = 0.0, pr = 0.0, pi = 0.0;
      size_t used = 0;
      for (size_t k = 0; k < cap; k++)
        {
          long   b  = (long)(t0 + k * period) - lag;
          double cr = 0.0, ci = 0.0;
          if (b < 0 || (size_t)b + n_marker > rx_len)
            continue;
          if (t0 + k * period + n_marker > truth_len)
            continue;
          for (size_t j = 0; j < n_marker; j++)
            {
              double th
                  = 2.0 * M_PI * (double)truth[t0 + k * period + j] / (double)m
                    + phi0;
              double re = (double)crealf (rx[(size_t)b + j]);
              double im = (double)cimagf (rx[(size_t)b + j]);
              double ur = cos (th), ui = sin (th);
              cr += re * ur + im * ui;
              ci += im * ur - re * ui;
            }
          acc += cr * cr + ci * ci;
          if (cr * cr + ci * ci > pr * pr + pi * pi)
            {
              pr = cr;
              pi = ci;
            }
          used++;
        }
      stat[li] = used ? acc : -1.0;
      phre[li] = pr;
      phim[li] = pi;
      occ[li]  = used;
      if (used && (best < 0 || stat[li] > stat[best]))
        best = li;
    }
  if (best < 0)
    return a;

  /* CFAR reference: the mean off-peak statistic, with a guard band so the
     correlation's own shoulders do not inflate the floor. */
  {
    const long guard = 3;
    double     sum = 0.0, runner = 0.0;
    size_t     cnt = 0;
    for (long li = 0; li < nl; li++)
      {
        if (labs (li - best) <= guard || stat[li] < 0.0)
          continue;
        sum += stat[li] / (double)(occ[li] ? occ[li] : 1);
        if (stat[li] > runner)
          runner = stat[li];
        cnt++;
      }
    if (cnt < 8)
      return a;
    {
      double floor_pk = sum / (double)cnt;
      size_t K        = occ[best] ? occ[best] : 1;
      int    ki       = (K > (size_t)INT_MAX) ? INT_MAX : (int)K;
      if (floor_pk <= 0.0)
        return a;
      /* Normalize to the unit-variance-per-quadrature convention the detection
         module's thresholds use: R^2 = sum_k |C_k|^2 / (sigma^2/2), whose null
         distribution is exactly marcum_q(K, 0, R). */
      a.stat      = sqrt (2.0 * stat[best] / floor_pk);
      a.threshold = det_threshold_noncoherent (pfa / (double)nl, ki);
      a.runner_db
          = (runner > 0.0) ? 10.0 * log10 (stat[best] / runner) : INFINITY;
      a.occurrences = K;
    }
  }

  a.lag       = (int)(best - lag_span);
  a.phase     = atan2 (phim[best], phre[best]);
  a.margin_db = 20.0 * log10 (a.stat / a.threshold);
  a.saturated = (best == 0 || best == nl - 1);

  /* A slip is an occurrence whose phase differs from the peak's by more than
     half a decision sector — the same thing a cycle slip does to the data. */
  {
    double half = M_PI / (double)m;
    for (size_t k = 0; k < a.occurrences; k++)
      {
        long   b  = (long)(t0 + k * period) - a.lag;
        double cr = 0.0, ci = 0.0, d;
        if (b < 0 || (size_t)b + n_marker > rx_len)
          continue;
        for (size_t j = 0; j < n_marker; j++)
          {
            double th
                = 2.0 * M_PI * (double)truth[t0 + k * period + j] / m + phi0;
            double re = (double)crealf (rx[(size_t)b + j]);
            double im = (double)cimagf (rx[(size_t)b + j]);
            cr += re * cos (th) + im * sin (th);
            ci += im * cos (th) - re * sin (th);
          }
        d = atan2 (ci, cr) - a.phase;
        while (d > M_PI)
          d -= 2.0 * M_PI;
        while (d < -M_PI)
          d += 2.0 * M_PI;
        if (fabs (d) > half)
          a.slips++;
      }
  }

  a.ok = (a.stat >= a.threshold) && !a.saturated && (a.runner_db >= 3.0);
  return a;
}

ber_align_t
ber_meter_detect (const ber_meter_state_t *s, const float complex *rx,
                  size_t rx_len, size_t t0, size_t n_marker, size_t period,
                  int lag_span, double pfa)
{
  return ber_align_detect (rx, rx_len, s->truth, s->truth_len, s->m, t0,
                           n_marker, period, lag_span, pfa);
}

int
ber_meter_align (ber_meter_state_t *s, const float complex *rx, size_t rx_len,
                 size_t t0, size_t n_marker, size_t period, int lag_span,
                 double pfa)
{
  s->last
      = ber_meter_detect (s, rx, rx_len, t0, n_marker, period, lag_span, pfa);
  s->mk_t0     = t0;
  s->mk_n      = n_marker ? n_marker : BER_SYNC_SYMS;
  s->mk_period = period;
  return s->last.ok;
}

size_t
ber_meter_score (ber_meter_state_t *s, const float complex *rx, size_t rx_len,
                 size_t lo, size_t hi)
{
  double phi0     = mpsk_phi0 (s->m);
  double step     = 2.0 * M_PI / (double)s->m;
  double rot      = s->last.phase + phi0;
  int    lag      = s->last.lag;
  size_t occ      = s->last.occurrences;
  size_t t0       = s->mk_t0;
  size_t n_marker = s->mk_n;
  size_t period   = s->mk_period;
  size_t scored   = 0;

  if (!rx || !s->truth)
    return 0;
  if (hi > rx_len)
    hi = rx_len;
  if (!n_marker)
    n_marker = BER_SYNC_SYMS;
  for (size_t i = lo; i < hi; i++)
    {
      long     t = (long)i + (long)lag;
      double   th, dec;
      unsigned gd, gt, x;
      int      d;
      if (t < 0 || (size_t)t >= s->truth_len
          || in_marker ((size_t)t, t0, n_marker, period, occ))
        {
          s->skipped++;
          continue;
        }
      th  = atan2 ((double)cimagf (rx[i]), (double)crealf (rx[i])) - rot;
      dec = th / step;
      d   = (int)(((lround (dec) % s->m) + s->m) % s->m);
      s->symbols++;
      scored++;
      if (d != (int)s->truth[(size_t)t])
        s->errors++;
      /* Bit errors on the Gray labels — the mapping the receivers' own
         steps_bits() emits, so the two are directly comparable. */
      gd = mpsk_gray_encode ((unsigned)d);
      gt = mpsk_gray_encode ((unsigned)s->truth[(size_t)t]);
      x  = gd ^ gt;
      for (int q = 0; q < s->bps; q++)
        s->bit_errors += (x >> q) & 1u;
      s->bits += (size_t)s->bps;
    }
  if (scored)
    s->bursts++;
  return scored;
}

void
ber_meter_set_align (ber_meter_state_t *s, ber_align_t align, size_t t0,
                     size_t n_marker, size_t period)
{
  s->last      = align;
  s->mk_t0     = t0;
  s->mk_n      = n_marker ? n_marker : BER_SYNC_SYMS;
  s->mk_period = period;
}

int
ber_meter_get_enough (const ber_meter_state_t *s)
{
  return s->errors >= s->target_errors;
}

ber_interval_t
ber_meter_interval (const ber_meter_state_t *s, size_t errors, size_t symbols)
{
  return ber_confidence (errors, symbols, s->conf);
}

ber_interval_t
ber_meter_ser (const ber_meter_state_t *s)
{
  return ber_confidence (s->errors, s->symbols, s->conf);
}

ber_interval_t
ber_meter_ber (const ber_meter_state_t *s)
{
  return ber_confidence (s->bit_errors, s->bits, s->conf);
}

size_t
ber_meter_get_errors (const ber_meter_state_t *s)
{
  return s->errors;
}
size_t
ber_meter_get_symbols (const ber_meter_state_t *s)
{
  return s->symbols;
}
size_t
ber_meter_get_bit_errors (const ber_meter_state_t *s)
{
  return s->bit_errors;
}
size_t
ber_meter_get_bits (const ber_meter_state_t *s)
{
  return s->bits;
}
size_t
ber_meter_get_skipped (const ber_meter_state_t *s)
{
  return s->skipped;
}
int
ber_meter_get_m (const ber_meter_state_t *s)
{
  return s->m;
}
size_t
ber_meter_get_target_errors (const ber_meter_state_t *s)
{
  return s->target_errors;
}
double
ber_meter_get_conf (const ber_meter_state_t *s)
{
  return s->conf;
}

/* ── state serialization ────────────────────────────────────────────────── */
/* The blob carries the RUNNING counters only. `m`/`target_errors`/`conf` are
 * configuration, restored by create(); the truth sequence is likewise
 * configuration, reinstalled by set_truth(). That keeps a blob small and
 * independent of however many symbols the reference happens to be. */

size_t
ber_meter_state_bytes (const ber_meter_state_t *s)
{
  (void)s;
  return sizeof (dp_state_hdr_t) + 6 * sizeof (uint64_t);
}

void
ber_meter_get_state (const ber_meter_state_t *s, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, ber_meter_state_bytes (s));
  dp_w_hdr (&w, BER_METER_STATE_MAGIC, BER_METER_STATE_VERSION,
            ber_meter_state_bytes (s));
  dp_w_u64 (&w, (uint64_t)s->errors);
  dp_w_u64 (&w, (uint64_t)s->symbols);
  dp_w_u64 (&w, (uint64_t)s->bit_errors);
  dp_w_u64 (&w, (uint64_t)s->bits);
  dp_w_u64 (&w, (uint64_t)s->skipped);
  dp_w_u64 (&w, (uint64_t)s->bursts);
}

int
ber_meter_set_state (ber_meter_state_t *s, const void *blob)
{
  dp_reader_t r;
  int rc = dp_state_validate (blob, ber_meter_state_bytes (s),
                              BER_METER_STATE_MAGIC, BER_METER_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  r             = dp_reader_init (blob, ber_meter_state_bytes (s));
  r.off         = sizeof (dp_state_hdr_t); /* past the envelope */
  s->errors     = (size_t)dp_r_u64 (&r);
  s->symbols    = (size_t)dp_r_u64 (&r);
  s->bit_errors = (size_t)dp_r_u64 (&r);
  s->bits       = (size_t)dp_r_u64 (&r);
  s->skipped    = (size_t)dp_r_u64 (&r);
  s->bursts     = (size_t)dp_r_u64 (&r);
  return DP_OK;
}

int
ber_meter_get_lag (const ber_meter_state_t *s)
{
  return s->last.lag;
}
double
ber_meter_get_phase (const ber_meter_state_t *s)
{
  return s->last.phase;
}
double
ber_meter_get_align_stat (const ber_meter_state_t *s)
{
  return s->last.stat;
}
double
ber_meter_get_align_margin_db (const ber_meter_state_t *s)
{
  return s->last.margin_db;
}
double
ber_meter_get_align_runner_db (const ber_meter_state_t *s)
{
  return s->last.runner_db;
}
size_t
ber_meter_get_align_occurrences (const ber_meter_state_t *s)
{
  return s->last.occurrences;
}
size_t
ber_meter_get_align_slips (const ber_meter_state_t *s)
{
  return s->last.slips;
}
int
ber_meter_get_align_saturated (const ber_meter_state_t *s)
{
  return s->last.saturated;
}
int
ber_meter_get_align_ok (const ber_meter_state_t *s)
{
  return s->last.ok;
}
