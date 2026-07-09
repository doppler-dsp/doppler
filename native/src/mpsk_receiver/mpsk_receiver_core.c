#include "mpsk_receiver/mpsk_receiver_core.h"

#include "wfm/wfm_dsp.h" /* wfm_rrc_taps / wfm_rrc_ntaps (RRC matched filter) */
#include <math.h>
#include <stdlib.h>

/* Numerical guard on the symbol magnitude in the decision discriminator. */
#define MPSK_RX_EPS 1e-12

/* Build the matched-filter taps for the selected pulse shape into a freshly
 * allocated array; *ntaps receives the length.  Returns NULL on bad args /
 * allocation failure.  I&D is a unit-DC boxcar (1/sps each), the matched
 * filter to a rectangular symbol pulse; RRC reuses the canonical wfm_rrc_taps.
 */
static float *
build_mf_taps (int pulse, size_t sps, double beta, int span, size_t *ntaps)
{
  if (pulse == MPSK_RX_PULSE_RRC)
    {
      if (span <= 0 || beta < 0.0 || beta > 1.0)
        return NULL;
      size_t nt = wfm_rrc_ntaps ((int)sps, span);
      float *t  = malloc (nt * sizeof (float));
      if (!t)
        return NULL;
      wfm_rrc_taps (beta, (int)sps, span, t);
      *ntaps = nt;
      return t;
    }
  /* MPSK_RX_PULSE_IANDD: boxcar averaging filter, one symbol wide. */
  float *t = malloc (sps * sizeof (float));
  if (!t)
    return NULL;
  for (size_t i = 0; i < sps; i++)
    t[i] = 1.0f / (float)sps;
  *ntaps = sps;
  return t;
}

mpsk_receiver_state_t *
mpsk_receiver_create (int m, size_t sps, int n, int pulse, double rrc_beta,
                      int rrc_span, double bn_carrier, double zeta,
                      double bn_timing, int acq_to_track, double lock_thresh,
                      double init_norm_freq, size_t warmup_syms,
                      int differential)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  if (sps == 0 || n <= 0 || sps % (size_t)n != 0)
    return NULL; /* arm length must be a whole number of samples */
  if (pulse != MPSK_RX_PULSE_IANDD && pulse != MPSK_RX_PULSE_RRC)
    return NULL;

  size_t ntaps;
  float *taps = build_mf_taps (pulse, sps, rrc_beta, rrc_span, &ntaps);
  if (!taps)
    return NULL;
  fir_state_t *mf = fir_create_real (taps, ntaps);
  if (!mf)
    {
      free (taps);
      return NULL;
    }
  mpsk_receiver_state_t *rx = calloc (1, sizeof (*rx));
  if (!rx)
    {
      fir_destroy (mf);
      free (taps);
      return NULL;
    }

  carrier_nda_init (&rx->car, bn_carrier, zeta, init_norm_freq, sps, n, m);
  symsync_init (&rx->sync, sps, bn_timing, zeta, FARROW_CUBIC,
                SYMSYNC_TED_GARDNER);
  rx->mf            = mf;
  rx->mf_taps       = taps;
  rx->m             = m;
  rx->sps           = sps;
  rx->n             = n;
  rx->pulse         = pulse;
  rx->rrc_beta      = rrc_beta;
  rx->rrc_span      = rrc_span;
  rx->acq_to_track  = acq_to_track ? 1 : 0;
  rx->lock_thresh   = lock_thresh;
  rx->warmup_syms   = warmup_syms;
  rx->tracking      = 0;
  rx->sym_count     = 0;
  rx->differential  = differential ? 1 : 0;
  rx->have_prev_idx = 0;
  rx->prev_idx      = 0;
  /* The NDA M-th-power loop has stable lock points only where Im(z^m) = 0 with
   * a restoring slope, i.e. the "0-grid" z = exp(j*2*pi*k/m) (z^m = +1). But
   * the QPSK constellation (mpsk convention) sits on the pi/4-offset grid, so
   * a receiver that fed the slicer the raw de-rotated stream would land every
   * QPSK symbol exactly on a decision boundary. Rotate the *symbol path*
   * (matched filter input) by exp(j*phi0) so the constellation lands where
   * mpsk_slice expects it, while the NDA arm keeps the unrotated stream (it
   * locks the 0-grid, so the lock metric is ~+scale at lock for every M). */
  rx->sym_rot
      = (float complex) (cos (mpsk_phi0 (m)) + sin (mpsk_phi0 (m)) * I);
  return rx;
}

