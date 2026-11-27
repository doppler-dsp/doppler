/**
 * @file cic_core.c
 * @brief CIC decimation filter — CIC_N stages, one-sample combs, integer
 * pipeline.
 *
 * Input amplitude is bounded: |Re| and |Im| under 2^headroom_bits
 * (CIC_PAPR_WIDE as built), clipped at and beyond that (see cic_core.h — that
 * constraint is the caller-facing contract; what follows is how it arises).
 *
 * Input samples (CF32) are scaled to a signed integer word, saturated, and
 * held modulo 2^CIC_REG_BITS in a register: the word is what the precision
 * names, or the CIC_REG_BITS - CIC_N * log2(R) bits the pipeline's gain
 * leaves when that is less. The
 * offset-binary midpoint is added after the combs, at the pipeline's gain,
 * so the normalising shift is of a non-negative value and no register is
 * signed.
 *
 * The unsigned modular-arithmetic CIC property holds unconditionally for
 * uint64_t: every overflow in the integrators cancels exactly in the comb
 * subtractions — so the integrator/comb pipeline needs no saturation logic,
 * no range checks and no floating-point.  Saturation happens once, at the
 * encoder above, and is the bound the caller sees.
 *
 * The hot path loops over CIC_N, a constant, so the compiler unrolls it.
 */

#include "doppler/cic/cic_core.h"

/* The library's out-of-line copy of dp_cic_decimate(). The body lives ONCE,
   as the JM_FORCEINLINE definition in cic_core.h; JM_FORCEINLINE is a plain
   `inline`, so this `extern` declaration asks this translation unit, and
   only this one, to emit that same definition with external linkage (the
   idiom of util's saturate.c). A C caller still inlines the header's copy;
   a C++ caller, which the header gives only the prototype, links this
   (doppler#3108). */
extern size_t dp_cic_decimate (dp_cic_state_t *state, const float _Complex *in,
                               size_t n_in, float _Complex *out,
                               size_t max_out);

#include <math.h>

#include <string.h>

/* ── internal helpers ──────────────────────────────────────────────────── */

/* log2 of a known power-of-two value via bit scan. */
static uint32_t
log2_pow2 (uint32_t x)
{
  uint32_t k = 0;
  while (x > 1)
    {
      k++;
      x >>= 1;
    }
  return k;
}

static int
valid_R (uint32_t R)
{
  /* Power of two from 2 to CIC_R_MAX, whose gain still leaves the register
     a word wider than CIC_PRECISION_16. See CIC_R_MAX. */
  return R >= 2 && R <= CIC_R_MAX && (R & (R - 1)) == 0;
}

int
dp_cic_precision_ok (uint32_t precision)
{
  return precision == CIC_PRECISION_16 || precision == CIC_PRECISION_32
         || precision == CIC_PRECISION_64;
}

int
dp_cic_headroom_bits_ok (uint32_t bits)
{
  return bits <= CIC_HEADROOM_BITS_MAX;
}

/* The headroom a precision starts with. */
static uint32_t
precision_headroom (uint32_t precision)
{
  return precision == CIC_PRECISION_16 ? CIC_HEADROOM_BITS_16
                                       : CIC_HEADROOM_BITS_WIDE;
}

/* Everything R, the precision and the headroom decide, from the three. */
static void
derive (dp_cic_state_t *s)
{
  s->shift = CIC_N * log2_pow2 (s->R);
  /* the word: what the precision names, or all the register has left */
  const uint32_t room = CIC_REG_BITS - s->shift;
  s->bits             = s->precision < room ? s->precision : room;
  s->offset           = (uint64_t)1 << (s->bits - 1u);
  /* unit amplitude as an integer, a power of two, so the scaling is exact */
  const int unit = (int)(s->bits - 1u - s->headroom_bits);
  s->in_scale    = ldexpf (1.0f, unit);
  s->out_scale   = ldexpf (1.0f, -unit);
  /* The word holds [-offset, offset - 1]. The top as a float is the
     largest float at or under offset - 1: that value itself while a float
     can hold it, and the float under offset once it rounds up to it. */
  s->in_lo = -(float)s->offset;
  s->in_hi = (float)(s->offset - 1u);
  if (s->in_hi >= (float)s->offset)
    s->in_hi = nextafterf ((float)s->offset, 0.0f);
}

/* ── lifecycle ─────────────────────────────────────────────────────────── */

