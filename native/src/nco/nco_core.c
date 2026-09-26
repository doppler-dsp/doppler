/**
 * @file nco_core.c
 * @brief Pure 32-bit phase-accumulator NCO implementation.
 *
 * The three execute bodies (steps_u32, steps_u32_scaled, steps_u32_ovf)
 * were lifted from the doppler reference (native/src/nco/nco_core.c) via
 * just-makeit --impl.  Only create/reset/max_out/properties are hand-written.
 */
#include "doppler/nco/nco_core.h"

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

dp_nco_state_t *
dp_nco_create (double norm_freq, uint32_t nmax)
{
  dp_nco_state_t *state = malloc (sizeof (*state));
  if (!state)
    return NULL;
  state->phase     = 0;
  state->phase_inc = nco_norm_freq_to_inc (norm_freq);
  state->norm_freq = norm_freq;
  state->nmax      = nmax;
  return state;
}

void
dp_nco_destroy (dp_nco_state_t *state)
{
  free (state);
}

void
dp_nco_reset (dp_nco_state_t *state)
{
  state->phase = 0;
}

/* ── Serializable state — standard envelope (see dp_state.h) ────────────────
 * Only the running phase accumulator; phase_inc / nmax come from create(). */

size_t
dp_nco_state_bytes (const dp_nco_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t);
}

void
dp_nco_get_state (const dp_nco_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, dp_nco_state_bytes (state));
  dp_w_hdr (&w, NCO_STATE_MAGIC, NCO_STATE_VERSION,
            dp_nco_state_bytes (state));
  dp_w_u32 (&w, state->phase);
}

int
dp_nco_set_state (dp_nco_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, dp_nco_state_bytes (state),
                              NCO_STATE_MAGIC, NCO_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, dp_nco_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  state->phase  = dp_r_u32 (&r);
  return DP_OK;
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

double
dp_nco_get_norm_freq (const dp_nco_state_t *state)
{
  return state->norm_freq;
}

void
dp_nco_set_norm_freq (dp_nco_state_t *state, double norm_freq)
{
  state->phase_inc = nco_norm_freq_to_inc (norm_freq);
  state->norm_freq = norm_freq;
}

uint32_t
dp_nco_get_phase (const dp_nco_state_t *state)
{
  return state->phase;
}

void
dp_nco_set_phase (dp_nco_state_t *state, uint32_t phase)
{
  state->phase = phase;
}

uint32_t
dp_nco_get_phase_inc (const dp_nco_state_t *state)
{
  return state->phase_inc;
}

/* ================================================================== */
/* Block generators                                                    */
/* ================================================================== */

/*
 * Pre-allocated buffer size for all generator methods: what the Python
 * binding allocates at create time, NOT a ceiling on the call.  Since
 * pass_capacity (jm gh-138) every generator here is told the caller's
 * capacity and clamps to it, and the binding grows its buffer on demand,
 * so a larger request is served rather than overrunning anything.  This
 * comment claimed the opposite until it was measured.
 */
#define NCO_MAX_OUT 65536u

size_t
dp_nco_steps_u32_max_out (dp_nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

/* Every batch stepper below is a thin loop over the matching
   nco_core.h single-sample primitive (nco_step_u32*) -- the per-sample
   phase-advance arithmetic lives in exactly ONE place (the header), not
   duplicated in each of these six loop bodies. */

size_t
dp_nco_steps_u32 (dp_nco_state_t *state, size_t n, uint32_t *out,
                  size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  for (size_t i = 0; i < n; i++)
    out[i] = nco_step_u32 (state);
  return n;
}

size_t
dp_nco_steps_u32_scaled_max_out (dp_nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
dp_nco_steps_u32_scaled (dp_nco_state_t *state, size_t n, uint32_t *out,
                         size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  for (size_t i = 0; i < n; i++)
    out[i] = nco_step_u32_scaled (state);
  return n;
}

size_t
dp_nco_steps_u32_ovf_max_out (dp_nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
dp_nco_steps_u32_ovf (dp_nco_state_t *state, size_t n, uint32_t *out,
                      uint8_t *out1, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  for (size_t i = 0; i < n; i++)
    out[i] = nco_step_u32_ovf (state, &out1[i]);
  return n;
}

size_t
dp_nco_steps_u32_ctrl_max_out (dp_nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
dp_nco_steps_u32_ctrl (dp_nco_state_t *state, const double *ctrl,
                       size_t ctrl_len, uint32_t *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (ctrl_len > max_out)
    ctrl_len = max_out;
  for (size_t i = 0; i < ctrl_len; i++)
    out[i] = nco_step_u32_ctrl (state, ctrl[i]);
  return ctrl_len;
}

size_t
dp_nco_steps_u32_scaled_ctrl_max_out (dp_nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
dp_nco_steps_u32_scaled_ctrl (dp_nco_state_t *state, const double *ctrl,
                              size_t ctrl_len, uint32_t *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (ctrl_len > max_out)
    ctrl_len = max_out;
  for (size_t i = 0; i < ctrl_len; i++)
    out[i] = nco_step_u32_scaled_ctrl (state, ctrl[i]);
  return ctrl_len;
}

size_t
dp_nco_steps_u32_ovf_ctrl_max_out (dp_nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
dp_nco_steps_u32_ovf_ctrl (dp_nco_state_t *state, const double *ctrl,
                           size_t ctrl_len, uint32_t *out, uint8_t *out1,
                           size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (ctrl_len > max_out)
    ctrl_len = max_out;
  for (size_t i = 0; i < ctrl_len; i++)
    out[i] = nco_step_u32_ovf_ctrl (state, ctrl[i], &out1[i]);
  return ctrl_len;
}
