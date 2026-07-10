#include "agc/agc_core.h"
#include "jm_simd.h"

agc_state_t *
agc_create (double ref_db, double loop_bw, double alpha)
{
  agc_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->ref_db             = ref_db;
  state->loop_bw            = loop_bw;
  state->alpha              = alpha;
  state->decim              = AGC_DECIM_DEFAULT;
  state->clip_db            = AGC_CLIP_DB_DEFAULT;
  state->gain_update_period = 1; /* default: exact per-sample agc_step() */
  state->gain_db            = 0.0;
  state->g_last             = 1.0; /* gain_db = 0 dB -> linear gain 1.0 */
  state->gain_phase         = 0;
  state->clip_lin           = (float)agc_exp10_ (state->clip_db * 0.05);
  /* Seed the detector with the reference power 10^(ref_db/10) so the
   * loop starts settled — avoids a large dB error on the first sample
   * and a log10(0) transient before any signal has arrived. */
  state->p_avg = pow (10.0, ref_db * 0.1);
  return state;
}

void
agc_destroy (agc_state_t *state)
{
  free (state);
}

void
agc_reset (agc_state_t *state)
{
  state->gain_db    = 0.0;
  state->g_last     = 1.0;
  state->gain_phase = 0;
  state->clip_lin   = (float)agc_exp10_ (state->clip_db * 0.05);
  state->p_avg      = pow (10.0, state->ref_db * 0.1);
}

int
agc_set_telemetry (agc_state_t *state, dp_tlm_t *tlm, const char *prefix,
                   uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  char name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.gain_db", prefix ? prefix : "agc");
  int id = dp_tlm_probe (tlm, name, decim);
  if (id < 0)
    return id; /* full table / overlong name: attach fails whole */
  state->tlm.id_gain = id;
  state->tlm.ctx     = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

/* Serializable state — whole-struct POD snapshot, pointer-free except the
 * telemetry attachment, which the TLM variant zeroes in blobs and keeps
 * live across restore (see DP_DEFINE_POD_STATE_TLM in dp_state.h). */
DP_DEFINE_POD_STATE_TLM (agc, agc_state_t, AGC_STATE_MAGIC, AGC_STATE_VERSION,
                         tlm)

double
agc_get_applied_gain_db (const agc_state_t *state)
{
  return 20.0 * log10 (state->g_last);
}

JM_HOT void
agc_steps (agc_state_t *state, const float complex *input,
           float complex *output, size_t n)
{
  size_t d  = state->decim ? state->decim : 1; /* chunk length, guard >=1 */
  double a1 = 1.0 - state->alpha;              /* per-sample EMA pole     */
  double g_prev = state->g_last;               /* ramp continues from here */

  /* Every full chunk shares the same control coefficients, so compute
   * them — including the reciprocal chunk length used for averaging —
   * once here rather than per chunk.  The supported decim values
   * (8/16/32) are powers of two, so 1.0/d is exact and the multiply
   * below is bit-identical to a divide. */
  double inv_d = 1.0 / (double)d;
  double ac    = 1.0;
  for (size_t k = 0; k < d; k++)
    ac *= a1;
  double alpha_d = 1.0 - ac;                         /* EMA pole over d  */
  double k_d     = (double)d * 4.0 * state->loop_bw; /* loop-filter gain */

  /* Output clip threshold, linear amplitude — constant for the call. */
  float clip_lin = (float)agc_exp10_ (state->clip_db * 0.05);

  for (size_t i = 0; i < n; i += d)
    {
      size_t c     = n - i < d ? n - i : d; /* this chunk's length */
      double inv_c = inv_d, alpha_c = alpha_d, k_c = k_d;
      if (c != d) /* final short chunk: rescale to its actual length */
        {
          inv_c = 1.0 / (double)c;
          ac    = 1.0;
          for (size_t k = 0; k < c; k++)
            ac *= a1;
          alpha_c = 1.0 - ac;
          k_c     = (double)c * 4.0 * state->loop_bw;
        }

      /* Linear gain interpolation (first-order hold): ramp from the gain
       * applied at the end of the previous chunk to the gain the loop
       * currently commands, so the applied gain has no inter-chunk
       * staircase.  At convergence g_target == g_prev and the ramp is a
       * constant.  gj is affine in j, so the loop still vectorises. */
      double g_target = agc_exp10_ (state->gain_db * 0.05);
      double dg       = (g_target - g_prev) * inv_c;

      /* Apply the interpolated gain ramp.  gj is affine in j, so this
       * loop vectorises cleanly. */
      for (size_t j = 0; j < c; j++)
        {
          float gj      = (float)(g_prev + (double)(j + 1) * dg);
          output[i + j] = input[i + j] * gj;
        }
      g_prev = g_target; /* ramp endpoint -> start of the next chunk */

      /* Chunk power via an explicit-SIMD sum-of-squares.  A cf32 sample
       * is an adjacent (re, im) float pair, so the 2*c floats starting
       * at &output[i] cover exactly this chunk and Sum(float^2) equals
       * Sum(|y|^2).  For the power-of-two decim chunks this is a pure
       * vector reduction with no scalar remainder. */
      float psum;
      JM_SUMSQ_F32 (psum, (const float *)&output[i], 2 * c);

      /* Square-clip the chunk's output to clip_db via the shared util
       * primitive.  Done after the power sum, so the detector still
       * sees the unclipped signal — clipping never perturbs the loop. */
      for (size_t j = 0; j < c; j++)
        output[i + j] = square_clip (output[i + j], clip_lin);

      /* Control update — once per chunk, with the rescaled coefficients. */
      double p_mean = (double)psum * inv_c;
      state->p_avg += alpha_c * (p_mean - state->p_avg);
      double meas_db = 10.0 * agc_log10_ (state->p_avg + AGC_POWER_FLOOR);
      state->gain_db += k_c * (state->ref_db - meas_db);
      /* Telemetry tap — per chunk update (event rate, not sample rate). */
      DP_TLM (state->tlm.ctx, state->tlm.id_gain, state->gain_db);
    }
  state->g_last = g_prev; /* persist for the next agc_steps() call */
}