void
mpsk_receiver_destroy (mpsk_receiver_state_t *state)
{
  if (!state)
    return;
  fir_destroy (state->mf);
  free (state->mf_taps);
  free (state);
}

void
mpsk_receiver_reset (mpsk_receiver_state_t *state)
{
  carrier_nda_reset (&state->car);
  symsync_reset (&state->sync);
  fir_reset (state->mf);
  state->tracking      = 0;
  state->sym_count     = 0;
  state->have_prev_idx = 0;
  state->prev_idx      = 0;
}

/* Serializable state — carrier_nda + symsync + matched-filter children as
 * nested sub-blobs, then running tracking/handover scalars; MF taps are config
 * (create). */
size_t
mpsk_receiver_state_bytes (const mpsk_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + carrier_nda_state_bytes (&s->car)
         + symsync_state_bytes (&s->sync) + fir_state_bytes (s->mf)
         + sizeof (uint64_t) + 3 * sizeof (uint32_t) + sizeof (float _Complex);
}

void
mpsk_receiver_get_state (const mpsk_receiver_state_t *s, void *blob)
{
  DP_GET_OPEN (MPSK_RECEIVER_STATE_MAGIC, MPSK_RECEIVER_STATE_VERSION,
               mpsk_receiver_state_bytes (s));
  DP_W_CHILD (&_w, carrier_nda, &s->car);
  DP_W_CHILD (&_w, symsync, &s->sync);
  DP_W_CHILD (&_w, fir, s->mf);
  dp_w_u32 (&_w, (uint32_t)s->tracking);
  dp_w_u64 (&_w, s->sym_count);
  dp_w_u32 (&_w, (uint32_t)s->have_prev_idx);
  dp_w_u32 (&_w, s->prev_idx);
  dp_w_cf32 (&_w, &s->sym_rot, 1);
}

int
mpsk_receiver_set_state (mpsk_receiver_state_t *s, const void *blob)
{
  DP_SET_OPEN (MPSK_RECEIVER_STATE_MAGIC, MPSK_RECEIVER_STATE_VERSION,
               mpsk_receiver_state_bytes (s));
  DP_R_CHILD (&_r, carrier_nda, &s->car);
  DP_R_CHILD (&_r, symsync, &s->sync);
  DP_R_CHILD (&_r, fir, s->mf);
  s->tracking      = (int)dp_r_u32 (&_r);
  s->sym_count     = (size_t)dp_r_u64 (&_r);
  s->have_prev_idx = (int)dp_r_u32 (&_r);
  s->prev_idx      = dp_r_u32 (&_r);
  dp_r_cf32 (&_r, &s->sym_rot, 1);
  return DP_OK;
}

/* Process one input sample. On a recovered symbol boundary, write the symbol
 * to *sym and return 1; else return 0.
 *
 * Per sample: de-rotate with the shared NCO (predetection), accumulate the NDA
 * I/Q arm (carrier acquisition, blind to data/timing), and feed the de-rotated
 * sample to the matched filter and Gardner timing loop. On an on-time symbol
 * in tracking mode, refine the same NCO with the decision-directed phase
 * error. */
