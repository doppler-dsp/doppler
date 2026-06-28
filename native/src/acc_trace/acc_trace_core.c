/**
 * @file acc_trace_core.c
 * @brief AccTrace — per-bin vector trace accumulator (mean/EMA/max/min hold).
 *
 * The running trace is held in double precision so that the linear mean stays
 * accurate over thousands of frames; input and output are float32.  The first
 * frame seeds the trace in every mode, which makes max/min-hold start from a
 * real sample (not +/-inf sentinels) and the EMA start unbiased.
 */
#include "acc_trace/acc_trace_core.h"

acc_trace_state_t *
acc_trace_create (size_t n, int mode, double alpha)
{
  if (n == 0 || mode < ACC_TRACE_MEAN || mode > ACC_TRACE_MINHOLD)
    return NULL;

  acc_trace_state_t *s = (acc_trace_state_t *)calloc (1, sizeof (*s));
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

void
acc_trace_destroy (acc_trace_state_t *state)
{
  if (!state)
    return;
  free (state->acc);
  free (state);
}

void
acc_trace_reset (acc_trace_state_t *state)
{
  memset (state->acc, 0, state->n * sizeof (double));
  state->count = 0;
}

/* Serializable state — running trace + fold count; config restored by
 * create(). */
size_t
acc_trace_state_bytes (const acc_trace_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (uint64_t) + s->n * sizeof (double);
}

void
acc_trace_get_state (const acc_trace_state_t *s, void *blob)
{
  DP_GET_OPEN (ACC_TRACE_STATE_MAGIC, ACC_TRACE_STATE_VERSION,
               acc_trace_state_bytes (s));
  dp_w_u64 (&_w, s->count);
  dp_w_bytes (&_w, s->acc, s->n * sizeof (double));
}

int
acc_trace_set_state (acc_trace_state_t *s, const void *blob)
{
  DP_SET_OPEN (ACC_TRACE_STATE_MAGIC, ACC_TRACE_STATE_VERSION,
               acc_trace_state_bytes (s));
  s->count = dp_r_u64 (&_r);
  dp_r_bytes (&_r, s->acc, s->n * sizeof (double));
  return DP_OK;
}

void
acc_trace_accumulate (acc_trace_state_t *state, const float *p, size_t p_len)
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
          acc[i] = a * (double)p[i] + (1.0 - a) * acc[i];
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
acc_trace_value_max_out (acc_trace_state_t *state)
{
  return state->n;
}

size_t
acc_trace_value (acc_trace_state_t *state, size_t n, float *out)
{
  (void)n; /* buffer is pre-sized to state->n by the binding */
  if (state->count == 0)
    return 0;
  for (size_t i = 0; i < state->n; i++)
    out[i] = (float)state->acc[i];
  return state->n;
}
