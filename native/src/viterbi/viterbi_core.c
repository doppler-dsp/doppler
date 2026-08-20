/*
 * viterbi_core.c — the soft-decision Viterbi decoder.
 *
 * Split out of conv_core.c and declared to just-makeit (doppler#893). `conv`
 * owns the CODE — polynomials, encoder, trellis arithmetic — and this owns
 * the DECODER built over one, so the object gets its Python face, its test
 * target and its benchmark target generated rather than hand-registered.
 */
#include "viterbi/viterbi_core.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

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

/* The declared constructor. A `conv_code_t *` is not expressible in a
   manifest, so the object takes the polynomials directly -- the array IS the
   code, and its length gives n. Callers holding a conv_code_t already (the
   CCSDS configuration, the validators) use viterbi_create_code below. */
viterbi_state_t *
viterbi_create (const uint32_t *poly, size_t poly_len, uint32_t k,
                uint32_t invert, size_t depth)
{
  conv_code_t c = { 0 };
  if (!poly || poly_len == 0 || poly_len > CONV_N_MAX)
    return NULL;
  c.k      = k;
  c.n      = (unsigned)poly_len;
  c.invert = invert;
  for (size_t i = 0; i < poly_len; i++)
    c.poly[i] = poly[i];
  return viterbi_create_code (&c, depth);
}

viterbi_state_t *
viterbi_create_code (const conv_code_t *c, size_t depth)
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

/* ── node synchronization ───────────────────────────────────────────────────
 *
 * The re-encoding metric: decode, re-encode the decisions, and count where
 * the result disagrees with what arrived. In sync the disagreements ARE the
 * channel's symbol errors; out of sync the decoder's output is unrelated to
 * its input and the count runs at about half the symbols. See conv_core.h and
 * docs/design/viterbi.md section 9.
 *
 * Chunked so that nothing is allocated and the window may be any length. The
 * decoder and the encoder both carry state across calls, so a chunk boundary
 * is not a discontinuity -- which is the same property the streaming decode
 * relies on and is worth stating, because a re-encode that restarted its
 * register per chunk would inject k-1 wrong symbols at every boundary and
 * read as a worse alignment.
 */
#define NODE_SYNC_CHUNK 256u

size_t
node_sync_scored_symbols (const viterbi_state_t *v, size_t n_llr)
{
  const conv_code_t *c    = viterbi_code (v);
  const size_t       nb   = viterbi_decode_max_out (v, (n_llr / c->n) * c->n);
  const size_t       warm = viterbi_depth (v);
  return nb > warm ? (nb - warm) * c->n : 0;
}

size_t
node_sync_score (viterbi_state_t *v, const float *llr, size_t n_llr)
{
  if (v == NULL || llr == NULL)
    return 0;

  const conv_code_t *c      = viterbi_code (v);
  const size_t       n      = c->n;
  const size_t       usable = (n_llr / n) * n;

  uint8_t    bits[NODE_SYNC_CHUNK];
  uint8_t    sym[NODE_SYNC_CHUNK * CONV_N_MAX];
  conv_enc_t enc;
  size_t     errors = 0, consumed = 0, decoded = 0;

  /* The warm-up, and it is the DECODER's number rather than the encoder's.
     Two cold starts overlap at the head of a window: the comparison encoder
     begins at a zero register while the transmitter's was mid-stream (k-1
     bits), and the decoder begins from its own all-zero prior, which is
     simply wrong when the window opens mid-capture -- measured, that second
     one is the larger of the two and k-1 does not cover it. The traceback
     depth is the decoder's own answer to how long its survivors take to be
     determined by the data rather than by where it started, so it is the
     honest quantity to skip. */
  const size_t warm = viterbi_depth (v);

  viterbi_reset (v);
  conv_enc_init (&enc);

  while (consumed < usable)
    {
      size_t take = usable - consumed;
      if (take > (size_t)NODE_SYNC_CHUNK * n)
        take = (size_t)NODE_SYNC_CHUNK * n;

      const size_t nb
          = viterbi_decode (v, llr + consumed, take, bits, NODE_SYNC_CHUNK);
      consumed += take;
      if (nb == 0)
        continue;

      conv_encode (&enc, c, bits, nb, sym, nb * n);

      for (size_t i = 0; i < nb; i++)
        {
          /* The decoder's output is aligned with its input and merely stops
             short, so decoded bit `decoded + i` came from the branch at
             symbol `(decoded + i) * n` of the window. */
          const size_t base = (decoded + i) * n;

          if (decoded + i < warm)
            continue;

          for (size_t j = 0; j < n; j++)
            {
              const unsigned got = llr[base + j] < 0.0f ? 1u : 0u;
              errors += (sym[i * n + j] != got);
            }
        }
      decoded += nb;
    }

  return errors;
}

int
node_sync_scan (viterbi_state_t *v, const float *llr, size_t n_llr,
                node_sync_t *out)
{
  if (v == NULL || llr == NULL)
    return 0;

  const conv_code_t *c = viterbi_code (v);
  const size_t       n = c->n;
  if (n_llr < n)
    return 0;

  /* Every hypothesis is scored over the SAME number of symbols, or the
     comparison would be between counts of different lengths -- which reads
     as a margin and is arithmetic. */
  const size_t span = ((n_llr - (n - 1u)) / n) * n;
  if (span == 0)
    return 0;

  size_t   best = SIZE_MAX, second = SIZE_MAX;
  unsigned best_p = 0;

  for (unsigned p = 0; p < (unsigned)n; p++)
    {
      const size_t e = node_sync_score (v, llr + p, span);
      if (e < best)
        {
          second = best;
          best   = e;
          best_p = p;
        }
      else if (e < second)
        second = e;
    }

  node_sync_t r = { best_p, best, second == SIZE_MAX ? best : second,
                    node_sync_scored_symbols (v, span), 0u };
  /* A window too short to emit a decision past the traceback scores zero
     everywhere, which is not a decision -- it reports as a margin of 0
     rather than as a confident phase 0. */
  r.margin = r.next > r.errors ? r.next - r.errors : 0u;

  if (out != NULL)
    *out = r;
  return 1;
}
