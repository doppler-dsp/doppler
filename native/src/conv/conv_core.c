/*
 * conv_core.c — convolutional codes: the description, the encoder, and the
 * Viterbi decoder that reads the same description.
 *
 * Both directions go through conv_outputs(), which is the only place in the
 * tree that says what this family of codes emits. See conv_core.h.
 */
#include "conv/conv_core.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

/* Parity of the tapped stages: the modulo-2 sum an adder in the encoder's
   figure computes. Folds 32 bits, so it covers any k up to CONV_K_MAX. */
static inline unsigned
parity32 (uint32_t v)
{
  v ^= v >> 16;
  v ^= v >> 8;
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return (unsigned)(v & 1u);
}

int
conv_code_valid (const conv_code_t *c)
{
  if (c == NULL || c->k < 2u || c->k > CONV_K_MAX || c->n < 1u
      || c->n > CONV_N_MAX)
    return 0;
  for (unsigned j = 0; j < c->n; j++)
    {
      /* A zero polynomial is an output carrying no information, and a
         polynomial wider than the register is a transcription that lost its
         alignment. Both are typos rather than codes. */
      if (c->poly[j] == 0u || c->poly[j] >> c->k)
        return 0;
    }
  return 1;
}

unsigned
conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit)
{
  const uint32_t reg = ((bit & 1u) << (c->k - 1u)) | state;
  unsigned       w   = 0;
  for (unsigned j = 0; j < c->n; j++)
    w |= parity32 (reg & c->poly[j]) << j;
  /* The inversion is a property of the CODE, applied here so that neither
     the encoder nor the decoder can hold a private opinion about it. */
  return w ^ (unsigned)(c->invert & ((1u << c->n) - 1u));
}

void
conv_enc_init (conv_enc_t *s)
{
  s->reg = 0u;
}

size_t
conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
             size_t n_in, uint8_t *out, size_t max_out)
{
  if (!conv_code_valid (c) || max_out < n_in * (size_t)c->n)
    return 0;

  const uint32_t mask = conv_states (c) - 1u;
  for (size_t i = 0; i < n_in; i++)
    {
      const unsigned b = in[i] & 1u;
      const unsigned w = conv_outputs (c, s->reg & mask, b);
      for (unsigned j = 0; j < c->n; j++)
        out[i * c->n + j] = (uint8_t)((w >> j) & 1u);
      s->reg = conv_next_state (c, s->reg & mask, b);
    }
  return n_in * (size_t)c->n;
}

/* ── the decoder ─────────────────────────────────────────────────────────
 *
 * Traceback rather than register exchange: at depth 60 and 64 states the two
 * cost about the same per bit, and traceback stores one bit per state per
 * step where register exchange stores a whole path per state.
 */
struct viterbi_state_t
{
  conv_code_t code;
  size_t      depth;
  uint32_t    nstate;

  float   *pm;   /**< path metric per state                              */
  float   *pm2;  /**< the next step's, swapped rather than copied        */
  uint8_t *dec;  /**< depth x nstate decisions: which predecessor won   */
  size_t   head; /**< ring cursor, in steps                            */
  size_t   fill; /**< steps recorded, saturating at depth              */

  /* Derived once: for each state, its two predecessors and their output
     words. The butterfly -- predecessors (ns << 1) & mask and | 1, both on
     the same input bit ns >> (k-2) -- holds for every k. */
  uint32_t *pred0;
  uint32_t *pred1;
  unsigned *out0;
  unsigned *out1;
  unsigned *inbit;
};

