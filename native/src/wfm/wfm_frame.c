/*
 * wfm_frame.c — the frame descriptor's geometry and materialisation. The
 * contract, and the reasoning behind each boundary, live on the declarations
 * in wfm/wfm_frame.h.
 */
#include "doppler/wfm/wfm_frame.h"

#include "doppler/clib_common.h"  /* DP_OK / DP_ERR_*       */
#include "doppler/cvt/cvt_core.h" /* dp_hex_to_bin          */
#include "doppler/dp_crc16.h"
#include "doppler/dp_interleave.h"
#include "doppler/gold/gold_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/wfm/wfm_names.h" /* SEQ_KIND_NAMES, LFSR_NAMES */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

dp_pn_state_t *
dp_wfm_seq_pn_create (const wfm_seq_t *s)
{
  if (!s || s->kind != WFM_SEQ_PN)
    return NULL;
  /* poly 0 is "the maximal-length one for this register", the same
     resolution dp_wfm_synth_create() applies to its --pn-poly. Passing 0
     through to dp_pn_create() instead means a register with NO FEEDBACK:
     it shifts the seed out and emits zeros for ever, which is a constant
     field that still looks like a field. And a register with NO
     m-sequence to resolve to (width 1: pn_mls_poly is 0) is refused rather
     than built as that same no-feedback register -- doppler#1602, the
     frame-field twin of #1590's source fix. */
  const uint64_t poly = s->poly ? s->poly : pn_mls_poly (s->reg_bits);
  if (poly == 0)
    return NULL;
  return dp_pn_create (poly, s->seed ? s->seed : 1u, s->reg_bits, s->lfsr);
}

/* A field's bits, written at `out`. Returns the count, or 0 if the descriptor
   cannot produce them — which is a REFUSAL, not a short write: a frame that
   half-materialises would be scored against a truth nobody can regenerate. */
size_t
dp_wfm_seq_bits (const wfm_seq_t *s, uint8_t *out, size_t cap)
{
  if (!s || s->len == 0 || s->len > cap)
    return 0;
  switch (s->kind)
    {
    case WFM_SEQ_LITERAL:
      if (!s->bits)
        return 0;
      for (size_t i = 0; i < s->len; i++)
        out[i] = s->bits[i] & 1u;
      return s->len;

    case WFM_SEQ_DOTTED:
      /* 1010… — a line at Rs/2 for an AGC or a timing loop to settle on.
         Starts high so a one-bit field is not silently the same as zeros. */
      for (size_t i = 0; i < s->len; i++)
        out[i] = (uint8_t)((i & 1u) ^ 1u);
      return s->len;

    case WFM_SEQ_PN:
      {
        dp_pn_state_t *p = dp_wfm_seq_pn_create (s);
        if (!p)
          return 0;
        size_t n = dp_pn_generate (p, s->len, out, cap);
        dp_pn_destroy (p);
        return n;
      }

    case WFM_SEQ_GOLD:
      {
        dp_gold_state_t *g
            = dp_gold_create (s->taps_a, s->seed_a ? s->seed_a : 1u, s->taps_b,
                              s->seed_b ? s->seed_b : 1u, s->reg_bits);
        if (!g)
          return 0;
        size_t n = dp_gold_generate (g, s->len, out, cap);
        dp_gold_destroy (g);
        return n;
      }

    case WFM_SEQ_DATA:
      /* A payload drawn from the frame's data source (docs/design/
         payload-data-source.md): the description knows its length, never
         its bits, so there is nothing here to write. */
      return 0;
    }
  return 0;
}

/* ── the general description ──────────────────────────────────────────
 *
 * Everything below the divider is the arithmetic BOTH framers in the tree
 * were carrying separately. See docs/design/frame-description.md; the two
 * rules that are easy to lose are that a derived field is a field (which is
 * what removes any need for a stage to expand what it covers), and that a
 * stage's cover is declared rather than inherited from what ran before it.
 */

/* A caller-supplied field's length. A derived one is sized by its stage and
   is resolved separately, because that resolution depends on this. */
static size_t
supplied_bits (const wfm_field_t *f)
{
  if (f->derived_by || f->seq.len == 0)
    return 0;
  const size_t reps = f->reps ? f->reps : 1u;
  return f->seq.len * reps;
}

size_t
dp_wfm_field_render (const wfm_field_t *f, uint8_t *out, size_t max_out)
{
  if (!f || !out || f->derived_by)
    return 0;
  const size_t n = supplied_bits (f);
  if (n == 0 || n > max_out)
    return 0;
  /* One period, then repeated verbatim -- see the declaration for why a
     repetition may never draw fresh bits. */
  if (dp_wfm_seq_bits (&f->seq, out, f->seq.len) != f->seq.len)
    return 0;
  for (size_t r = 1; r * f->seq.len < n; r++)
    memcpy (out + r * f->seq.len, out, f->seq.len);
  return n;
}

/* ── the Field text form ──────────────────────────────────────────────
 *
 * docs/design/frame-description.md §F.1. The grammar is small on purpose,
 * and every rule below is a REFUSAL rather than a repair: the reader this
 * replaces skipped empty fields (`pn::10` read as `pn:10`) and let strtoull
 * stop wherever it liked (`12abc` read as 12, `010` as 8), so a typo became
 * a different, valid-looking field.
 */

/* A token: a run of the spec, not NUL-terminated. */
typedef struct
{
  const char *p;
  size_t      n;
} field_tok_t;

/* Tokens a generated field can have: gold's seven is the most. One more is
   allowed so that an eighth is SEEN and refused, rather than truncated. */
#define FIELD_MAX_TOKENS 8u

/* A macro's value as text, so a refusal names the number it enforces. */
#define FIELD_STR_(x) #x
#define FIELD_STR(x) FIELD_STR_ (x)

static const char FIELD_TOO_LONG[]
    = "LEN * REPS is past the Field bound "
      "of " FIELD_STR (WFM_FIELD_MAX_BITS) " bits";

static int
field_refuse (const char **why, const char *msg)
{
  if (why)
    *why = msg;
  return DP_ERR_INVALID;
}

