/*
 * wfm_frame.c — the frame descriptor's geometry and materialisation. The
 * contract, and the reasoning behind each boundary, live on the declarations
 * in wfm/wfm_frame.h.
 */
#include "wfm/wfm_frame.h"

#include "dp_crc16.h"
#include "dp_interleave.h"
#include "gold/gold_core.h"
#include "pn/pn_core.h"
#include "wfm/wfm_dsp.h" /* the DSSS burst assembler declared there */

#include <stdlib.h>
#include <string.h>

/* A field's bits, written at `out`. Returns the count, or 0 if the descriptor
   cannot produce them — which is a REFUSAL, not a short write: a frame that
   half-materialises would be scored against a truth nobody can regenerate. */
static size_t
seq_bits (const wfm_seq_t *s, uint8_t *out, size_t cap)
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
        /* poly 0 is "the maximal-length one for this register", the same
           resolution wfm_synth_create() applies to its --pn-poly. Passing 0
           through to pn_create() instead means a register with NO FEEDBACK:
           it shifts the seed out and emits zeros for ever, which is a
           constant field that still looks like a field. */
        pn_state_t *p
            = pn_create (s->poly ? s->poly : pn_mls_poly (s->reg_bits),
                         s->seed ? s->seed : 1u, s->reg_bits, s->lfsr);
        if (!p)
          return 0;
        size_t n = pn_generate (p, s->len, out, cap);
        pn_destroy (p);
        return n;
      }

    case WFM_SEQ_GOLD:
      {
        gold_state_t *g
            = gold_create (s->taps_a, s->seed_a ? s->seed_a : 1u, s->taps_b,
                           s->seed_b ? s->seed_b : 1u, s->reg_bits);
        if (!g)
          return 0;
        size_t n = gold_generate (g, s->len, out, cap);
        gold_destroy (g);
        return n;
      }
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

int
wfm_frame_desc_layout (const wfm_frame_desc_t *d, wfm_frame_desc_layout_t *out)
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

  /* 1. what the caller supplied */
  for (unsigned i = 0; i < d->n_fields; i++)
    out->field_bits[i] = supplied_bits (&d->field[i]);

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

  /* 5. a stage that emits a different stream sets the output length; the
        bits it does not cover pass through at their own width. */
  out->out_bits = out->frame_bits;
  for (unsigned s = 0; s < d->n_stages; s++)
    {
      const wfm_stage_t *st = &d->stage[s];
      if (!st->emit_num || !st->emit_den || out->stage[s].n == 0)
        continue;
      const size_t cov = out->stage[s].n;
      out->out_bits
          = (out->frame_bits - cov) + cov * st->emit_num / st->emit_den;
    }
  return 0;
}

int
wfm_frame_describe (const wfm_frame_t *f, wfm_frame_desc_t *out)
{
  if (!f || !out)
    return -1;
  memset (out, 0, sizeof *out);

  /* `preamble_reps == 0` means NO preamble, which is this struct's rule and
     not the general one — a field that simply does not repeat leaves `reps`
     zero and is emitted once. So the repetition count is what decides here,
     and a zero one leaves the field empty rather than emitting one period. */
  if (f->preamble_reps)
    {
      out->field[WFM_FRAME_FIELD_PREAMBLE].seq  = f->preamble;
      out->field[WFM_FRAME_FIELD_PREAMBLE].reps = f->preamble_reps;
    }
  out->field[WFM_FRAME_FIELD_SYNC].seq    = f->sync;
  out->field[WFM_FRAME_FIELD_PAYLOAD].seq = f->payload;

  /* The CRC is a field AND a stage: a trailer on the wire, derived by a
     transform covering the payload it protects and the trailer it wrote.
     Both are ALWAYS declared, and `f->crc` unset switches the stage off by
     giving it nothing to cover — which is what an optional stage is in this
     representation, and what `ccsds_tm_frame_layout()` already reports for a
     stage that did not run. Declaring the field either way is also what
     keeps `crc_off` at the end of the payload when the trailer is absent. */
  out->field[WFM_FRAME_FIELD_CRC].bits       = WFM_FRAME_CRC_BITS;
  out->field[WFM_FRAME_FIELD_CRC].derived_by = 1u; /* stage 0, plus one */
  out->n_fields                              = 4u;

  out->stage[0].kind        = WFM_STAGE_CRC16;
  out->stage[0].first_field = WFM_FRAME_FIELD_PAYLOAD;
  out->stage[0].n_fields    = f->crc ? 2u : 0u; /* payload + its own trailer */
  out->n_stages             = 1u;
  return 0;
}

