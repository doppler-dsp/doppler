/*
 * desc.c — 131.0-B-3 section 9 as a wfm_frame_desc_t, plus the kernels that
 * run it.
 *
 * frame.c is the assembler this component grew first: it knows the four
 * stages by name and runs each over a span it works out itself. This file is
 * the same standard expressed as DATA — three fields and three stages handed
 * to the general assembler in `wfm/wfm_frame.h`, which knows nothing about
 * CCSDS.
 *
 * The two are checked against each other byte for byte
 * (test_ccsds_tm_frame.c), which is the only claim worth making about a
 * generalization: not that it is tidier, but that it produces the same bits
 * as the code that was already falsified against published vectors.
 *
 * ## Why the kernels travel rather than being called
 *
 * `wfm_frame.h` must not depend on `ccsds_tm` — this file depends on IT, to
 * get the descriptor — so the general assembler cannot call
 * `ccsds_tm_randomise` directly without the two components forming a cycle.
 * The transforms are passed in as a table instead. That is a layering
 * requirement first and an extension point second, but it is a real one: a
 * caller with a stage doppler has never heard of supplies its own entry.
 *
 * Every kernel below is the SAME function frame.c calls. Nothing here
 * reimplements a transform; what differs between the two paths is only which
 * bits each stage is handed, and that is exactly what the description states.
 */
#include "ccsds_tm/ccsds_tm_frame.h"

#include <string.h>

/* Field and stage indices. Named because every cover is a range of them, and
   an off-by-one would move a span silently -- which is the failure this whole
   component is shaped around. */
enum
{
  F_ASM = 0,
  F_FRAME,
  F_PARITY,
  N_FIELD
};
enum
{
  S_OUTER = 0,
  S_RAND,
  S_INNER,
  N_STAGE
};

/* The marker's bits, expanded once by the function that owns the expansion.
 * A description carries a pointer to a field's bits, so the pattern needs a
 * home that outlives the call -- and it must not become a second
 * transcription of 0x1ACFFC1D, which is the whole reason
 * ccsds_tm_asm_bits() exists. Filled lazily, in the shape rs.c already uses
 * for its tables. */
static uint8_t asm_pattern[CCSDS_TM_ASM_BITS];
static int     asm_ready = 0;

static const uint8_t *
marker_bits (void)
{
  if (!asm_ready)
    {
      ccsds_tm_asm_bits (asm_pattern);
      asm_ready = 1;
    }
  return asm_pattern;
}

/* Unpacked bits to octets and back, MSB-first: figure 9-1 puts the first
 * transmitted bit of the ASM at the top of 0x1A and 4.3.9.2 orders an R-S
 * symbol the same way, so one direction serves both. frame.c has the same
 * pair for the same reason; they are four lines of bit shuffling rather than
 * a transform, and the alternative is a public header for them. */
static void
pack (const uint8_t *bits, size_t nbytes, uint8_t *bytes)
{
  for (size_t i = 0; i < nbytes; i++)
    {
      uint8_t v = 0;
      for (unsigned b = 0; b < 8u; b++)
        v = (uint8_t)(((unsigned)v << 1u) | (bits[i * 8u + b] & 1u));
      bytes[i] = v;
    }
}

static void
unpack (const uint8_t *bytes, size_t nbytes, uint8_t *bits)
{
  for (size_t i = 0; i < nbytes; i++)
    {
      for (unsigned b = 0; b < 8u; b++)
        bits[i * 8u + b] = (uint8_t)((bytes[i] >> (7u - b)) & 1u);
    }
}

/* 4.3, 4.4.1. The span is the whole codeblock — the information section at
 * its head and the check symbols this derives in its tail — which is the
 * shape wfm_frame.h requires of a derived field, and is also exactly how the
 * standard lays a codeblock out. */
static int
outer_in_unit (const wfm_stage_t *st, uint8_t *bits, size_t n, void *user)
{
  (void)user;
  if (st->depth == 0 || st->depth > CCSDS_TM_RS_MAX_DEPTH
      || n != (size_t)CCSDS_TM_RS_N * st->depth * 8u)
    return -1;

  uint8_t info[CCSDS_TM_RS_K * CCSDS_TM_RS_MAX_DEPTH];
  uint8_t block[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];
  pack (bits, (size_t)CCSDS_TM_RS_K * st->depth, info);
  if (ccsds_tm_rs_encode_block (info, st->depth, block) == 0)
    return -1;

  /* The whole codeblock, not just the parity: 4.4.1 has S2 reassembling the
     information "in the same way as they entered", so the head is unchanged
     and writing it back costs nothing and asserts nothing. */
  unpack (block, (size_t)CCSDS_TM_RS_N * st->depth, bits);
  return 0;
}