static int
tok_is (field_tok_t t, const char *word)
{
  const size_t n = strlen (word);
  return t.n == n && memcmp (t.p, word, n) == 0;
}

/* A number consumed WHOLE: decimal, or hex after `0x`. A sign, a space, a
   trailing letter, an empty token and a bare `0x` are all refused, and a
   leading 0 is decimal -- there is no octal to mistype into. Public because
   wfmgen's numeric flags read by the same rule (doppler#1611). */
int
dp_wfm_parse_u64 (const char *p, size_t n, uint64_t *v)
{
  unsigned base = 10u;
  size_t   i    = 0;
  if (n >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
      base = 16u;
      i    = 2;
    }
  if (i == n)
    return -1;
  uint64_t acc = 0;
  for (; i < n; i++)
    {
      const int c = (unsigned char)p[i];
      unsigned  d;
      if (c >= '0' && c <= '9')
        d = (unsigned)(c - '0');
      else if (base == 16u && c >= 'a' && c <= 'f')
        d = (unsigned)(c - 'a' + 10);
      else if (base == 16u && c >= 'A' && c <= 'F')
        d = (unsigned)(c - 'A' + 10);
      else
        return -1;
      if (acc > (UINT64_MAX - d) / base)
        return -1; /* overflow is a refusal, not a wrap */
      acc = acc * base + d;
    }
  *v = acc;
  return 0;
}

static int
tok_u64 (field_tok_t t, uint64_t *v)
{
  return dp_wfm_parse_u64 (t.p, t.n, v);
}

/* Split [p, p + n) at every ':'. An EMPTY token is kept, so the caller sees
   it and refuses it. Returns the count, or FIELD_MAX_TOKENS + 1 when there
   are more than the table holds. */
static size_t
split_colons (const char *p, size_t n, field_tok_t *t)
{
  size_t      k     = 0;
  const char *start = p;
  for (size_t i = 0; i <= n; i++)
    if (i == n || p[i] == ':')
      {
        if (k == FIELD_MAX_TOKENS)
          return FIELD_MAX_TOKENS + 1u;
        t[k].p = start;
        t[k].n = (size_t)(p + i - start);
        k++;
        start = p + i + 1;
      }
  return k;
}

/* A literal: `0`/`1` digits, or `0x` hex through cvt's dp_hex_to_bin -- the
   one hex expansion, not a second one. */
static int
parse_literal (const char *p, size_t n, wfm_seq_t *q, uint8_t **owned,
               const char **why)
{
  const int    is_hex = n >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X');
  const size_t digits = is_hex ? n - 2u : n;
  if (digits == 0)
    return field_refuse (why, "a hex literal needs a digit after 0x");
  if (is_hex && digits > SIZE_MAX / 4u)
    return field_refuse (why, "a hex literal is too long");
  const size_t nbits = is_hex ? 4u * digits : digits;

  /* Both buffers are at most 4x the spec, which is already in memory, so
     only a genuine OOM fails them -- the abort-on-OOM helpers, not an unwind
     path no test can reach. */
  uint8_t *b = dp_xmalloc (nbits);
  if (is_hex)
    {
      char *text = dp_xmalloc (digits + 1u);
      memcpy (text, p + 2, digits);
      text[digits]     = '\0';
      const size_t got = dp_hex_to_bin (text, b, nbits, DP_BITORDER_BIG);
      free (text);
      if (got != nbits)
        {
          free (b);
          return field_refuse (why, "a hex literal holds a character other "
                                    "than 0-9, a-f or A-F");
        }
    }
  else
    for (size_t i = 0; i < n; i++)
      {
        if (p[i] != '0' && p[i] != '1')
          {
            free (b);
            return field_refuse (why, "a binary literal holds a character "
                                      "other than 0 or 1");
          }
        b[i] = (uint8_t)(p[i] - '0');
      }

  memset (q, 0, sizeof *q);
  q->kind = WFM_SEQ_LITERAL;
  q->len  = nbits;
  q->bits = b;
  *owned  = b;
  return DP_OK;
}

/* A generated field -- `pn:`, `gold:` or `dotted:` -- or `data:`, the kind
   word being the same SEQ_KIND_NAMES spelling the JSON scene and the CLI
   use. */