viterbi_state_t *
viterbi_create (const conv_code_t *c, size_t depth)
{
  if (!conv_code_valid (c) || depth == 0u)
    return NULL;

  viterbi_state_t *s = (viterbi_state_t *)calloc (1, sizeof *s);
  if (s == NULL)
    return NULL;

  s->code   = *c;
  s->depth  = depth;
  s->nstate = conv_states (c);

  s->pm    = (float *)malloc (s->nstate * sizeof *s->pm);
  s->pm2   = (float *)malloc (s->nstate * sizeof *s->pm2);
  s->dec   = (uint8_t *)malloc (depth * s->nstate);
  s->pred0 = (uint32_t *)malloc (s->nstate * sizeof *s->pred0);
  s->pred1 = (uint32_t *)malloc (s->nstate * sizeof *s->pred1);
  s->out0  = (unsigned *)malloc (s->nstate * sizeof *s->out0);
  s->out1  = (unsigned *)malloc (s->nstate * sizeof *s->out1);
  s->inbit = (unsigned *)malloc (s->nstate * sizeof *s->inbit);
  if (!s->pm || !s->pm2 || !s->dec || !s->pred0 || !s->pred1 || !s->out0
      || !s->out1 || !s->inbit)
    {
      viterbi_destroy (s);
      return NULL;
    }

  const uint32_t mask = s->nstate - 1u;
  for (uint32_t ns = 0; ns < s->nstate; ns++)
    {
      /* The bit that reaches ns is the one that was shifted in, which after
         the shift sits in ns's top position. */
      const unsigned b  = (unsigned)(ns >> (c->k - 2u)) & 1u;
      const uint32_t p0 = (ns << 1) & mask;
      const uint32_t p1 = p0 | 1u;
      s->inbit[ns]      = b;
      s->pred0[ns]      = p0;
      s->pred1[ns]      = p1;
      s->out0[ns]       = conv_outputs (c, p0, b);
      s->out1[ns]       = conv_outputs (c, p1, b);
    }

  viterbi_reset (s);
  return s;
}

void
viterbi_destroy (viterbi_state_t *s)
{
  if (s == NULL)
    return;
  free (s->pm);
  free (s->pm2);
  free (s->dec);
  free (s->pred0);
  free (s->pred1);
  free (s->out0);
  free (s->out1);
  free (s->inbit);
  free (s);
}

void
viterbi_reset (viterbi_state_t *s)
{
  /* The encoder starts from a reset register, so the all-zero state is the
     only one with any prior probability. -FLT_MAX/4 rather than -inf keeps
     every later arithmetic operation finite. */
  for (uint32_t i = 0; i < s->nstate; i++)
    s->pm[i] = -FLT_MAX / 4.0f;
  s->pm[0] = 0.0f;
  s->head  = 0;
  s->fill  = 0;
  memset (s->dec, 0, s->depth * s->nstate);
}

const conv_code_t *
viterbi_code (const viterbi_state_t *s)
{
  return &s->code;
}

size_t
viterbi_depth (const viterbi_state_t *s)
{
  return s->depth;
}

size_t
viterbi_decode_max_out (const viterbi_state_t *s, size_t n_llr)
{
  const size_t steps = n_llr / s->code.n;
  const size_t owed
      = s->depth - 1u - (s->fill < s->depth - 1u ? s->fill : s->depth - 1u);
  return steps > owed ? steps - owed : 0u;
}

/* Walk `depth-1` steps back from `st` through the ring and return the input
   bit on the branch taken there. */
static unsigned
traceback (const viterbi_state_t *s, uint32_t st)
{
  size_t idx = s->head;
  for (size_t i = 0; i + 1u < s->depth; i++)
    {
      idx                = (idx == 0u) ? s->depth - 1u : idx - 1u;
      const uint8_t took = s->dec[idx * s->nstate + st];
      st                 = took ? s->pred1[st] : s->pred0[st];
    }
  /* idx now indexes the oldest step in the window; the bit decided there is
     the one whose branch entered `st`. */
  return s->inbit[st];
}

size_t
viterbi_decode (viterbi_state_t *s, const float *llr, size_t n_llr,
                uint8_t *out, size_t max_out)
{
  const conv_code_t *c = &s->code;
  if (n_llr % c->n != 0u)
    return 0;
  if (max_out < viterbi_decode_max_out (s, n_llr))
    return 0;

  const size_t   steps = n_llr / c->n;
  const uint32_t S     = s->nstate;
  const unsigned npat  = 1u << c->n;
  float          bm[1u << CONV_N_MAX];
  size_t         nout = 0;

  for (size_t t = 0; t < steps; t++)
    {
      const float *l = llr + t * c->n;

      /* Branch metrics once per step, not per state: rate 1/n has only 2^n
         distinct output words, so 128 branches at k=7 index a table of 4. */
      for (unsigned p = 0; p < npat; p++)
        {
          float m = 0.0f;
          for (unsigned j = 0; j < c->n; j++)
            m += ((p >> j) & 1u) ? -l[j] : l[j];
          bm[p] = m;
        }

      uint8_t *row  = s->dec + s->head * S;
      float    best = -FLT_MAX;
      uint32_t barg = 0;

      for (uint32_t ns = 0; ns < S; ns++)
        {
          const float m0 = s->pm[s->pred0[ns]] + bm[s->out0[ns]];
          const float m1 = s->pm[s->pred1[ns]] + bm[s->out1[ns]];
          const int   t1 = m1 > m0;
          const float m  = t1 ? m1 : m0;
          row[ns]        = (uint8_t)t1;
          s->pm2[ns]     = m;
          if (m > best)
            {
              best = m;
              barg = ns;
            }
        }

      /* Renormalise: path metrics grow without bound on a stream, and a
         common offset cannot reorder survivors. */
      for (uint32_t ns = 0; ns < S; ns++)
        s->pm2[ns] -= best;

      float *tmp = s->pm;
      s->pm      = s->pm2;
      s->pm2     = tmp;

      s->head = (s->head + 1u == s->depth) ? 0u : s->head + 1u;
      if (s->fill < s->depth)
        s->fill++;

      if (s->fill == s->depth)
        out[nout++] = (uint8_t)traceback (s, barg);
    }

  return nout;
}

