/*
 * mpsk_receiver_r_core.c — the real-input M-PSK receiver.
 *
 * Deliberately thin. Every loop, discriminator, handover rule and demapper
 * decision lives in mpsk_rx_loops.{h,c} and is shared verbatim with the
 * complex twin; what is left here is the front end (a matched DDCR instead of
 * a matched DDC), the two rate conversions its halfband forces, and the
 * block API. If a receiver behaviour needs changing, it does NOT change here.
 */
#include "mpsk_receiver_r/mpsk_receiver_r_core.h"

#include <stdlib.h>

mpsk_receiver_r_state_t *
mpsk_receiver_r_create (int m, double sps, size_t m_out, int pulse,
                        double rrc_beta, int rrc_span, double bn_carrier,
                        double zeta, double bn_timing, int acq_to_track,
                        double lock_thresh, double init_norm_freq,
                        size_t warmup_syms, int differential,
                        size_t num_phases, int nda_tap)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL;
  if (pulse != MPSK_RX_PULSE_IANDD && pulse != MPSK_RX_PULSE_RRC)
    return NULL;
  /* Written as !(x > y) so a NaN parameter is rejected, not accepted. The
     bound is 2*m_out, not m_out: the cascade behind the R2C halfband runs at
     twice the overall rate, and Ddcr requires that below 0.5. */
  if (m_out < 2u || m_out > (size_t)RATESYNC_MAX_M || (m_out & 1u) != 0u
      || !(sps > 2.0 * (double)m_out) || !(rrc_beta >= 0.0)
      || !(rrc_beta <= 1.0) || rrc_span < 1 || !(bn_carrier >= 0.0)
      || !(bn_timing >= 0.0) || !(zeta > 0.0) || num_phases < 2u
      || (num_phases & (num_phases - 1u)) != 0u
      || nda_tap < MPSK_RX_NDA_TAP_STROBE || nda_tap > MPSK_RX_NDA_TAP_LO_ARM)
    return NULL;

  mpsk_receiver_r_state_t *rx = calloc (1, sizeof (*rx));
  if (!rx)
    return NULL;

  /* The one conversion this type owns. Ddcr's LO runs at the INTERMEDIATE
     rate (fs_in/2) and the R2C halfband has an fs/4 shift baked into it, so
     tuning a real tone at input-normalised f_c to DC is
     norm_freq = -(2*f_c + 0.5) -- ddcr_core.h's own contract, not a guess.
     Differentiating it gives the readback scaling used below: a deviation of
     d at the intermediate rate is d/2 in input-normalised terms. */
  rx->fe = ddcr_create_matched (-(2.0 * init_norm_freq + 0.5),
                                (double)m_out / sps, pulse, rrc_beta,
                                (size_t)rrc_span, (double)m_out, num_phases);
  if (!rx->fe)
    {
      free (rx);
      return NULL;
    }
  rx->centre_freq = init_norm_freq;

  /* lo_sps = sps/2: the halfband decimates 2:1 before the mix, so the LO sees
     half as many samples per symbol as the input does. This is the whole
     reason mpsk_rx_loops_init takes lo_sps separately from sps. */
  mpsk_rx_loops_init (&rx->l, m, sps, 0.5 * sps, m_out, bn_carrier, zeta,
                      bn_timing, RATESYNC_TED_GARDNER, acq_to_track,
                      lock_thresh, warmup_syms, differential, nda_tap);
  ratesync_loop_bind_cascade (&rx->l.timing, rx->fe->rc);
  return rx;
}

void
mpsk_receiver_r_destroy (mpsk_receiver_r_state_t *state)
{
  if (!state)
    return;
  ddcr_destroy (state->fe);
  free (state);
}

void
mpsk_receiver_r_reset (mpsk_receiver_r_state_t *state)
{
  ddcr_reset (state->fe);
  mpsk_rx_loops_reset (&state->l);
}

size_t
mpsk_receiver_r_steps_max_out (mpsk_receiver_r_state_t *state)
{
  (void)state;
  return 0; /* sps > 2*m_out, so symbols <= inputs */
}

size_t
mpsk_receiver_r_steps (mpsk_receiver_r_state_t *state, const float *x,
                       size_t x_len, float complex *out, size_t max_out)
{
  size_t emitted = 0;
  /* Telemetry hoisted to loop entry, as the complex twin: the detached loop
     carries no call site, so the hot loop state stays in registers. */
  if (!state->l.tlm.ctx)
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex y;
          if (mpsk_receiver_r_step_ted (state, x[i], &y, RATESYNC_TED_GARDNER)
              && emitted < max_out)
            out[emitted++] = y;
        }
    }
  else
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex y;
          if (mpsk_receiver_r_step_ted (state, x[i], &y, RATESYNC_TED_GARDNER))
            {
              if (emitted < max_out)
                out[emitted++] = y;
              mpsk_rx_tlm_flush (&state->l);
            }
        }
    }
  return emitted;
}