static int
parse_generated (const char *p, size_t n, wfm_seq_t *q, const char **why)
{
  field_tok_t  t[FIELD_MAX_TOKENS];
  const size_t k = split_colons (p, n, t);

  /* The two words of the retired `--data prbs|none` choice (#1619 F6a),
     refused by name: `--data` is the Field grammar and nothing else. */
  if (k == 1 && tok_is (t[0], "none"))
    return field_refuse (why, "none is not a Field: code-only continuous "
                              "dsss is --code-only (\"code_only\" in a "
                              "scene)");
  if (k == 1 && tok_is (t[0], "prbs"))
    return field_refuse (why, "prbs is not a Field: the seeded PN is "
                              "continuous dsss's default, so omit --data "
                              "(a seeded stream on --data is doppler#1717)");
  int kind = -1;
  for (int i = 0; i < (int)(sizeof SEQ_KIND_NAMES / sizeof *SEQ_KIND_NAMES);
       i++)
    if (tok_is (t[0], SEQ_KIND_NAMES[i]))
      kind = i;
  if (kind == WFM_SEQ_LITERAL)
    return field_refuse (why, "a literal is written as its bits (0101) or "
                              "in hex (0x5), not as literal:");
  if (kind < 0)
    return field_refuse (why, "a field is 0/1 bits, 0x hex, or starts "
                              "with pn:, gold:, dotted: or data:");
  if (k > FIELD_MAX_TOKENS)
    return field_refuse (why, "too many ':' fields");

  wfm_seq_t s;
  memset (&s, 0, sizeof s);
  s.kind = (wfm_seq_kind_t)kind;

  uint64_t len;
  if (k < 2 || tok_u64 (t[1], &len) != 0 || len == 0 || len > SIZE_MAX)
    return field_refuse (why, "LEN, the output length in bits, must be a "
                              "number > 0");
  s.len = (size_t)len;

  if (s.kind == WFM_SEQ_DOTTED)
    {
      if (k != 2)
        return field_refuse (why, "dotted takes only a length: dotted:LEN");
      *q = s;
      return DP_OK;
    }
  if (s.kind == WFM_SEQ_DATA)
    {
      if (k != 2)
        return field_refuse (why, "data takes only a length: data:LEN");
      *q = s;
      return DP_OK;
    }

  uint64_t reg;
  if (k < 3 || tok_u64 (t[2], &reg) != 0 || reg == 0 || reg > 64)
    return field_refuse (why, "REG, the register width, must be 1..64");
  s.reg_bits = (uint32_t)reg;

  /* A SEED, POLY or tap is a register's worth of bits. The generators mask
     a wider one, silently -- 32 on a 5-bit register is the all-zero one --
     so it is refused here instead (doppler#1624; pn_fits_register). */
  if (s.kind == WFM_SEQ_GOLD)
    {
      uint64_t v[4];
      if (k != 7)
        return field_refuse (
            why, "gold is gold:LEN:REG:TAPS_A:SEED_A:TAPS_B:SEED_B");
      for (size_t i = 0; i < 4; i++)
        if (tok_u64 (t[3 + i], &v[i]) != 0)
          return field_refuse (why, "a gold tap or seed is not a number");
      for (size_t i = 0; i < 4; i++)
        if (!pn_fits_register (v[i], s.reg_bits))
          return field_refuse (why, "a gold tap or seed has a bit above "
                                    "its REG-bit register");
      s.taps_a = v[0];
      s.seed_a = v[1];
      s.taps_b = v[2];
      s.seed_b = v[3];
      *q       = s;
      return DP_OK;
    }

  /* pn: after REG, up to two numbers (SEED, POLY), then optionally the
     register form as a WORD -- last, so it can never be mistaken for a
     number's position. */
  size_t nums = k - 3u;
  if (nums > 0)
    for (int i = 0; i < 2; i++)
      if (tok_is (t[k - 1u], LFSR_NAMES[i]))
        {
          s.lfsr = i;
          nums--;
          break;
        }
  if (nums > 2)
    return field_refuse (why, "pn is pn:LEN:REG[:SEED[:POLY]][:galois|"
                              "fibonacci]");
  if (nums > 0 && tok_u64 (t[3], &s.seed) != 0)
    return field_refuse (why, "a pn SEED is not a number");
  if (nums > 1 && tok_u64 (t[4], &s.poly) != 0)
    return field_refuse (why, "a pn POLY is not a number");
  if (!pn_fits_register (s.seed | s.poly, s.reg_bits))
    return field_refuse (why, "a pn SEED or POLY has a bit above its "
                              "REG-bit register");
  /* No POLY means "the maximal-length one for this register", and a 1-bit
     register has none: the render refuses it (doppler#1602), so accepting
     it here only moved the refusal to a later point on every face -- where
     it arrived without a reason. Refused where the text is read, and the
     one remedy that makes it buildable is named. */
  if (s.poly == 0 && pn_mls_poly (s.reg_bits) == 0)
    return field_refuse (why, "a pn REG this narrow has no maximal-length "
                              "polynomial: give POLY, or a wider REG");
  *q = s;
  return DP_OK;
}

int
dp_wfm_field_parse (const char *spec, wfm_field_t *field, uint8_t **owned,
                    const char **why)
{
  if (why)
    *why = NULL;
  if (!field || !owned)
    return field_refuse (why, "no field to parse into");
  if (!spec || !*spec)
    return field_refuse (why, "an empty field");

  /* `*REPS`, at most once, and last. */
  size_t      n    = strlen (spec);
  uint64_t    reps = 1;
  const char *star = strchr (spec, '*');
  if (star)
    {
      field_tok_t r = { star + 1, n - (size_t)(star + 1 - spec) };
      if (tok_u64 (r, &reps) != 0 || reps == 0)
        return field_refuse (why, "*REPS must be a number >= 1");
      n = (size_t)(star - spec);
      if (n == 0)
        return field_refuse (why, "*REPS repeats nothing");
    }

  wfm_seq_t q;
  uint8_t  *mine = NULL;
  int       rc;
  if (spec[0] == '0' || spec[0] == '1')
    rc = parse_literal (spec, n, &q, &mine, why);
  else
    rc = parse_generated (spec, n, &q, why);
  if (rc != DP_OK)
    return rc;

  /* LEN and REPS are each at most WFM_FIELD_MAX_BITS here, so the product
     is computed in 64 bits without wrapping. */
  if (q.len > WFM_FIELD_MAX_BITS || reps > WFM_FIELD_MAX_BITS
      || (uint64_t)q.len * reps > WFM_FIELD_MAX_BITS)
    {
      free (mine);
      return field_refuse (why, FIELD_TOO_LONG);
    }

  memset (field, 0, sizeof *field);
  field->seq  = q;
  field->reps = (size_t)reps;
  *owned      = mine;
  return DP_OK;
}

/* Append @p s at @p *at when @p dst is non-NULL; count it either way. */
static void
put (char *dst, size_t *at, const char *s)
{
  const size_t n = strlen (s);
  if (dst)
    memcpy (dst + *at, s, n);
  *at += n;
}

/* A register parameter: 0 as `0`, anything else in hex -- taps and seeds
   are written in hex in the literature, and the record already does. */
static void
put_hex (char *dst, size_t *at, uint64_t v)
{
  char b[24];
  if (v == 0)
    (void)snprintf (b, sizeof b, ":0");
  else
    (void)snprintf (b, sizeof b, ":0x%llx", (unsigned long long)v);
  put (dst, at, b);
}

/* The canonical text of @p f at @p dst (NULL: count only). Returns the
   length, or 0 when @p f has no text form. */
