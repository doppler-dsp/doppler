/**
 * @file acc_trace_core.c
 * @brief AccTrace — per-bin vector trace accumulator (mean/EMA/max/min hold).
 *
 * The running trace is held in double precision so that the linear mean stays
 * accurate over thousands of frames; input and output are float32.  The first
 * frame seeds the trace in every mode, which makes max/min-hold start from a
 * real sample (not +/-inf sentinels) and the EMA start unbiased.
 */
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/util/util_core.h"

/* The one alpha rule, for create() and the setter alike.  Only the EMA reads
 * alpha, and there it must be a smoothing factor in (0, 1]: 0 never leaves
 * the first frame, a negative one extrapolates away from the data, and above
 * 1 dp_ema_step saturates to pass-through -- none of them is an average.
 * (dp_ema_step itself keeps alpha = 0 as "freeze"; this is AccTrace's
 * contract, not the primitive's.)  The negated form also refuses a NaN. */
static int
acc_trace_alpha_ok (int mode, double alpha)
{
  return mode != ACC_TRACE_EXP || (alpha > 0.0 && alpha <= 1.0);
}

dp_acc_trace_state_t *
dp_acc_trace_create (size_t n, int mode, double alpha)
{
  if (n == 0 || mode < ACC_TRACE_MEAN || mode > ACC_TRACE_MINHOLD)
    return NULL;
  if (!acc_trace_alpha_ok (mode, alpha))
    return NULL;

  dp_acc_trace_state_t *s = (dp_acc_trace_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->acc = (double *)calloc (n, sizeof (double));
  if (!s->acc)
    {
      free (s);
      return NULL;
    }

  s->n     = n;
  s->mode  = (acc_trace_mode_t)mode;
  s->alpha = alpha;
  s->count = 0;
  return s;
}

int
dp_acc_trace_set_alpha (dp_acc_trace_state_t *state, double alpha)
{
  if (!acc_trace_alpha_ok ((int)state->mode, alpha))
    return DP_ERR_INVALID;
  state->alpha = alpha;
  return DP_OK;
}

void
dp_acc_trace_destroy (dp_acc_trace_state_t *state)
{
  if (!state)
    return;
  free (state->acc);
  free (state);
}

void
dp_acc_trace_reset (dp_acc_trace_state_t *state)
{
  memset (state->acc, 0, state->n * sizeof (double));
  state->count = 0;
}

/* Serializable state — running trace + fold count; config restored by
 * create(). */
size_t
dp_acc_trace_state_bytes (const dp_acc_trace_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (uint64_t) + s->n * sizeof (double);
}

void
dp_acc_trace_get_state (const dp_acc_trace_state_t *s, void *blob)
{
  DP_GET_OPEN (ACC_TRACE_STATE_MAGIC, ACC_TRACE_STATE_VERSION,
               dp_acc_trace_state_bytes (s));
  dp_w_u64 (&_w, s->count);
  dp_w_bytes (&_w, s->acc, s->n * sizeof (double));
}

int
dp_acc_trace_set_state (dp_acc_trace_state_t *s, const void *blob)
{
  DP_SET_OPEN (ACC_TRACE_STATE_MAGIC, ACC_TRACE_STATE_VERSION,
               dp_acc_trace_state_bytes (s));
  s->count = dp_r_u64 (&_r);
  dp_r_bytes (&_r, s->acc, s->n * sizeof (double));
  return DP_OK;
}

void
dp_acc_trace_accumulate (dp_acc_trace_state_t *state, const float *p,
                         size_t p_len)
{
  const size_t n = state->n;
  if (p_len < n)
    return; /* require a full frame */

  double *acc = state->acc;

  /* First frame seeds the trace directly for every mode. */
  if (state->count == 0)
    {
      for (size_t i = 0; i < n; i++)
        acc[i] = (double)p[i];
      state->count = 1;
      return;
    }

  state->count++;

  switch (state->mode)
    {
    case ACC_TRACE_MEAN:
      {
        /* Welford running mean: acc += (p - acc) / count. */
        const double inv = 1.0 / (double)state->count;
        for (size_t i = 0; i < n; i++)
          acc[i] += ((double)p[i] - acc[i]) * inv;
        break;
      }
    case ACC_TRACE_EXP:
      {
        const double a = state->alpha;
        for (size_t i = 0; i < n; i++)
          acc[i] = dp_ema_step (acc[i], (double)p[i], a);
        break;
      }
    case ACC_TRACE_MAXHOLD:
      for (size_t i = 0; i < n; i++)
        {
          const double v = (double)p[i];
          if (v > acc[i])
            acc[i] = v;
        }
      break;
    case ACC_TRACE_MINHOLD:
      for (size_t i = 0; i < n; i++)
        {
          const double v = (double)p[i];
          if (v < acc[i])
            acc[i] = v;
        }
      break;
    }
}

size_t
dp_acc_trace_value_max_out (dp_acc_trace_state_t *state)
{
  return state->n;
}

size_t
dp_acc_trace_value (dp_acc_trace_state_t *state, size_t n, float *out,
                    size_t max_out)
{
  (void)n; /* the trace length is state->n; n is vestigial */
  if (state->count == 0)
    return 0;
  /* Emission stops at the caller's capacity (jm gh-138). */
  size_t n_out = state->n < max_out ? state->n : max_out;
  for (size_t i = 0; i < n_out; i++)
    out[i] = (float)state->acc[i];
  return n_out;
}
