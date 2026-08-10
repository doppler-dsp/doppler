/**
 * @file lo_core.c
 * @brief Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors.
 *
 * Scalar throughout.  Two AVX-512 kernels lived here and were removed:
 * `-march=x86-64-v2` is the shipped baseline, so `__AVX512F__` was never
 * defined in any wheel or CI job and neither kernel was compiled, let
 * alone tested — while the `steps_ctrl` one folded its control in
 * float32 where the scalar path folds in double through the shared
 * primitive, so the same object would have produced different phase
 * increments on the one configuration that did build it.  Two
 * implementations, one of them unreachable and the two disagreeing, is
 * strictly worse than one.  See docs/design/nco.md §10.
 */
#include "lo/lo_core.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Sine LUT — 2^16 single-precision entries.
 *
 * LUT index is the top 16 bits of the 32-bit phase accumulator:
 *   idx    = (uint16_t)(phase >> 16)
 *   sin(θ) = lo_sin_lut[idx]
 *   cos(θ) = lo_sin_lut[(uint16_t)(idx + LO_LUT_QTR)]
 *
 * LO_LUT_QTR = N/4 = 16384 shifts by π/2, mapping sin → cos without
 * extra storage.  The uint16_t cast wraps at 65536 branchlessly.
 *
 * 65536 floats × 4 bytes = 256 KB — fits in L2 on all modern CPUs.
 * SFDR: ~96 dBc from 16-bit phase truncation.
 * ------------------------------------------------------------------ */
/* LO_LUT_BITS / LO_LUT_SIZE / LO_LUT_QTR are defined in lo_core.h so the
 * inline lo_step() in the header can share them. */

/* Definition of the LUT declared `extern` in lo_core.h.  Filled lazily by
 * lut_init() on the first lo_create()/lo_init(); read-only afterwards. */
float      lo_sin_lut[LO_LUT_SIZE];
static int lut_ready = 0;

static void
lut_init (void)
{
  if (lut_ready)
    return;
  for (unsigned i = 0; i < LO_LUT_SIZE; i++)
    lo_sin_lut[i] = sinf (2.0f * (float)M_PI * (float)i / (float)LO_LUT_SIZE);
  lut_ready = 1;
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

void
lo_init (lo_state_t *state, double norm_freq)
{
  lut_init ();
  state->phase     = 0;
  state->phase_inc = nco_norm_freq_to_inc (norm_freq);
  state->norm_freq = norm_freq;
}

lo_state_t *
lo_create (double norm_freq)
{
  lo_state_t *state = malloc (sizeof (*state));
  if (!state)
    return NULL;
  lo_init (state, norm_freq);
  return state;
}

void
lo_destroy (lo_state_t *state)
{
  free (state);
}

void
lo_reset (lo_state_t *state)
{
  state->phase = 0;
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

double
lo_get_norm_freq (const lo_state_t *state)
{
  return state->norm_freq;
}

void
lo_set_norm_freq (lo_state_t *state, double norm_freq)
{
  state->phase_inc = nco_norm_freq_to_inc (norm_freq);
  state->norm_freq = norm_freq;
}

uint32_t
lo_get_phase (const lo_state_t *state)
{
  return state->phase;
}

void
lo_set_phase (lo_state_t *state, uint32_t phase)
{
  state->phase = phase;
}

/* ── Serializable state — standard envelope + phase (the only per-sample
 * state); see dp_state.h. ───────────────────────────────────────────────── */

size_t
lo_state_bytes (const lo_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t);
}

void
lo_get_state (const lo_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, lo_state_bytes (state));
  dp_w_hdr (&w, LO_STATE_MAGIC, LO_STATE_VERSION, lo_state_bytes (state));
  dp_w_u32 (&w, state->phase);
}

int
lo_set_state (lo_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, lo_state_bytes (state), LO_STATE_MAGIC,
                              LO_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, lo_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  state->phase  = dp_r_u32 (&r);
  return DP_OK;
}

uint32_t
lo_get_phase_inc (const lo_state_t *state)
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
#define LO_MAX_OUT 65536u

size_t
lo_steps_max_out (lo_state_t *state)
{
  (void)state;
  return LO_MAX_OUT;
}

size_t
lo_steps_ctrl_max_out (lo_state_t *state)
{
  (void)state;
  return LO_MAX_OUT;
}

/* ================================================================== */
/* Execute — free-running                                              */
/* ================================================================== */

size_t
lo_steps (lo_state_t *state, size_t n, float complex *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;
  for (size_t i = 0; i < n; i++)
    {
      uint16_t idx = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc;
    }
  state->phase = ph;
  return n;
}

/* ================================================================== */
/* Execute — with per-sample FM control port                           */
/* ================================================================== */

size_t
lo_steps_ctrl (lo_state_t *state, const double *ctrl, size_t ctrl_len,
               float complex *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (ctrl_len > max_out)
    ctrl_len = max_out;
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;
  for (size_t i = 0; i < ctrl_len; i++)
    {
      uint32_t ctrl_inc = nco_norm_freq_to_inc (ctrl[i]);
      uint16_t idx      = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc + ctrl_inc;
    }
  state->phase = ph;
  return ctrl_len;
}