static size_t
field_emit (const wfm_field_t *f, char *dst)
{
  const wfm_seq_t *s  = &f->seq;
  size_t           at = 0;
  char             b[48];

  switch (s->kind)
    {
    case WFM_SEQ_LITERAL:
      if (!s->bits)
        return 0;
      if (s->len % 4u == 0)
        {
          put (dst, &at, "0x");
          /* dp_bin_to_hex writes its NUL too; room for it is ensured by the
             caller, which sizes with a NULL pass first. */
          if (dst
              && dp_bin_to_hex (s->bits, s->len, (uint8_t *)dst + at,
                                s->len / 4u + 1u, DP_BITORDER_BIG)
                     != s->len / 4u)
            return 0;
          at += s->len / 4u;
        }
      else
        {
          for (size_t i = 0; i < s->len; i++)
            if (dst)
              dst[at + i] = (char)('0' + (s->bits[i] & 1u));
          at += s->len;
        }
      break;

    case WFM_SEQ_DOTTED:
      (void)snprintf (b, sizeof b, "dotted:%zu", s->len);
      put (dst, &at, b);
      break;

    case WFM_SEQ_DATA:
      (void)snprintf (b, sizeof b, "data:%zu", s->len);
      put (dst, &at, b);
      break;

    case WFM_SEQ_PN:
      (void)snprintf (b, sizeof b, "pn:%zu:%u", s->len, s->reg_bits);
      put (dst, &at, b);
      /* A zero seed or poly is the default and is omitted -- unless the
         poly follows it, which needs the seed's position filled. */
      if (s->seed || s->poly)
        put_hex (dst, &at, s->seed);
      if (s->poly)
        put_hex (dst, &at, s->poly);
      if (s->lfsr == 1)
        {
          put (dst, &at, ":");
          put (dst, &at, LFSR_NAMES[1]);
        }
      break;

    case WFM_SEQ_GOLD:
      (void)snprintf (b, sizeof b, "gold:%zu:%u", s->len, s->reg_bits);
      put (dst, &at, b);
      put_hex (dst, &at, s->taps_a);
      put_hex (dst, &at, s->seed_a);
      put_hex (dst, &at, s->taps_b);
      put_hex (dst, &at, s->seed_b);
      break;

    default:
      return 0;
    }

  if (f->reps > 1)
    {
      (void)snprintf (b, sizeof b, "*%zu", f->reps);
      put (dst, &at, b);
    }
  return at;
}

size_t
dp_wfm_field_format (const wfm_field_t *field, char *buf, size_t cap)
{
  if (!field || field->derived_by || field->seq.len == 0)
    return 0;
  const size_t n = field_emit (field, NULL);
  if (n == 0 || !buf || n + 1u > cap)
    return n;
  if (field_emit (field, buf) != n)
    return 0;
  buf[n] = '\0';
  return n;
}

size_t
dp_wfm_field_bits (const char *spec, uint8_t *out, size_t max_out,
                   const char **why)
{
  wfm_field_t f;
  uint8_t    *owned = NULL;
  if (dp_wfm_field_parse (spec, &f, &owned, why) != DP_OK)
    return 0;
  if (f.seq.kind == WFM_SEQ_DATA)
    {
      (void)field_refuse (why, "data:LEN has no bits of its own: they "
                               "come from the frame's data source");
      return 0;
    }
  size_t n = supplied_bits (&f);
  if (out)
    {
      /* The parser refuses what a generator cannot build, so with room
         the render writes all n: the explore corpus holds it for every
         text it accepts (native/validation/wfm_field_explore.c). */
      if (n > max_out)
        {
          (void)field_refuse (why, "the output is smaller than the field");
          n = 0;
        }
      else
        (void)dp_wfm_field_render (&f, out, max_out);
    }
  free (owned);
  return n;
}

