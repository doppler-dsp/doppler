/**
 * @file nco_core.c
 * @brief Pure 32-bit phase-accumulator NCO implementation.
 *
 * The three execute bodies (steps_u32, steps_u32_scaled, steps_u32_ovf)
 * were lifted from the doppler reference (native/src/nco/nco_core.c) via
 * just-makeit --impl.  Only create/reset/max_out/properties are hand-written.
 */
#include "nco/nco_core.h"

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

nco_state_t *
nco_create (double norm_freq, uint32_t nmax)
{
  nco_state_t *state = malloc (sizeof (*state));
  if (!state)
    return NULL;
  state->phase     = 0;
  state->phase_inc = nco_norm_to_inc (norm_freq);
  state->norm_freq = norm_freq;
  state->nmax      = nmax;
  return state;
}

void
nco_destroy (nco_state_t *state)
{
  free (state);
}

void
nco_reset (nco_state_t *state)
{
  state->phase = 0;
}

/* ── Serializable state — standard envelope (see dp_state.h) ────────────────
 * Only the running phase accumulator; phase_inc / nmax come from create(). */

size_t
nco_state_bytes (const nco_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t);
}

void
nco_get_state (const nco_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, nco_state_bytes (state));
  dp_w_hdr (&w, NCO_STATE_MAGIC, NCO_STATE_VERSION, nco_state_bytes (state));
  dp_w_u32 (&w, state->phase);
}

int
nco_set_state (nco_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, nco_state_bytes (state), NCO_STATE_MAGIC,
                              NCO_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, nco_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  state->phase  = dp_r_u32 (&r);
  return DP_OK;
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

double
nco_get_norm_freq (const nco_state_t *state)
{
  return state->norm_freq;
}

void
nco_set_norm_freq (nco_state_t *state, double norm_freq)
{
  state->phase_inc = nco_norm_to_inc (norm_freq);
  state->norm_freq = norm_freq;
}

uint32_t
nco_get_phase (const nco_state_t *state)
{
  return state->phase;
}

void
nco_set_phase (nco_state_t *state, uint32_t phase)
{
  state->phase = phase;
}

uint32_t
nco_get_phase_inc (const nco_state_t *state)
{
  return state->phase_inc;
}

/* ================================================================== */
/* Block generators                                                    */
/* ================================================================== */

/*
 * Pre-allocated buffer size for all generator methods.  The Python
 * extension allocates output buffers of this size at create time; calling
 * with n > 65536 overflows the buffer and is undefined behaviour.
 */
#define NCO_MAX_OUT 65536u

size_t
nco_steps_u32_max_out (nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

/* Every batch stepper below is a thin loop over the matching
   nco_core.h single-sample primitive (nco_step_u32*) -- the per-sample
   phase-advance arithmetic lives in exactly ONE place (the header), not
   duplicated in each of these six loop bodies. */

size_t
nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out)
{
  for (size_t i = 0; i < n; i++)
    out[i] = nco_step_u32 (state);
  return n;
}

size_t
nco_steps_u32_scaled_max_out (nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out)
{
  for (size_t i = 0; i < n; i++)
    out[i] = nco_step_u32_scaled (state);
  return n;
}

size_t
nco_steps_u32_ovf_max_out (nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
nco_steps_u32_ovf (nco_state_t *state, size_t n, uint32_t *out, uint8_t *out1)
{
  for (size_t i = 0; i < n; i++)
    out[i] = nco_step_u32_ovf (state, &out1[i]);
  return n;
}

size_t
nco_steps_u32_ctrl_max_out (nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
nco_steps_u32_ctrl (nco_state_t *state, const float *ctrl, size_t ctrl_len,
                    uint32_t *out)
{
  for (size_t i = 0; i < ctrl_len; i++)
    out[i] = nco_step_u32_ctrl (state, (double)ctrl[i]);
  return ctrl_len;
}

size_t
nco_steps_u32_scaled_ctrl_max_out (nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
nco_steps_u32_scaled_ctrl (nco_state_t *state, const float *ctrl,
                           size_t ctrl_len, uint32_t *out)
{
  for (size_t i = 0; i < ctrl_len; i++)
    out[i] = nco_step_u32_scaled_ctrl (state, (double)ctrl[i]);
  return ctrl_len;
}

size_t
nco_steps_u32_ovf_ctrl_max_out (nco_state_t *state)
{
  (void)state;
  return NCO_MAX_OUT;
}

size_t
nco_steps_u32_ovf_ctrl (nco_state_t *state, const float *ctrl, size_t ctrl_len,
                        uint32_t *out, uint8_t *out1)
{
  for (size_t i = 0; i < ctrl_len; i++)
    out[i] = nco_step_u32_ovf_ctrl (state, (double)ctrl[i], &out1[i]);
  return ctrl_len;
}