/* ── the state bytes interface ───────────────────────────────────────────
 *
 * Running state is the path metrics, the traceback ring, and the cursor into
 * it. `pm2` is scratch -- it is overwritten before it is read on every step,
 * so carrying it would serialize noise. The predecessor and output tables are
 * DERIVED from the code, which viterbi_create rebuilds identically.
 *
 * `fill` is part of the answer rather than bookkeeping: a decoder resumed
 * with `fill < depth` still owes its traceback, and viterbi_decode_max_out
 * reads it. Dropping it would resume a decoder that emits bits it has not
 * earned.
 */
typedef struct
{
  uint64_t depth;
  uint64_t head;
  uint64_t fill;
  uint32_t k;
  uint32_t n;
  uint32_t invert;
  uint32_t poly[CONV_N_MAX];
} viterbi_extra_t;

size_t
viterbi_state_bytes (const viterbi_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (viterbi_extra_t)
         + (size_t)s->nstate * sizeof (float) /* pm            */
         + s->depth * (size_t)s->nstate;      /* traceback ring */
}

void
viterbi_get_state (const viterbi_state_t *s, void *blob)
{
  DP_GET_OPEN (VITERBI_STATE_MAGIC, VITERBI_STATE_VERSION,
               viterbi_state_bytes (s));

  /* memset rather than a designated initializer: the padding a designated
     initializer leaves unspecified (C11 6.7.9p10) is written to the blob
     whole, which reads green on one compiler and red on another. Zeroing
     first defines every byte regardless of what the layout turns out to be.
     The same reason zeroes poly[n..CONV_N_MAX-1], which conv_code_t does not
     require a caller to fill -- serializing them would make the blob depend
     on the caller's stack. */
  viterbi_extra_t extra;
  memset (&extra, 0, sizeof extra);
  extra.depth  = (uint64_t)s->depth;
  extra.head   = (uint64_t)s->head;
  extra.fill   = (uint64_t)s->fill;
  extra.k      = s->code.k;
  extra.n      = s->code.n;
  extra.invert = s->code.invert & ((1u << s->code.n) - 1u);
  for (unsigned j = 0; j < s->code.n; j++)
    extra.poly[j] = s->code.poly[j];

  dp_w_bytes (&_w, &extra, sizeof extra);
  dp_w_f32 (&_w, s->pm, s->nstate);
  dp_w_bytes (&_w, s->dec, s->depth * (size_t)s->nstate);
}

int
viterbi_set_state (viterbi_state_t *s, const void *blob)
{
  DP_SET_OPEN (VITERBI_STATE_MAGIC, VITERBI_STATE_VERSION,
               viterbi_state_bytes (s));

  viterbi_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);

  /* The envelope's size check does not imply a configuration match: two
     codes of the same k and n differ in no dimension the length can see, so
     the code is compared field by field. */
  if (extra.depth != (uint64_t)s->depth || extra.k != s->code.k
      || extra.n != s->code.n
      || extra.invert != (s->code.invert & ((1u << s->code.n) - 1u)))
    return DP_ERR_INVALID;
  for (unsigned j = 0; j < s->code.n; j++)
    if (extra.poly[j] != s->code.poly[j])
      return DP_ERR_INVALID;

  /* The cursor indexes the ring and `fill` gates emission; a blob claiming
     either out of range would be traced back through memory it does not
     own. */
  if (extra.head >= (uint64_t)s->depth || extra.fill > (uint64_t)s->depth)
    return DP_ERR_INVALID;

  dp_r_f32 (&_r, s->pm, s->nstate);
  dp_r_bytes (&_r, s->dec, s->depth * (size_t)s->nstate);
  s->head = (size_t)extra.head;
  s->fill = (size_t)extra.fill;
  return DP_OK;
}