int
dp_wfm_frame_desc_layout (const wfm_frame_desc_t  *d,
                          wfm_frame_desc_layout_t *out)
{
  if (!d || !out)
    return -1;
  if (d->n_fields > WFM_FRAME_MAX_FIELDS || d->n_stages > WFM_FRAME_MAX_STAGES)
    return -1;
  memset (out, 0, sizeof *out);
  out->n_fields = d->n_fields;
  out->n_stages = d->n_stages;

  for (unsigned s = 0; s < d->n_stages; s++)
    {
      const wfm_stage_t *st = &d->stage[s];
      if (st->n_fields && (size_t)st->first_field + st->n_fields > d->n_fields)
        return -1;
    }

  /* 1. what the caller supplied.
   *
   * A field that declares a LENGTH but supplies no bits is derived, and a
   * derived field with no producing stage is refused (doppler#1155). It used
   * to fall through here at zero length: the frame came out short, the stage
   * that should have filled it ran over a cover whose tail no longer existed,
   * and the record was generated with exit 0 and nothing on stderr. Measured
   * on the guide's own description -- 40 bits rather than 56, with the
   * payload's own bits not surviving.
   *
   * Refused HERE, where geometry is decided, so it covers every reader at
   * once: the builder, the scene JSON, the CLI and Python all funnel through
   * this function. The builder cannot reach the state anyway --
   * dp_wfm_frame_add_stage() wires the producer from the cover it is given --
   * so the check exists for the readers that take the two facts as
   * independent integers and could otherwise let them disagree. */
  /* A frame draws from ONE data source, so it carries at most one data
     field: a second would be a second position for the same bits, with no
     rule for which chunk lands where. */
  unsigned n_data = 0;
  for (unsigned i = 0; i < d->n_fields; i++)
    {
      const wfm_field_t *f = &d->field[i];
      if (!f->derived_by && f->bits && f->seq.len == 0)
        return -1;
      if (!f->derived_by && f->seq.kind == WFM_SEQ_DATA && f->seq.len
          && ++n_data > 1u)
        return -1;
      out->field_bits[i] = supplied_bits (f);
    }

  /* 2. size the derived fields. A stage that covers no supplied bits derives
        nothing — the general form of "a CRC over an empty payload protects
        nothing", which this file has always applied to that one case. */
  for (unsigned i = 0; i < d->n_fields; i++)
    {
      const wfm_field_t *f = &d->field[i];
      if (!f->derived_by)
        continue;
      const unsigned si = f->derived_by - 1u;
      if (si >= d->n_stages)
        return -1;

      const wfm_stage_t *st = &d->stage[si];

      /* A derived field must be the LAST field of its producing stage's
         cover. That is what lets one in-place op signature serve a CRC, an
         outer code and a randomiser alike — the kernel gets the whole span,
         reads the information at its head and writes the check symbols into
         its tail. A description that breaks it would hand a kernel a span
         whose shape it cannot know, so it is refused here rather than
         producing a frame with the parity in the middle of the data. */
      if (st->n_fields && (unsigned)(st->first_field + st->n_fields - 1u) != i)
        return -1;

      size_t src = 0;
      for (unsigned c = 0; c < st->n_fields; c++)
        src += supplied_bits (&d->field[st->first_field + c]);
      out->field_bits[i] = src ? f->bits : 0u;
    }

  /* 3. offsets, in wire order */
  size_t off = 0;
  for (unsigned i = 0; i < d->n_fields; i++)
    {
      out->field_off[i] = off;
      off += out->field_bits[i];
    }
  out->frame_bits = off;

  /* 4. each stage's span, from its DECLARED cover */
  for (unsigned s = 0; s < d->n_stages; s++)
    {
      const wfm_stage_t *st = &d->stage[s];
      size_t             n  = 0;
      for (unsigned c = 0; c < st->n_fields; c++)
        n += out->field_bits[st->first_field + c];
      if (n)
        {
          out->stage[s].first = out->field_off[st->first_field];
          out->stage[s].n     = n;
        }
    }

  /* 5. A stage that emits a different stream sets the output length.
   *
   * The arithmetic here used to be `(frame_bits - cov) + cov * num / den`,
   * which reads as "the bits it does not cover pass through at their own
   * width". Nothing implements that. `dp_wfm_frame_assemble` hands `emit` the
   * WHOLE assembled frame and requires exactly `out_bits` back, so a
   * partially-covering emitting stage laid out cleanly and could then never
   * assemble: the kernel expands bits it was never promised and returns a
   * count that is not `out_bits`. The caller saw a 0 from `assemble` and had
   * no way to learn the description, rather than the data, was wrong.
   *
   * So the cover must be the whole frame, and there may be only one such
   * stage -- a second would have to consume the first's output, which
   * nothing passes it. Both are refused here, where the geometry is decided,
   * rather than discovered as a silent 0 later. A stage that rewrites PART
   * of a frame is the in-place kind; that is what a partial cover is for. */
  out->out_bits = out->frame_bits;
  int emitting  = 0;
  for (unsigned s = 0; s < d->n_stages; s++)
    {
      const wfm_stage_t *st = &d->stage[s];
      if (!st->emit_num || !st->emit_den || out->stage[s].n == 0)
        continue;
      if (emitting || out->stage[s].n != out->frame_bits)
        return -1;
      emitting      = 1;
      out->out_bits = out->frame_bits * st->emit_num / st->emit_den;
    }
  return 0;
}

int
dp_wfm_frame_fixed (wfm_frame_desc_t *d, const wfm_seq_t *preamble,
                    size_t reps, const wfm_seq_t *sync,
                    const wfm_seq_t *payload, int crc)
{
  if (!d)
    return -1;
  memset (d, 0, sizeof *d);

  /* A field is included on its LENGTH, never on its pointer: a length with
     no array is an unbuildable description, and it has to reach
     dp_wfm_frame_assemble to be refused there rather than be dropped here
     and assemble a frame quietly missing it. A preamble needs a repetition
     count as well, because `reps` is how many periods go on the wire and
     zero of them is none. */
  if (preamble && preamble->len && reps
      && dp_wfm_frame_add_field (d, "preamble", preamble, reps) < 0)
    return -1;
  if (sync && sync->len && dp_wfm_frame_add_field (d, "sync", sync, 0u) < 0)
    return -1;

  /* The payload is a field even when it is empty, so a CRC stage always has
     one to cover: a CRC over nothing protects nothing, and the layout
     reports that stage as not run rather than as a trailer of 16 bits. */
  static const wfm_seq_t none = { .kind = WFM_SEQ_LITERAL };
  if (dp_wfm_frame_add_field (d, "payload", payload ? payload : &none, 0u) < 0)
    return -1;
  if (crc
      && (dp_wfm_frame_add_derived (d, "crc", WFM_FRAME_CRC_BITS) < 0
          || dp_wfm_frame_add_stage (d, WFM_STAGE_CRC16, "payload", "crc")
                 < 0))
    return -1;
  return 0;
}

/* CRC-16-CCITT over the head of the span, written MSB-first into its tail.
 *
 * The built-in, because `dp_crc16.h` is already a dependency of this file and
 * a CRC is not a property of any one standard. Everything else -- an outer
 * code, a randomiser, an inner code -- belongs to the component that
 * configures it and arrives through wfm_frame_ops_t.
 *
 * `n` is the whole cover, information followed by the 16-bit trailer this
 * derives, so the protected length is `n - WFM_FRAME_CRC_BITS`. */
static int
crc16_in_unit (const wfm_stage_t *st, uint8_t *bits, size_t n, void *user)
{
  (void)st;
  (void)user;
  if (n <= WFM_FRAME_CRC_BITS)
    return -1;
  const size_t   prot = n - WFM_FRAME_CRC_BITS;
  const uint16_t c    = dp_crc16_ccitt (bits, prot);
  for (size_t i = 0; i < WFM_FRAME_CRC_BITS; i++)
    bits[prot + i] = (uint8_t)((c >> (15 - i)) & 1u); /* MSB-first */
  return 0;
}

/* The receive side of the same rule: what the CRC protects is everything the
 * span covers except the trailer it derived, so recompute over the head and
 * compare with the tail. Nothing is corrected -- a CRC detects and cannot
 * repair -- so `corrected` and `symbols` stay zero and `ok` is the verdict. */
static int
crc16_undo (const wfm_stage_t *st, uint8_t *bits, size_t n,
            wfm_frame_stage_rx_t *rx, void *user)
{
  (void)st;
  (void)user;
  if (n <= WFM_FRAME_CRC_BITS)
    return -1;
  const size_t   prot = n - WFM_FRAME_CRC_BITS;
  const uint16_t want = dp_crc16_ccitt (bits, prot);
  uint16_t       got  = 0;
  for (size_t i = 0; i < WFM_FRAME_CRC_BITS; i++)
    got = (uint16_t)((got << 1) | (bits[prot + i] & 1u));

  rx->units   = 1u;
  rx->ok      = (want == got) ? 1u : 0u;
  rx->checked = 1;
  return 0;
}