/* Section 10. Its own inverse and length-preserving, so the receive side runs
 * the identical call over the identical span. */
/* `depth` carries WHICH generator, because B-6 specifies two: 1 (or 0, the
 * unset default) selects 10.4.1's, 2 selects 10.4.2's legacy sequence. A
 * kernel that picked for itself would produce a waveform the transmitter and
 * the receiver could disagree about, which is the failure this component is
 * shaped around. */
static const ccsds_tm_rand_t *
rand_choice (const wfm_stage_t *st)
{
  return (st->depth == 2u) ? &CCSDS_TM_RAND_LEGACY : &CCSDS_TM_RAND;
}

static int
rand_in_unit (const wfm_stage_t *st, uint8_t *bits, size_t n, void *user)
{
  (void)user;
  ccsds_tm_randomise_with (rand_choice (st), bits, n);
  return 0;
}

/* Section 3. The one stage that consumes the frame and emits a different
 * stream, and the only one whose state outlives the frame: 3.3.2 fixes the
 * output as one uninterrupted symbol sequence, so the register belongs to the
 * caller and arrives as `user`. A NULL says "this frame stands alone" rather
 * than "I forgot", exactly as it does for ccsds_tm_frame_encode. */
static size_t
inner_emit (const wfm_stage_t *st, const uint8_t *in, size_t n, uint8_t *out,
            size_t max_out, void *user)
{
  (void)st;
  conv_enc_t  own;
  conv_enc_t *s = (conv_enc_t *)user;
  if (s == NULL)
    {
      conv_enc_init (&own);
      s = &own;
    }
  return conv_encode (s, &CCSDS_TM_CONV, in, n, out, max_out);
}

/* ── the receive direction ────────────────────────────────────────────
 *
 * The same spans, read from the same description, which is the property that
 * matters: an encoder and a decoder that work out their own coverage cannot
 * be checked against each other, only against a third thing. Here there is no
 * third thing to disagree with.
 */

/* 4.3, 4.4.1 in reverse: de-interleave, decode each codeword up to E = 16
 * symbol errors, write the repairs back. The counts are the point -- an outer
 * code that is correcting is a margin being spent, and that is visible long
 * before the link fails. */
static int
outer_undo (const wfm_stage_t *st, uint8_t *bits, size_t n,
            wfm_frame_stage_rx_t *rx, void *user)
{
  (void)user;
  if (st->depth == 0 || st->depth > CCSDS_TM_RS_MAX_DEPTH
      || n != (size_t)CCSDS_TM_RS_N * st->depth * 8u)
    return -1;

  uint8_t block[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];
  pack (bits, (size_t)CCSDS_TM_RS_N * st->depth, block);

  ccsds_tm_rs_block_rx_t br;
  if (ccsds_tm_rs_decode_block (block, st->depth, &br) == 0)
    return -1;
  unpack (block, (size_t)CCSDS_TM_RS_N * st->depth, bits);

  rx->units     = br.codewords;
  rx->ok        = br.codewords - br.uncorrectable;
  rx->corrected = br.corrected;
  rx->symbols   = br.symbols;
  rx->checked   = 1;
  return 0;
}

/* Section 10, and it is its OWN inverse -- 10.3.4 has the receiver run the
 * identical call over the identical span, which is why this is the same
 * function the transmit side uses. It detects nothing, so it reports one unit
 * that is always good: a derandomiser cannot fail, it can only be pointed at
 * the wrong bits, and the stage that catches THAT is the one after it. */
static int
rand_undo (const wfm_stage_t *st, uint8_t *bits, size_t n,
           wfm_frame_stage_rx_t *rx, void *user)
{
  (void)user;
  ccsds_tm_randomise_with (rand_choice (st), bits, n);
  rx->units   = 1u;
  rx->ok      = 1u;
  rx->checked = 1;
  return 0;
}

static const wfm_stage_op_t OPS[] = {
  { WFM_STAGE_RS, outer_in_unit, NULL, outer_undo },
  { WFM_STAGE_RANDOMISE, rand_in_unit, NULL, rand_undo },
  /* No undo for the inner code, deliberately. It is streaming and emits its
     decisions `depth` bits late, so it is undone before frame synchronisation
     and a frame checker never sees channel symbols -- the boundary
     ccsds_tm_frame.h argues at length is the only place it can go.
     wfm_frame_check reports it as NOT CHECKED rather than as passed. */
  { WFM_STAGE_CONV, NULL, inner_emit, NULL },
};

