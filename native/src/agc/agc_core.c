#include "agc/agc_core.h"
#include "dp_simd.h"
#include "util/util_core.h"
#include <float.h>

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

size_t
agc_settling_samples (double loop_bw, double alpha, double gain_err_db,
                      double tol_db)
{
  /* Refuse rather than guess: every one of these makes the question
     meaningless, and a plausible number would be worse than none. */
  if (!(loop_bw > 0.0) || !(alpha > 0.0) || !(alpha <= 1.0) || !(tol_db > 0.0)
      || !isfinite (gain_err_db))
    return 0;
  /* Already inside the tolerance: settled at sample zero, but the contract
     says >= 1 for an answer, so report the first sample. */
  if (fabs (gain_err_db) <= tol_db)
    return 1;

  agc_state_t *s = agc_create (0.0, loop_bw, alpha);
  if (!s)
    return 0;

  /* An input needing `gain_err_db` of gain to reach a 0 dB reference. The
     direction is the caller's: positive means quiet, which is the slow
     case because the detector's dB reading crawls up a concave log. */
  float complex x = (float)pow (10.0, -gain_err_db / 20.0) * (0.6f + 0.8f * I);

  /* Bounded search. The measured multiplier tops out near 5 for the
     slowest detector this object accepts; 64 filter time constants is an
     order of magnitude of headroom over that, and a loop that has not
     settled by then is not going to. */
  size_t budget = (size_t)(64.0 / (4.0 * loop_bw)) + 1u;
  size_t n      = 0;
  for (; n < budget; n++)
    {
      (void)agc_step (s, x);
      if (fabs (s->gain_db - gain_err_db) <= tol_db)
        break;
    }
  agc_destroy (s);
  return n < budget ? n + 1u : 0u;
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
  const char *p = prefix ? prefix : "agc";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.gain_db", p);
  int id_gain = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.level_db", p);
  int id_level = dp_tlm_probe (tlm, name, decim);
  if (id_gain < 0 || id_level < 0)
    return DP_ERR_INVALID; /* full table / overlong name: fails whole */
  state->tlm.id_gain  = id_gain;
  state->tlm.id_level = id_level;
  state->tlm.ctx      = tlm; /* set last: emit sites gate on ctx */
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
  /* Total, for the same reason the detector's input is (see AGC_POWER_CEIL).
     g_last underflows to 0 for an extreme commanded gain -- agc_exp10_
     saturates low rather than returning a denormal -- and 20*log10(0) is
     -INF, which is a non-finite value escaping through a PUBLIC accessor
     even though the state behind it is perfectly well-formed. Saturating to
     the smallest normal double keeps the reading finite while leaving it
     unmistakably "off" (about -6151 dB); NaN takes the same low rail, on the
     rule this object uses everywhere: when the input is unknown, attenuate. */
  return 20.0 * log10 (saturate (state->g_last, DBL_MIN, DBL_MAX, DBL_MIN));
}

JM_HOT void
agc_steps (agc_state_t *state, const float complex *input,
           float complex *output, size_t n)
{
  size_t d      = state->decim ? state->decim : 1; /* chunk len, >=1 */
  double g_prev = state->g_last; /* ramp continues from here */

  /* Every full chunk shares the same control coefficients, so compute
   * them — including the reciprocal chunk length used for averaging —
   * once here rather than per chunk.  The supported decim values
   * (8/16/32) are powers of two, so 1.0/d is exact and the multiply
   * below is bit-identical to a divide. */
  double inv_d = 1.0 / (double)d;
  /* The detector's pole, compounded over the chunk by the shared
     primitive.  This used to be a repeated multiply of (1 - alpha)
     followed by `1 - ac`, which is catastrophic cancellation: at d == 1,
     where the answer must be alpha itself, it was 6 ulps off at alpha
     0.05 and 26865 off at 1e-5.  ema_alpha_decim is exact there, which
     is what makes `decim = 1` genuinely the undecimated recursion and
     therefore comparable to the per-sample path at all.  See
     docs/design/ema.md §6 and doppler#698. */
  double alpha_d = ema_alpha_decim (state->alpha, d);
  /* The loop filter's gain, compounded the same way and for the same
     reason.  The closed-loop error decays by (1 - k1) per sample with
     k1 = 4*loop_bw, so over d samples it decays by (1 - k1)^d; a chunked
     update applying d*k1 is the RECTANGULAR approximation to that, and
     (1 - d*k1) is always the smaller, so a larger decim always converged
     FASTER.  That was the 2.53 dB spread §23 recorded and declined to
     assert.  The divergence is second order in d*k1 -- at decim 32 with
     loop_bw 0.0025 the header's own `loop_bw << 1/(4*decim)` precondition
     is only 3x, which is why it showed there first.  See doppler#699. */
  double k_d = ema_alpha_decim (4.0 * state->loop_bw, d);

  /* Output clip threshold, linear amplitude — constant for the call. */
  float clip_lin = (float)agc_exp10_ (state->clip_db * 0.05);

  for (size_t i = 0; i < n; i += d)
    {
      size_t c     = n - i < d ? n - i : d; /* this chunk's length */
      double inv_c = inv_d, alpha_c = alpha_d, k_c = k_d;
      if (c != d) /* final short chunk: rescale to its actual length */
        {
          inv_c   = 1.0 / (double)c;
          alpha_c = ema_alpha_decim (state->alpha, c);
          k_c     = ema_alpha_decim (4.0 * state->loop_bw, c);
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
      DP_SUMSQ_F32 (psum, (const float *)&output[i], 2 * c);

      /* Square-clip the chunk's output to clip_db via the shared util
       * primitive.  Done after the power sum, so the detector still
       * sees the unclipped signal — clipping never perturbs the loop. */
      for (size_t j = 0; j < c; j++)
        output[i + j] = square_clip (output[i + j], clip_lin);

      /* Control update — once per chunk, with the rescaled coefficients. */
      /* The detector's input is the AGC's one safety boundary — see
         AGC_POWER_CEIL.  psum is a float reduction, so an overflowing chunk
         arrives here as an infinity; saturate() sends that, and any NaN, to
         the ceiling rather than into p_avg. */
      double p_mean  = saturate ((double)psum * inv_c, 0.0, AGC_POWER_CEIL,
                                 AGC_POWER_CEIL);
      state->p_avg   = ema_step (state->p_avg, p_mean, alpha_c);
      double meas_db = 10.0 * agc_log10_ (state->p_avg + AGC_POWER_FLOOR);
      state->gain_db += k_c * (state->ref_db - meas_db);
      /* Telemetry tap — per chunk update (event rate, not sample rate).
         Same pairing as agc_step(): the command, then the measured level
         that command was answering. */
      DP_TLM (state->tlm.ctx, state->tlm.id_gain, state->gain_db);
      DP_TLM (state->tlm.ctx, state->tlm.id_level, meas_db);
    }
  state->g_last = g_prev; /* persist for the next agc_steps() call */
}