/* ── the block interleaver ───────────────────────────────────────────────
 *
 * A permutation, so the cover it occupies on the wire is exactly what it
 * reads: no derived field, no expansion, `emit_num == 0`. That makes it the
 * simplest stage class there is, and the second BUILTIN — unlike the outer
 * and inner codes it needs no configuration a component has to supply, so it
 * does not belong in a `wfm_frame_ops_t` table.
 *
 * The geometry is `depth` rows of `unit_bits`-wide units, and the COLUMN
 * count is derived from the span: a stage's cover is what says how much
 * there is to permute, and deriving the other way -- fixing columns and
 * letting the row count fall out -- would silently change the permutation
 * when a payload length changed. A span that is not a whole number of
 * `depth * unit_bits` units is REFUSED, because there is no honest thing to
 * do with the remainder: padding changes the length and dropping it loses
 * bits.
 *
 * Out of place, because a transpose is: the scratch is one allocation per
 * stage application over a frame-sized buffer, which is the same order as
 * the frame itself. */
static int
ilv_geometry (const wfm_stage_t *st, size_t n, size_t *rows, size_t *cols,
              size_t *unit)
{
  const size_t r = st->depth ? (size_t)st->depth : 1u;
  const size_t u = st->unit_bits ? (size_t)st->unit_bits : 1u;
  if (n == 0 || r == 0 || u == 0)
    return -1;
  if (n % (r * u) != 0)
    return -1;
  *rows = r;
  *cols = n / (r * u);
  *unit = u;
  return 0;
}

static int
ilv_apply (const wfm_stage_t *st, uint8_t *bits, size_t n, int forward)
{
  size_t rows, cols, unit;
  if (ilv_geometry (st, n, &rows, &cols, &unit) != 0)
    return -1;
  uint8_t *tmp = (uint8_t *)malloc (n);
  if (!tmp)
    return -1;
  if (forward)
    dp_interleave_u8 (bits, tmp, rows, cols, unit);
  else
    dp_deinterleave_u8 (bits, tmp, rows, cols, unit);
  memcpy (bits, tmp, n);
  free (tmp);
  return 0;
}

static int
ilv_in_unit (const wfm_stage_t *st, uint8_t *bits, size_t n, void *user)
{
  (void)user;
  return ilv_apply (st, bits, n, 1);
}

/* De-interleaving detects nothing, so it reports one unit that is always
 * good -- the same answer, and for the same reason, that the derandomiser
 * gives: it cannot fail, it can only be pointed at the wrong bits, and the
 * stage that catches THAT is the one after it. */
static int
ilv_undo (const wfm_stage_t *st, uint8_t *bits, size_t n,
          wfm_frame_stage_rx_t *rx, void *user)
{
  (void)user;
  if (ilv_apply (st, bits, n, 0) != 0)
    return -1;
  rx->units   = 1u;
  rx->ok      = 1u;
  rx->checked = 1;
  return 0;
}

static const wfm_stage_op_t BUILTIN[] = {
  { WFM_STAGE_CRC16, crc16_in_unit, NULL, crc16_undo },
  { WFM_STAGE_INTERLEAVE, ilv_in_unit, NULL, ilv_undo },
};

static const wfm_stage_op_t *
find_op (const wfm_frame_ops_t *ops, uint32_t kind)
{
  if (ops)
    {
      for (unsigned i = 0; i < ops->n_op; i++)
        {
          if (ops->op[i].kind == kind)
            return &ops->op[i];
        }
    }
  for (size_t i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++)
    {
      if (BUILTIN[i].kind == kind)
        return &BUILTIN[i];
    }
  return NULL;
}

size_t
dp_wfm_frame_assemble (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                       uint8_t *out, size_t max_out)
{
  return dp_wfm_frame_assemble_data (d, ops, NULL, out, max_out);
}

size_t
dp_wfm_frame_assemble_data (const wfm_frame_desc_t *d,
                            const wfm_frame_ops_t *ops, const uint8_t *data,
                            uint8_t *out, size_t max_out)
{
  wfm_frame_desc_layout_t l;
  if (!d || !out || dp_wfm_frame_desc_layout (d, &l) != 0)
    return 0;
  if (l.out_bits == 0 || l.out_bits > max_out)
    return 0;

  /* A data field's bits are the caller's chunk; without one the frame is
     refused BEFORE anything is written, like an unrunnable stage below. */
  for (unsigned i = 0; i < d->n_fields; i++)
    if (!d->field[i].derived_by && d->field[i].seq.kind == WFM_SEQ_DATA
        && l.field_bits[i] && !data)
      return 0;

  /* Every stage must have a kernel BEFORE anything is written. A stage
     discovered to be unrunnable half way through would leave a partly coded
     frame in the caller's buffer, which is the shape `dp_wfm_seq_bits` already
     refuses for a field: a frame that half-materialises is scored against a
     truth nobody can reproduce. */
  for (unsigned s = 0; s < d->n_stages; s++)
    {
      if (l.stage[s].n == 0)
        continue; /* declared but not running -- nothing to look up */
      const wfm_stage_op_t *op = find_op (ops, d->stage[s].kind);
      if (!op || (op->in_unit == NULL) == (op->emit == NULL))
        return 0;
    }

  /* The frame is assembled in the TAIL of the buffer when a stage expands it
     into a different stream, so the stream can be written from the head with
     no scratch allocation. With no such stage the tail IS the buffer. */
  uint8_t *frame = out + (l.out_bits - l.frame_bits);

  for (unsigned i = 0; i < d->n_fields; i++)
    {
      const wfm_field_t *f = &d->field[i];
      const size_t       n = l.field_bits[i];
      if (n == 0 || f->derived_by)
        continue; /* absent, or written by the stage that derives it */

      if (f->seq.kind == WFM_SEQ_DATA)
        memcpy (frame + l.field_off[i], data, n); /* LEN * REPS, as drawn */
      else if (dp_wfm_field_render (f, frame + l.field_off[i], n) != n)
        return 0;
    }

  for (unsigned s = 0; s < d->n_stages; s++)
    {
      if (l.stage[s].n == 0)
        continue;
      const wfm_stage_op_t *op = find_op (ops, d->stage[s].kind);
      void                 *u  = ops ? ops->user : NULL;
      if (op->in_unit)
        {
          if (op->in_unit (&d->stage[s], frame + l.stage[s].first,
                           l.stage[s].n, u)
              != 0)
            return 0;
        }
      else if (op->emit (&d->stage[s], frame, l.frame_bits, out, max_out, u)
               != l.out_bits)
        return 0;
    }
  return l.out_bits;
}