static int
process_sample (mpsk_receiver_state_t *rx, float complex x, float complex *sym)
{
  float complex d = carrier_nda_wipeoff (&rx->car, x); /* predetection wipe */
  double        pe, lk;
  if (carrier_nda_arm_step (&rx->car, d, &pe, &lk))
    {
      rx->car.lock += CARRIER_NDA_LOCK_ALPHA * (lk - rx->car.lock);
      if (!rx->tracking)
        carrier_nda_steer (&rx->car, pe); /* NDA acquisition steer */
    }
  float complex mf = fir_step (rx->mf, d * rx->sym_rot); /* matched filter */
  float complex y;
  if (!symsync_step (&rx->sync, mf, &y)) /* Gardner symbol timing */
    return 0;

  if (rx->tracking)
    {
      /* postdetection decision-directed carrier error on the full-SNR symbol;
       * m == 2 reduces to the BPSK Costas discriminator. Steers the shared NCO
       * at symbol rate (naturally lower loop bandwidth than NDA acquisition).
       */
      float complex ahat;
      mpsk_slice (y, rx->m, &ahat);
      double ay = (double)cabsf (y) + MPSK_RX_EPS;
      double e  = (double)cimagf (y * conjf (ahat)) / ay;
      carrier_nda_steer (&rx->car, e);
    }
  rx->sym_count++;
  /* opt-in handover: after a timing/carrier warmup and a confident lock, hand
   * the carrier from NDA acquisition to decision-directed tracking. */
  if (rx->acq_to_track && !rx->tracking && rx->sym_count >= rx->warmup_syms
      && rx->car.lock > rx->lock_thresh)
    rx->tracking = 1;
  *sym = y;
  return 1;
}

size_t
mpsk_receiver_steps_max_out (mpsk_receiver_state_t *state)
{
  (void)state;
  return 0;
}

size_t
mpsk_receiver_steps (mpsk_receiver_state_t *state, const float complex *x,
                     size_t x_len, float complex *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t i = 0; i < x_len; i++)
    {
      float complex y;
      if (process_sample (state, x[i], &y) && emitted < max_out)
        out[emitted++] = y;
    }
  return emitted;
}

/* Slice one recovered symbol to its log2(M) hard bits (LSB-first) into bits[];
 * returns the bit count. Coherent uses the absolute Gray label; differential
 * uses the Gray label of the index *difference* between consecutive symbols
 * (rotation-invariant), referencing an implicit zero-phase start. */
static int
symbol_to_bits (mpsk_receiver_state_t *rx, float complex y, uint8_t *bits)
{
  float complex ahat;
  unsigned      label = mpsk_slice (y, rx->m, &ahat);
  unsigned      out_label;
  if (rx->differential)
    {
      unsigned idx      = mpsk_gray_decode (label & (unsigned)(rx->m - 1));
      unsigned prev     = rx->have_prev_idx ? rx->prev_idx : 0u;
      unsigned diff     = (idx + (unsigned)rx->m - prev) % (unsigned)rx->m;
      rx->prev_idx      = idx;
      rx->have_prev_idx = 1;
      out_label         = mpsk_gray_encode (diff);
    }
  else
    out_label = label;
  int bps = mpsk_bps (rx->m);
  for (int b = 0; b < bps; b++)
    bits[b] = (uint8_t)((out_label >> b) & 1u);
  return bps;
}

size_t
mpsk_receiver_bits_max_out (mpsk_receiver_state_t *state)
{
  (void)state;
  return 0;
}

size_t
mpsk_receiver_bits (mpsk_receiver_state_t *state, const float complex *x,
                    size_t x_len, uint8_t *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t i = 0; i < x_len; i++)
    {
      float complex y;
      if (!process_sample (state, x[i], &y))
        continue;
      uint8_t bits[3];
      int     nb = symbol_to_bits (state, y, bits);
      for (int b = 0; b < nb && emitted < max_out; b++)
        out[emitted++] = bits[b];
    }
  return emitted;
}

double
mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state)
{
  return carrier_nda_get_norm_freq (&state->car);
}

void
mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val)
{
  carrier_nda_set_norm_freq (&state->car, val);
}

double
mpsk_receiver_get_lock (const mpsk_receiver_state_t *state)
{
  return state->car.lock;
}

double
mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state)
{
  return symsync_get_rate (&state->sync);
}

int
mpsk_receiver_get_tracking (const mpsk_receiver_state_t *state)
{
  return state->tracking;
}

int
mpsk_receiver_get_m (const mpsk_receiver_state_t *state)
{
  return state->m;
}

size_t
mpsk_receiver_get_sps (const mpsk_receiver_state_t *state)
{
  return state->sps;
}

int
mpsk_receiver_get_n (const mpsk_receiver_state_t *state)
{
  return state->n;
}