void
ccsds_tm_frame_ops (wfm_frame_ops_t *out, conv_enc_t *conv)
{
  if (out == NULL)
    return;
  out->op   = OPS;
  out->n_op = (unsigned)(sizeof OPS / sizeof OPS[0]);
  out->user = conv;
}

int
ccsds_tm_frame_describe (const ccsds_tm_frame_cfg_t *cfg, size_t frame_len,
                         const uint8_t *frame_bits, wfm_frame_desc_t *out)
{
  /* The refusals are ccsds_tm_frame_layout's, asked of it rather than
     restated: an unallowed depth, an empty frame, or a frame off the
     223*I grid. A second copy of that rule is a second thing to get wrong,
     and this one would be wrong in the direction of describing a CADU the
     encoder refuses to build. */
  if (out == NULL || ccsds_tm_frame_layout (cfg, frame_len, NULL) == 0)
    return -1;

  memset (out, 0, sizeof *out);
  out->n_fields = N_FIELD;
  out->n_stages = N_STAGE;

  /* 9.4.1: the marker is field ZERO, and that placement plus the covers
     below is the entire content of the coverage table. */
  out->field[F_ASM].seq.kind = WFM_SEQ_LITERAL;
  out->field[F_ASM].seq.bits = marker_bits ();
  out->field[F_ASM].seq.len  = cfg->attach_asm ? CCSDS_TM_ASM_BITS : 0u;

  out->field[F_FRAME].seq.kind = WFM_SEQ_LITERAL;
  out->field[F_FRAME].seq.bits = frame_bits;
  out->field[F_FRAME].seq.len  = frame_len * 8u;

  out->field[F_PARITY].bits = (size_t)CCSDS_TM_RS_2E * cfg->rs_depth * 8u;
  out->field[F_PARITY].derived_by = S_OUTER + 1u;

  /* 9.5.1 / 9.2.1.5: the outer code's data space is the Transfer Frame and
     the check symbols it derives — and NOT the marker. */
  out->stage[S_OUTER].kind        = WFM_STAGE_RS;
  out->stage[S_OUTER].depth       = cfg->rs_depth;
  out->stage[S_OUTER].first_field = F_FRAME;
  out->stage[S_OUTER].n_fields    = cfg->rs_depth ? 2u : 0u;

  /* 10.3.2 / 10.3.4 note 1: the same span, for a different reason — which is
     why they are two stages rather than one. */
  out->stage[S_RAND].kind        = WFM_STAGE_RANDOMISE;
  out->stage[S_RAND].first_field = F_FRAME;
  out->stage[S_RAND].n_fields
      = cfg->randomise ? (cfg->rs_depth ? 2u : 1u) : 0u;

  /* 3.2.1 / 9.2.1.4: everything, marker included. The one span that starts
     at bit 0, and the one stage that emits a different stream. */
  out->stage[S_INNER].kind        = WFM_STAGE_CONV;
  out->stage[S_INNER].first_field = F_ASM;
  out->stage[S_INNER].n_fields    = cfg->convolutional ? N_FIELD : 0u;
  out->stage[S_INNER].emit_num    = 2u;
  out->stage[S_INNER].emit_den    = 1u;
  return 0;
}

/* ── the choices, as a description ──────────────────────────────────────
 *
 * The coverage table this file opens with, applied to a caller's fields.
 * It lives here rather than in `wfm/wfm_frame.h` because the covers are
 * THIS standard's: a general description knows what a field and a stage
 * are, and cannot know which covers which.
 *
 * A generator and whatever undoes the frame later both call this, so the
 * two hold one layout rather than each deriving one.
 */