int
dp_wfm_frame_check (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                    uint8_t *bits, wfm_frame_rx_t *rx)
{
  wfm_frame_desc_layout_t l;
  wfm_frame_rx_t          out;
  if (!d || !bits || dp_wfm_frame_desc_layout (d, &l) != 0)
    return -1;

  memset (&out, 0, sizeof out);
  out.n_stages = d->n_stages;

  /* REVERSE order. The stages were applied in declaration order, each over
     its own span, so undoing them in the same order would hand a kernel bits
     a later stage is still sitting on top of. */
  for (unsigned k = d->n_stages; k-- > 0;)
    {
      if (l.stage[k].n == 0)
        continue; /* declared but not running */

      const wfm_stage_op_t *op = find_op (ops, d->stage[k].kind);
      if (!op || !op->undo)
        continue; /* not reversed here -- reported as unchecked, not passed */

      if (op->undo (&d->stage[k], bits + l.stage[k].first, l.stage[k].n,
                    &out.stage[k], ops ? ops->user : NULL)
          != 0)
        return -1;
      out.checked++;
    }

  if (rx != NULL)
    *rx = out;
  if (out.checked == 0)
    return -1; /* carries no check != the check passed */

  for (unsigned k = 0; k < d->n_stages; k++)
    {
      if (out.stage[k].checked && out.stage[k].ok != out.stage[k].units)
        return 0;
    }
  return 1;
}

int
dp_wfm_frame_desc_crc_ok (const wfm_frame_desc_t *d, const uint8_t *rx_bits)
{
  wfm_frame_desc_layout_t l;
  if (!d || !rx_bits || dp_wfm_frame_desc_layout (d, &l) != 0)
    return -1;

  for (unsigned s = 0; s < d->n_stages; s++)
    {
      if (d->stage[s].kind != WFM_STAGE_CRC16 || l.stage[s].n == 0)
        continue;

      /* The trailer is the last field of the cover — the rule that lets one
         in-place kernel signature serve every check-symbol stage, read back
         here from the other side. What the CRC protects is everything the
         stage covers except the trailer it derived. */
      const unsigned last
          = d->stage[s].first_field + d->stage[s].n_fields - 1u;
      const size_t tr = l.field_bits[last];
      if (tr == 0 || tr > l.stage[s].n)
        return -1;
      const size_t prot = l.stage[s].n - tr;

      const uint16_t want = dp_crc16_ccitt (rx_bits + l.stage[s].first, prot);
      uint16_t       got  = 0;
      for (size_t i = 0; i < tr; i++)
        got = (uint16_t)((got << 1)
                         | (rx_bits[l.stage[s].first + prot + i] & 1u));
      return want == got;
    }
  return -1; /* no CRC stage: "carries no check" is not "the check failed" */
}

/* ── the DSSS burst: a FRAME, then spread ──────────────────────────────
 *
 * These live here rather than in wfm_dsp.c because they are frame functions:
 * what they do is assemble the layout above and spread it. Keeping them beside
 * the descriptor is also what keeps `wfm_dsp_core` -- spreading and RRC taps,
 * linked by every receiver that wants a matched filter -- free of the pn/gold
 * dependency the generated sequence kinds carry.
 */
size_t
dp_wfm_dsss_desc_nchips (const wfm_frame_desc_t *d, size_t acq_len,
                         size_t acq_reps, size_t data_len)
{
  wfm_frame_desc_layout_t l;
  if (!d || dp_wfm_frame_desc_layout (d, &l) != 0)
    return 0;
  const size_t pre = acq_len * acq_reps;
  if (l.out_bits && data_len == 0)
    return 0;                         /* frame bits with no spreading code */
  return pre + l.out_bits * data_len; /* 0 when there is nothing to send */
}

size_t
dp_wfm_dsss_desc_chips (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                        const uint8_t *acq_code, size_t acq_len,
                        size_t acq_reps, const uint8_t *data_code,
                        size_t data_len, uint8_t *out, size_t max_out)
{
  return dp_wfm_dsss_desc_chips_data (d, ops, NULL, acq_code, acq_len,
                                      acq_reps, data_code, data_len, out,
                                      max_out);
}

size_t
dp_wfm_dsss_desc_chips_data (const wfm_frame_desc_t *d,
                             const wfm_frame_ops_t *ops, const uint8_t *data,
                             const uint8_t *acq_code, size_t acq_len,
                             size_t acq_reps, const uint8_t *data_code,
                             size_t data_len, uint8_t *out, size_t max_out)
{
  const size_t total
      = dp_wfm_dsss_desc_nchips (d, acq_len, acq_reps, data_len);
  if (total == 0 || total > max_out || !out)
    return 0;

  wfm_frame_desc_layout_t l;
  if (dp_wfm_frame_desc_layout (d, &l) != 0)
    return 0;

  /* Assemble first, spread second. The description says what the frame IS --
     which fields, which stages, and the span each stage covers -- and every
     answer about the bits comes from `dp_wfm_frame_assemble`. A stage whose
     kernel this caller did not supply makes the assembly fail, and the burst
     is then refused rather than transmitted without it. */
  uint8_t *bits = (l.out_bits > 0) ? malloc (l.out_bits) : NULL;
  if (l.out_bits > 0
      && (!bits
          || dp_wfm_frame_assemble_data (d, ops, data, bits, l.out_bits)
                 != l.out_bits))
    {
      free (bits);
      return 0;
    }

  size_t w = 0;
  /* The preamble is NOT a frame field here, and that is the DSSS-specific
     decision: it is unmodulated, unspread and uncoded, because it is the
     coherent pull-in target a receiver correlates raw chips against. So a
     description handed to this function covers exactly what gets spread, and
     an inner code that "covers everything" covers everything SPREAD. */
  for (size_t r = 0; r < acq_reps; r++)
    for (size_t i = 0; i < acq_len; i++)
      out[w++] = acq_code[i] & 1u;
  /* Every frame bit spread by the data code: a 0 bit transmits the code
     as-is, a 1 bit transmits it inverted. */
  for (size_t i = 0; i < l.out_bits; i++)
    for (size_t j = 0; j < data_len; j++)
      out[w++] = (uint8_t)(bits[i] ^ (data_code[j] & 1u));

  free (bits);
  return w;
}