int
wfm_frame_layout (const wfm_frame_t *f, wfm_frame_layout_t *out)
{
  wfm_frame_desc_t        d;
  wfm_frame_desc_layout_t l;
  if (!f || !out || wfm_frame_describe (f, &d) != 0
      || wfm_frame_desc_layout (&d, &l) != 0)
    return -1;
  memset (out, 0, sizeof *out);

  out->preamble_off  = l.field_off[WFM_FRAME_FIELD_PREAMBLE];
  out->preamble_bits = l.field_bits[WFM_FRAME_FIELD_PREAMBLE];
  out->sync_off      = l.field_off[WFM_FRAME_FIELD_SYNC];
  out->sync_bits     = l.field_bits[WFM_FRAME_FIELD_SYNC];
  out->payload_off   = l.field_off[WFM_FRAME_FIELD_PAYLOAD];
  out->payload_bits  = l.field_bits[WFM_FRAME_FIELD_PAYLOAD];
  out->crc_off       = l.field_off[WFM_FRAME_FIELD_CRC];
  out->crc_bits      = l.field_bits[WFM_FRAME_FIELD_CRC];
  out->total_bits    = l.frame_bits;
  return 0;
}

size_t
wfm_frame_nbits (const wfm_frame_t *f)
{
  wfm_frame_layout_t l;
  if (wfm_frame_layout (f, &l) != 0)
    return 0;
  return l.total_bits;
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
find_op (const wfm_frame_ops_t *ops, wfm_stage_kind_t kind)
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
wfm_frame_assemble (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                    uint8_t *out, size_t max_out)
{
  wfm_frame_desc_layout_t l;
  if (!d || !out || wfm_frame_desc_layout (d, &l) != 0)
    return 0;
  if (l.out_bits == 0 || l.out_bits > max_out)
    return 0;

  /* Every stage must have a kernel BEFORE anything is written. A stage
     discovered to be unrunnable half way through would leave a partly coded
     frame in the caller's buffer, which is the shape `seq_bits` already
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

      /* One period, then repeated verbatim — a generated field must repeat
         the SAME bits, not draw fresh ones, or it is not a periodic
         acquisition target and coherent integration across reps is void. */
      if (seq_bits (&f->seq, frame + l.field_off[i], f->seq.len) != f->seq.len)
        return 0;
      for (size_t r = 1; r * f->seq.len < n; r++)
        memcpy (frame + l.field_off[i] + r * f->seq.len,
                frame + l.field_off[i], f->seq.len);
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

size_t
wfm_frame_bits (const wfm_frame_t *f, uint8_t *out, size_t max_out)
{
  wfm_frame_desc_t d;
  if (!f || wfm_frame_describe (f, &d) != 0)
    return 0;
  return wfm_frame_assemble (&d, NULL, out, max_out);
}

int
wfm_frame_check (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                 uint8_t *bits, wfm_frame_rx_t *rx)
{
  wfm_frame_desc_layout_t l;
  wfm_frame_rx_t          out;
  if (!d || !bits || wfm_frame_desc_layout (d, &l) != 0)
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
wfm_frame_desc_crc_ok (const wfm_frame_desc_t *d, const uint8_t *rx_bits)
{
  wfm_frame_desc_layout_t l;
  if (!d || !rx_bits || wfm_frame_desc_layout (d, &l) != 0)
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

int
wfm_frame_crc_ok (const wfm_frame_t *f, const uint8_t *rx_bits)
{
  wfm_frame_desc_t d;
  if (!f || wfm_frame_describe (f, &d) != 0)
    return -1;
  return wfm_frame_desc_crc_ok (&d, rx_bits);
}

/* ── the DSSS burst: a FRAME, then spread ──────────────────────────────
 *
 * These live here rather than in wfm_dsp.c because they are frame functions:
 * what they do is assemble the layout above and spread it. Keeping them beside
 * the descriptor is also what keeps `wfm_dsp_core` -- spreading and RRC taps,
 * linked by every receiver that wants a matched filter -- free of the pn/gold
 * dependency the generated sequence kinds carry. Their declarations stay in
 * wfm/wfm_dsp.h, where every caller already looks for them.
 */
size_t
wfm_dsss_desc_nchips (const wfm_frame_desc_t *d, size_t acq_len,
                      size_t acq_reps, size_t data_len)
{
  wfm_frame_desc_layout_t l;
  if (!d || wfm_frame_desc_layout (d, &l) != 0)
    return 0;
  const size_t pre = acq_len * acq_reps;
  if (l.out_bits && data_len == 0)
    return 0;                         /* frame bits with no spreading code */
  return pre + l.out_bits * data_len; /* 0 when there is nothing to send */
}

size_t
wfm_dsss_desc_chips (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                     const uint8_t *acq_code, size_t acq_len, size_t acq_reps,
                     const uint8_t *data_code, size_t data_len, uint8_t *out,
                     size_t max_out)
{
  const size_t total = wfm_dsss_desc_nchips (d, acq_len, acq_reps, data_len);
  if (total == 0 || total > max_out || !out)
    return 0;

  wfm_frame_desc_layout_t l;
  if (wfm_frame_desc_layout (d, &l) != 0)
    return 0;

  /* Assemble first, spread second. The description says what the frame IS --
     which fields, which stages, and the span each stage covers -- and every
     answer about the bits comes from `wfm_frame_assemble`. A stage whose
     kernel this caller did not supply makes the assembly fail, and the burst
     is then refused rather than transmitted without it. */
  uint8_t *bits = (l.out_bits > 0) ? malloc (l.out_bits) : NULL;
  if (l.out_bits > 0
      && (!bits
          || wfm_frame_assemble (d, ops, bits, l.out_bits) != l.out_bits))
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

/* The four-field DSSS burst, expressed as the general one.
 *
 * `wfm_frame_t` is one configuration of `wfm_frame_desc_t`, so this pair is
 * the description pair with the description filled in — no second layout, no
 * second spreader, and no way for the two spellings to answer differently.
 * Note what is NOT in the description: the acquisition preamble. It is
 * unmodulated and unspread, so it belongs to the waveform rather than to the
 * frame, and `wfm_dsss_desc_chips` prepends it. */
static int
dsss_four_field (const uint8_t *sync, size_t sync_len, const uint8_t *payload,
                 size_t payload_len, int crc, wfm_frame_desc_t *out)
{
  wfm_frame_t f  = { 0 };
  f.sync.kind    = WFM_SEQ_LITERAL;
  f.sync.bits    = sync;
  f.sync.len     = sync_len;
  f.payload.kind = WFM_SEQ_LITERAL;
  f.payload.bits = payload;
  f.payload.len  = payload_len;
  f.crc          = crc;
  return wfm_frame_describe (&f, out);
}

size_t
wfm_frame_dsss_nchips (size_t acq_len, size_t acq_reps, size_t data_len,
                       size_t sync_len, size_t payload_len, int crc)
{
  wfm_frame_desc_t d;
  if (dsss_four_field (NULL, sync_len, NULL, payload_len, crc, &d) != 0)
    return 0;
  return wfm_dsss_desc_nchips (&d, acq_len, acq_reps, data_len);
}

size_t
wfm_frame_dsss_chips (const uint8_t *acq_code, size_t acq_len, size_t acq_reps,
                      const uint8_t *data_code, size_t data_len,
                      const uint8_t *sync, size_t sync_len,
                      const uint8_t *payload, size_t payload_len, int crc,
                      uint8_t *out)
{
  wfm_frame_desc_t d;
  if (dsss_four_field (sync, sync_len, payload, payload_len, crc, &d) != 0)
    return 0;
  const size_t total = wfm_dsss_desc_nchips (&d, acq_len, acq_reps, data_len);
  /* The four-field form has no capacity argument and never had one: its
     caller sizes `out` from _nchips by contract. Pass that same number. */
  return wfm_dsss_desc_chips (&d, NULL, acq_code, acq_len, acq_reps, data_code,
                              data_len, out, total);
}