dp_cic_state_t *
dp_cic_create (uint32_t R)
{
  if (!valid_R (R))
    return NULL;

  dp_cic_state_t *s = (dp_cic_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->R             = R;
  s->precision     = CIC_PRECISION_DEFAULT;
  s->headroom_bits = precision_headroom (s->precision);
  derive (s);
  return s;
}

void
dp_cic_destroy (dp_cic_state_t *state)
{
  free (state);
}

void
dp_cic_reset (dp_cic_state_t *state)
{
  memset (state->integ_re, 0, sizeof (state->integ_re));
  memset (state->integ_im, 0, sizeof (state->integ_im));
  memset (state->comb_re, 0, sizeof (state->comb_re));
  memset (state->comb_im, 0, sizeof (state->comb_im));
  state->phase   = 0;
  state->clipped = 0;
}

/* ── Serializable state — standard envelope (see dp_state.h) ─────────────────
 * Order: R, precision, headroom_bits, integ_re, integ_im, comb_re, comb_im
 * (each CIC_N u64), phase, clipped. R, the precision and the headroom travel
 * because reconfigure(), set_precision() and set_headroom_bits() change them
 * (doppler#2642); the rest of the config is derived from the three. */

size_t
dp_cic_state_bytes (const dp_cic_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + 3 * sizeof (uint32_t)
         + 4 * CIC_N * sizeof (uint64_t) + sizeof (uint32_t)
         + sizeof (uint8_t);
}

void
dp_cic_get_state (const dp_cic_state_t *state, void *blob)
{
  const size_t ab = CIC_N * sizeof (uint64_t);
  dp_writer_t  w  = dp_writer_init (blob, dp_cic_state_bytes (state));
  dp_w_hdr (&w, CIC_STATE_MAGIC, CIC_STATE_VERSION,
            dp_cic_state_bytes (state));
  dp_w_u32 (&w, state->R);
  dp_w_u32 (&w, state->precision);
  dp_w_u32 (&w, state->headroom_bits);
  dp_w_bytes (&w, state->integ_re, ab);
  dp_w_bytes (&w, state->integ_im, ab);
  dp_w_bytes (&w, state->comb_re, ab);
  dp_w_bytes (&w, state->comb_im, ab);
  dp_w_u32 (&w, state->phase);
  dp_w_bytes (&w, &state->clipped, sizeof state->clipped);
}

int
dp_cic_set_state (dp_cic_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, dp_cic_state_bytes (state),
                              CIC_STATE_MAGIC, CIC_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  const size_t ab = CIC_N * sizeof (uint64_t);
  dp_reader_t  r  = dp_reader_init (blob, dp_cic_state_bytes (state));
  r.off           = sizeof (dp_state_hdr_t);
  /* Decode whole into a temporary, check it, then commit: an R create()
     refuses, or a phase a live counter at that R never holds, is refused
     before anything is written. */
  dp_cic_state_t t = *state;
  t.R              = dp_r_u32 (&r);
  t.precision      = dp_r_u32 (&r);
  t.headroom_bits  = dp_r_u32 (&r);
  dp_r_bytes (&r, t.integ_re, ab);
  dp_r_bytes (&r, t.integ_im, ab);
  dp_r_bytes (&r, t.comb_re, ab);
  dp_r_bytes (&r, t.comb_im, ab);
  t.phase = dp_r_u32 (&r);
  dp_r_bytes (&r, &t.clipped, sizeof t.clipped);
  if (!valid_R (t.R) || t.phase >= t.R || !dp_cic_precision_ok (t.precision)
      || !dp_cic_headroom_bits_ok (t.headroom_bits))
    return DP_ERR_INVALID;
  derive (&t);
  *state = t;
  return DP_OK;
}

/* ── decimate ──────────────────────────────────────────────────────────── */

double
dp_cic_dc_gain (const dp_cic_state_t *state)
{
  return pow ((double)state->R, (double)CIC_N)
         / ldexp (1.0, (int)state->shift);
}

double
dp_cic_get_group_delay (const dp_cic_state_t *state)
{
  /* CIC_N boxcars of R: the response's center is CIC_N * (R - 1) / 2 behind
     the newest input, and output k is formed R - 1 inputs after k * R. */
  return (double)(state->R - 1u) * (double)(CIC_N - 2) / 2.0;
}

size_t
dp_cic_decimate_max_out (dp_cic_state_t *state)
{
  (void)state;
  return 0; /* one output per decim >= 1 inputs, so outputs <= inputs */
}

/* dp_cic_decimate is a static inline defined in cic_core.h. */

/* ── reconfigure ───────────────────────────────────────────────────────── */

void
dp_cic_reconfigure (dp_cic_state_t *state, uint32_t R)
{
  if (!valid_R (R))
    return;
  state->R = R;
  derive (state);
  dp_cic_reset (state);
}

int
dp_cic_set_precision (dp_cic_state_t *state, uint32_t precision)
{
  if (!dp_cic_precision_ok (precision))
    return DP_ERR_INVALID;
  state->precision     = precision;
  state->headroom_bits = precision_headroom (precision);
  derive (state);
  dp_cic_reset (state);
  return DP_OK;
}

uint32_t
dp_cic_get_precision (const dp_cic_state_t *state)
{
  return state->precision;
}

uint32_t
dp_cic_get_bits (const dp_cic_state_t *state)
{
  return state->bits;
}

int
dp_cic_set_headroom_bits (dp_cic_state_t *state, uint32_t bits)
{
  if (!dp_cic_headroom_bits_ok (bits))
    return DP_ERR_INVALID;
  state->headroom_bits = bits;
  derive (state);
  dp_cic_reset (state);
  return DP_OK;
}

uint32_t
dp_cic_get_headroom_bits (const dp_cic_state_t *state)
{
  return state->headroom_bits;
}

double
dp_cic_get_full_scale (const dp_cic_state_t *state)
{
  return ldexp (1.0, (int)state->headroom_bits);
}

double
dp_cic_get_step (const dp_cic_state_t *state)
{
  return ldexp (1.0, (int)state->headroom_bits - (int)(state->bits - 1u));
}