int
ccsds_tm_frame_desc_of (const ccsds_tm_frame_spec_t *s, wfm_frame_desc_t *d)
{
  if (!s || !d)
    return -1;
  memset (d, 0, sizeof *d);

  unsigned i_data = 0, i_crc = 0, i_parity = 0, n = 0;

  /* The marker's bits come from the ONE function that expands them, never a
     second transcription. Static because the description points at it and
     must outlive this call; it is the same 32 bits in every frame. */
  static uint8_t marker[CCSDS_TM_ASM_BITS];
  if (s->attach_asm)
    {
      ccsds_tm_asm_bits (marker);
      d->field[n].seq.kind = WFM_SEQ_LITERAL;
      d->field[n].seq.bits = marker;
      d->field[n].seq.len  = CCSDS_TM_ASM_BITS;
      n++;
    }
  /* A field is included on its LENGTH, never on its pointer being non-NULL:
     a length with no array is an unbuildable descriptor, and it has to reach
     `wfm_frame_assemble` to be refused there. Dropping the field instead
     would assemble a frame that is quietly missing it -- the same silent
     unframing the composer's own tests pin. */
  if (s->preamble_len && s->preamble_reps)
    {
      d->field[n].seq.kind = WFM_SEQ_LITERAL;
      d->field[n].seq.bits = s->preamble;
      d->field[n].seq.len  = s->preamble_len;
      d->field[n].reps     = s->preamble_reps;
      n++;
    }
  if (s->sync_len)
    {
      d->field[n].seq.kind = WFM_SEQ_LITERAL;
      d->field[n].seq.bits = s->sync;
      d->field[n].seq.len  = s->sync_len;
      n++;
    }

  /* The payload is a field even when its bits are unknown: a receiver holds
     the geometry and fills the contents in later. */
  i_data               = n;
  d->field[n].seq.kind = WFM_SEQ_LITERAL;
  d->field[n].seq.bits = s->payload;
  d->field[n].seq.len  = s->payload_len;
  n++;

  /* Stage indices are needed before the stages exist, because a derived
     field names the stage that writes it. They are assigned in APPLICATION
     order: the CRC first, then the outer code over the result, then the
     randomiser, then the inner code. */
  unsigned s_crc = 0, s_rs = 0, s_rand = 0, s_ilv = 0, s_conv = 0, ns = 0;
  if (s->crc)
    s_crc = ns++;
  if (s->rs_depth)
    s_rs = ns++;
  if (s->randomise)
    s_rand = ns++;
  /* The interleaver is the LAST data-group stage, so it is what the channel
     sees; the inner code is applied over everything after it. */
  if (s->interleave_depth)
    s_ilv = ns++;
  if (s->convolutional)
    s_conv = ns++;
  if (ns > WFM_FRAME_MAX_STAGES)
    return -1;

  if (s->crc)
    {
      i_crc                  = n;
      d->field[n].bits       = WFM_FRAME_CRC_BITS;
      d->field[n].derived_by = s_crc + 1u;
      n++;
    }
  if (s->rs_depth)
    {
      i_parity               = n;
      d->field[n].bits       = (size_t)CCSDS_TM_RS_2E * s->rs_depth * 8u;
      d->field[n].derived_by = s_rs + 1u;
      n++;
    }
  if (n > WFM_FRAME_MAX_FIELDS)
    return -1;
  d->n_fields = n;
  d->n_stages = ns;

  /* The data group: payload, its CRC, and the outer code's check symbols.
     Contiguous by construction, and each derived field is the last of its
     own stage's cover, which is what lets one kernel signature serve them
     all. */
  const unsigned data_end = n; /* one past the last data field */

  if (s->crc)
    {
      d->stage[s_crc].kind        = WFM_STAGE_CRC16;
      d->stage[s_crc].first_field = i_data;
      d->stage[s_crc].n_fields    = i_crc - i_data + 1u;
    }
  if (s->rs_depth)
    {
      d->stage[s_rs].kind        = WFM_STAGE_RS;
      d->stage[s_rs].depth       = s->rs_depth;
      d->stage[s_rs].first_field = i_data;
      d->stage[s_rs].n_fields    = i_parity - i_data + 1u;
    }
  if (s->randomise)
    {
      d->stage[s_rand].kind        = WFM_STAGE_RANDOMISE;
      d->stage[s_rand].first_field = i_data;
      d->stage[s_rand].n_fields    = data_end - i_data;
      /* WHICH generator, carried on the stage: 131.0-B-6 specifies two and
         they produce waveforms only the matching receiver derandomises, so
         this is not a detail the kernel may pick for itself. `depth` is the
         stage's free parameter and the randomiser has no other use for it. */
      d->stage[s_rand].depth = (unsigned)s->randomise;
    }
  if (s->interleave_depth)
    {
      /* Last of the data-group stages, so it is what the channel sees. Its
         COLUMN count is derived from the span it covers, so a payload length
         change moves the geometry -- which is correct, and is why both ends
         read the same description rather than each computing one. */
      d->stage[s_ilv].kind        = WFM_STAGE_INTERLEAVE;
      d->stage[s_ilv].depth       = s->interleave_depth;
      d->stage[s_ilv].unit_bits   = s->interleave_unit_bits;
      d->stage[s_ilv].first_field = i_data;
      d->stage[s_ilv].n_fields    = data_end - i_data;
    }
  if (s->convolutional)
    {
      d->stage[s_conv].kind        = WFM_STAGE_CONV;
      d->stage[s_conv].first_field = 0u;
      d->stage[s_conv].n_fields    = n;
      /* 3.2.1: rate 1/2, so the frame it covers leaves twice as long. */
      d->stage[s_conv].emit_num = 2u;
      d->stage[s_conv].emit_den = 1u;
    }
  return 0;
}
