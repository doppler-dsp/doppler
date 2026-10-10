/**
 * @file acc_trace_core.c
 * @brief AccTrace — per-bin vector trace accumulator (mean/EMA/max/min hold).
 *
 * The running trace is held in double precision; input and output are
 * float32.  In mean mode it is a per-bin SUM, divided by the count only when
 * it is read: no per-frame divide, and over 10^7 frames of exponential power
 * a relative error of 2.4e-14 where the Welford update it replaced reached
 * 3.0e-13 (docs/design/spectrogram-measurements.md, 'The trace's mean is a
 * sum').  The first frame seeds the trace in every mode, which makes
 * max/min-hold start from a real sample (not +/-inf sentinels) and the EMA
 * start unbiased.
 *
 * Every fold is a plain loop with an unconditional store, so the compiler
 * vectorizes it in the builds that ship (GCC x86-64-v2, clang aarch64): the
 * hold modes are a select, not the branch around a store they were, which
 * only an AVX-512 build could vectorize (with masked stores), so the
 * portable build ran them scalar, 7-10x slower than the mean.
 */
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/util/util_core.h"
#include <stdint.h>

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
  /* n doubles must have a byte count a size_t holds: refused here, not
     left to calloc's own overflow check, which ASan and TSan report as an
     error rather than a NULL. */
  if (n == 0 || n > SIZE_MAX / sizeof (double) || mode < ACC_TRACE_MEAN
      || mode > ACC_TRACE_MINHOLD)
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

/* Serializable state — mode, fold count, alpha, running trace (in mean mode,
 * the per-bin sum: version 3, so a version-2 blob, which held the mean, is
 * refused rather than read as a sum).  n is config,
 * restored by create() and checked by the blob's size.  mode is config too,
 * and travels only as a REJECT key, the way DDC packs its rate: a mean
 * trace's blob restored into an exp instance would otherwise come back OK and
 * go on as an EMA.  alpha travels with the trace because a setter's value is
 * STATE: anything a setter can change after create() is packed, and restored
 * only past that setter's own predicate (#2022).  A resume that took alpha
 * from create() went on averaging with the old one (#2000). */
size_t
dp_acc_trace_state_bytes (const dp_acc_trace_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t) + sizeof (uint64_t)
         + sizeof (double) + s->n * sizeof (double);
}

void
dp_acc_trace_get_state (const dp_acc_trace_state_t *s, void *blob)
{
  DP_GET_OPEN (ACC_TRACE_STATE_MAGIC, ACC_TRACE_STATE_VERSION,
               dp_acc_trace_state_bytes (s));
  dp_w_u32 (&_w, (uint32_t)s->mode);
  dp_w_u64 (&_w, s->count);
  dp_w_f64 (&_w, s->alpha);
  dp_w_bytes (&_w, s->acc, s->n * sizeof (double));
}

int
dp_acc_trace_set_state (dp_acc_trace_state_t *s, const void *blob)
{
  DP_SET_OPEN (ACC_TRACE_STATE_MAGIC, ACC_TRACE_STATE_VERSION,
               dp_acc_trace_state_bytes (s));
  const uint32_t mode  = dp_r_u32 (&_r);
  const uint64_t count = dp_r_u64 (&_r);
  const double   alpha = dp_r_f64 (&_r);
  /* Both checks before anything is written, so a refused blob leaves the
   * state as it was: a blob from another mode is another configuration, and
   * a blob cannot install an alpha that dp_acc_trace_set_alpha would refuse.
   */
  if (mode != (uint32_t)s->mode)
    return DP_ERR_INVALID;
  if (!acc_trace_alpha_ok ((int)s->mode, alpha))
    return DP_ERR_INVALID;
  s->count = count;
  s->alpha = alpha;
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
      /* The per-bin sum; dp_acc_trace_value divides by count. */
      for (size_t i = 0; i < n; i++)
        acc[i] += (double)p[i];
      break;
    case ACC_TRACE_EXP:
      {
        const double a = state->alpha;
        for (size_t i = 0; i < n; i++)
          acc[i] = dp_ema_step (acc[i], (double)p[i], a);
        break;
      }
    /* A select and an unconditional store, so both vectorize (a packed
       max/min). A NaN in the frame never replaces the trace: the compare is
       false, so the bin keeps what it held. That rests on the library's
       declared -fno-finite-math-only beside its -ffast-math
       (CMakeLists.txt:146), which test_fp_policy.c holds. */
    case ACC_TRACE_MAXHOLD:
      for (size_t i = 0; i < n; i++)
        {
          const double v = (double)p[i];
          acc[i]         = v > acc[i] ? v : acc[i];
        }
      break;
    case ACC_TRACE_MINHOLD:
      for (size_t i = 0; i < n; i++)
        {
          const double v = (double)p[i];
          acc[i]         = v < acc[i] ? v : acc[i];
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
  if (state->mode == ACC_TRACE_MEAN)
    {
      /* the sum becomes the mean here, once per reading, in double */
      const double count = (double)state->count;
      for (size_t i = 0; i < n_out; i++)
        out[i] = (float)(state->acc[i] / count);
    }
  else
    for (size_t i = 0; i < n_out; i++)
      out[i] = (float)state->acc[i];
  return n_out;
}