int
dp_wfm_frame_field_index (const wfm_frame_desc_t *d, const char *name)
{
  /* No empty-name guard here, deliberately: the anonymous-field skip below
     already returns -1 for "", and two mechanisms producing one behaviour
     mask each other -- sabotage either and the test stays green, which is
     how a broken guard survives. One mechanism, one test that can fail. */
  if (!d || !name)
    return -1;

  const unsigned n = (d->n_fields <= WFM_FRAME_MAX_FIELDS)
                         ? d->n_fields
                         : WFM_FRAME_MAX_FIELDS;
  for (unsigned i = 0; i < n; i++)
    {
      /* An unnamed field is anonymous, not named "" -- skipping it is what
         stops an unnamed description answering questions about fields it
         does not have. */
      if (d->field[i].name[0] == '\0')
        continue;
      if (strncmp (d->field[i].name, name, WFM_FRAME_NAME_MAX) == 0)
        return (int)i;
    }
  return -1;
}

/* ── building a description by name ──────────────────────────────────────
 *
 * Appending, rather than a constructor per shape. A field count baked into a
 * prototype forces every field's every parameter into it -- which is what
 * `dp_frame_create()`'s 38 arguments are -- so a fifth field has to be an
 * append, not a signature change.
 */

/* A name is taken if any field already carries it. Anonymous fields never
   collide, because an unnamed field is anonymous rather than named "". */
static int
name_taken (const wfm_frame_desc_t *d, const char *name)
{
  return (name && name[0] != '\0') ? (dp_wfm_frame_field_index (d, name) >= 0)
                                   : 0;
}

static void
set_name (wfm_field_t *f, const char *name)
{
  if (!name || name[0] == '\0')
    {
      f->name[0] = '\0';
      return;
    }
  size_t n = strlen (name);
  if (n >= WFM_FRAME_NAME_MAX)
    n = WFM_FRAME_NAME_MAX - 1u;
  memcpy (f->name, name, n);
  f->name[n] = '\0';
}

int
dp_wfm_frame_add_field (wfm_frame_desc_t *d, const char *name,
                        const wfm_seq_t *seq, size_t reps)
{
  if (!d || !seq || d->n_fields >= WFM_FRAME_MAX_FIELDS
      || name_taken (d, name))
    return -1;

  const unsigned i = d->n_fields;
  memset (&d->field[i], 0, sizeof d->field[i]);
  set_name (&d->field[i], name);
  d->field[i].seq  = *seq;
  d->field[i].reps = reps;
  d->n_fields      = i + 1u;
  return (int)i;
}

int
dp_wfm_frame_add_derived (wfm_frame_desc_t *d, const char *name, size_t bits)
{
  if (!d || bits == 0u || d->n_fields >= WFM_FRAME_MAX_FIELDS
      || name_taken (d, name))
    return -1;

  const unsigned i = d->n_fields;
  memset (&d->field[i], 0, sizeof d->field[i]);
  set_name (&d->field[i], name);
  d->field[i].bits = bits;
  /* derived_by stays 0 -- no stage exists yet to name. dp_wfm_frame_add_stage
     wires it when the stage that covers this field arrives. */
  d->n_fields = i + 1u;
  return (int)i;
}

int
dp_wfm_frame_add_stage_at (wfm_frame_desc_t *d, uint32_t kind, unsigned first,
                           unsigned n_fields)
{
  if (!d || d->n_stages >= WFM_FRAME_MAX_STAGES)
    return -1;

  const unsigned s = d->n_stages;
  memset (&d->stage[s], 0, sizeof d->stage[s]);
  d->stage[s].kind        = kind;
  d->stage[s].first_field = first;
  d->stage[s].n_fields    = n_fields;
  d->n_stages             = s + 1u;

  /* Wire the derived field's producer, if the last covered field is one.
     A field with a declared length and no source bits IS derived -- that is
     the definition, not a heuristic -- and dp_wfm_frame_desc_layout already
     refuses any description where such a field is not the last of its
     producing stage's cover. So there is exactly one stage it could name,
     and wiring it here is what stops a caller stating it a second, different
     way. A cover past the fields, or a stage that does not run
     (`n_fields == 0`), wires nothing and is left for the layout to judge. */
  if (n_fields > 0u && first + n_fields <= d->n_fields)
    {
      wfm_field_t *lastf = &d->field[first + n_fields - 1u];
      if (lastf->bits > 0u && lastf->seq.len == 0u && lastf->derived_by == 0u)
        lastf->derived_by = s + 1u; /* stage index, PLUS ONE */
    }
  return (int)s;
}

int
dp_wfm_frame_add_stage (wfm_frame_desc_t *d, uint32_t kind, const char *first,
                        const char *last)
{
  if (!d)
    return -1;
  const int a = dp_wfm_frame_field_index (d, first);
  const int b = dp_wfm_frame_field_index (d, last);
  if (a < 0 || b < 0 || b < a)
    return -1;
  return dp_wfm_frame_add_stage_at (d, kind, (unsigned)a,
                                    (unsigned)(b - a) + 1u);
}