size_t
mpsk_receiver_r_bits_max_out (mpsk_receiver_r_state_t *state)
{
  (void)state;
  return 0;
}

size_t
mpsk_receiver_r_bits (mpsk_receiver_r_state_t *state, const float *x,
                      size_t x_len, uint8_t *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t i = 0; i < x_len; i++)
    {
      float complex y;
      if (!mpsk_receiver_r_step_ted (state, x[i], &y, RATESYNC_TED_GARDNER))
        continue;
      if (state->l.tlm.ctx)
        mpsk_rx_tlm_flush (&state->l);
      uint8_t bits[3];
      int     nb = mpsk_rx_symbol_to_bits (&state->l, y, bits);
      for (int b = 0; b < nb && emitted < max_out; b++)
        out[emitted++] = bits[b];
    }
  return emitted;
}

double
mpsk_receiver_r_get_norm_freq (const mpsk_receiver_r_state_t *state)
{
  /* The loop's estimate is in cycles/sample at the LO's (intermediate) rate.
     Ddcr's tuning law is norm_freq = -(2*f_c + 0.5), so an intermediate-rate
     deviation is HALF as many cycles/sample at the real input rate. */
  return state->centre_freq + 0.5 * mpsk_rx_freq_est (&state->l);
}

double
mpsk_receiver_r_get_nco_freq (const mpsk_receiver_r_state_t *state)
{
  return state->centre_freq - 0.5 * state->l.freq_ctrl;
}

void
mpsk_receiver_r_set_norm_freq (mpsk_receiver_r_state_t *state, double val)
{
  state->centre_freq = val;
  ddcr_set_norm_freq (state->fe, -(2.0 * val + 0.5));
  mpsk_rx_set_freq_est (&state->l, 0.0);
}

double
mpsk_receiver_r_get_lock (const mpsk_receiver_r_state_t *state)
{
  return state->l.lock;
}

int
mpsk_receiver_r_get_locked (const mpsk_receiver_r_state_t *state)
{
  return state->l.car_lock.locked;
}

double
mpsk_receiver_r_get_last_error (const mpsk_receiver_r_state_t *state)
{
  return state->l.car_error;
}

void
mpsk_receiver_r_configure_lock (mpsk_receiver_r_state_t *state,
                                double up_thresh, double down_thresh,
                                uint32_t n_up, uint32_t n_down)
{
  mpsk_rx_configure_lock (&state->l, up_thresh, down_thresh, n_up, n_down);
}

int
mpsk_receiver_r_set_telemetry (mpsk_receiver_r_state_t *state, dp_tlm_t *tlm,
                               const char *prefix, uint32_t decim)
{
  return mpsk_rx_set_telemetry (&state->l, tlm, prefix, decim);
}

double
mpsk_receiver_r_get_timing_rate (const mpsk_receiver_r_state_t *state)
{
  return state->l.timing.rate_est;
}

int
mpsk_receiver_r_get_tracking (const mpsk_receiver_r_state_t *state)
{
  return state->l.tracking;
}

int
mpsk_receiver_r_get_m (const mpsk_receiver_r_state_t *state)
{
  return state->l.m;
}

double
mpsk_receiver_r_get_sps (const mpsk_receiver_r_state_t *state)
{
  return state->l.sps;
}

size_t
mpsk_receiver_r_get_m_out (const mpsk_receiver_r_state_t *state)
{
  return state->l.m_out;
}

int
mpsk_receiver_r_get_clipped (const mpsk_receiver_r_state_t *state)
{
  return ddcr_get_clipped (state->fe) ? 1 : 0;
}

/* ── Serializable state — two children, no scalars of our own ───────────────
 */

size_t
mpsk_receiver_r_state_bytes (const mpsk_receiver_r_state_t *s)
{
  return sizeof (dp_state_hdr_t) + ddcr_state_bytes (s->fe)
         + mpsk_rx_loops_state_bytes (&s->l);
}

void
mpsk_receiver_r_get_state (const mpsk_receiver_r_state_t *s, void *blob)
{
  const size_t total = mpsk_receiver_r_state_bytes (s);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, MPSK_RECEIVER_R_STATE_MAGIC, MPSK_RECEIVER_R_STATE_VERSION,
            total);
  char *p = (char *)blob + w.off;
  ddcr_get_state (s->fe, p);
  p += ddcr_state_bytes (s->fe);
  mpsk_rx_loops_get_state (&s->l, p);
}

int
mpsk_receiver_r_set_state (mpsk_receiver_r_state_t *s, const void *blob)
{
  const size_t total = mpsk_receiver_r_state_bytes (s);
  int rc = dp_state_validate (blob, total, MPSK_RECEIVER_R_STATE_MAGIC,
                              MPSK_RECEIVER_R_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  const char *p = (const char *)blob + sizeof (dp_state_hdr_t);
  rc            = ddcr_set_state (s->fe, p);
  if (rc != DP_OK)
    return rc;
  p += ddcr_state_bytes (s->fe);
  return mpsk_rx_loops_set_state (&s->l, p);
}
