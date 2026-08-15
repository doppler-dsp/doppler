/*
 * wfm_frame.c — the frame descriptor's geometry and materialisation. The
 * contract, and the reasoning behind each boundary, live on the declarations
 * in wfm/wfm_frame.h.
 */
#include "wfm/wfm_frame.h"

#include "dp_crc16.h"
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

int
wfm_frame_layout (const wfm_frame_t *f, wfm_frame_layout_t *out)
{
  if (!f || !out)
    return -1;
  memset (out, 0, sizeof *out);

  size_t off         = 0;
  out->preamble_bits = (f->preamble.len && f->preamble_reps)
                           ? f->preamble.len * f->preamble_reps
                           : 0;
  out->preamble_off  = off;
  off += out->preamble_bits;

  out->sync_bits = f->sync.len;
  out->sync_off  = off;
  off += out->sync_bits;

  out->payload_bits = f->payload.len;
  out->payload_off  = off;
  off += out->payload_bits;

  /* The CRC protects the payload, so with no payload there is nothing to
     protect and it is dropped rather than emitting crc16 of nothing. Same
     rule wfm_frame_dsss_nchips() has always applied. */
  out->crc_bits
      = (f->crc && out->payload_bits) ? WFM_FRAME_CRC_BITS : (size_t)0;
  out->crc_off = off;
  off += out->crc_bits;

  out->total_bits = off;
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

size_t
wfm_frame_bits (const wfm_frame_t *f, uint8_t *out, size_t max_out)
{
  wfm_frame_layout_t l;
  if (!out || wfm_frame_layout (f, &l) != 0)
    return 0;
  if (l.total_bits == 0 || l.total_bits > max_out)
    return 0;

  if (l.preamble_bits)
    {
      /* One period, then repeated verbatim — a generated preamble must repeat
         the SAME bits, not draw fresh ones, or it is not a periodic
         acquisition target and coherent integration across reps is void. */
      size_t n
          = seq_bits (&f->preamble, out + l.preamble_off, f->preamble.len);
      if (n != f->preamble.len)
        return 0;
      for (size_t r = 1; r < f->preamble_reps; r++)
        memcpy (out + l.preamble_off + r * f->preamble.len,
                out + l.preamble_off, f->preamble.len);
    }
  if (l.sync_bits
      && seq_bits (&f->sync, out + l.sync_off, l.sync_bits) != l.sync_bits)
    return 0;
  if (l.payload_bits
      && seq_bits (&f->payload, out + l.payload_off, l.payload_bits)
             != l.payload_bits)
    return 0;
  if (l.crc_bits)
    {
      uint16_t c = dp_crc16_ccitt (out + l.payload_off, l.payload_bits);
      for (size_t i = 0; i < l.crc_bits; i++)
        out[l.crc_off + i] = (uint8_t)((c >> (15 - i)) & 1u); /* MSB-first */
    }
  return l.total_bits;
}

int
wfm_frame_crc_ok (const wfm_frame_t *f, const uint8_t *rx_bits)
{
  wfm_frame_layout_t l;
  if (!rx_bits || wfm_frame_layout (f, &l) != 0 || l.crc_bits == 0)
    return -1;
  uint16_t want = dp_crc16_ccitt (rx_bits + l.payload_off, l.payload_bits);
  uint16_t got  = 0;
  for (size_t i = 0; i < l.crc_bits; i++)
    got = (uint16_t)((got << 1) | (rx_bits[l.crc_off + i] & 1u));
  return want == got;
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
wfm_frame_dsss_nchips (size_t acq_len, size_t acq_reps, size_t data_len,
                       size_t sync_len, size_t payload_len, int crc)
{
  /* The layout is wfm_frame's, so this is the preamble (unspread) plus the
     spread group. Expressing it here a second time is what the frame
     descriptor exists to stop. */
  wfm_frame_t f  = { 0 };
  f.sync.kind    = WFM_SEQ_LITERAL;
  f.sync.len     = sync_len;
  f.payload.kind = WFM_SEQ_LITERAL;
  f.payload.len  = payload_len;
  f.crc          = crc;

  size_t pre   = acq_len * acq_reps;
  size_t nbits = wfm_frame_nbits (&f);
  if (nbits && data_len == 0)
    return 0;                    /* frame bits with no spreading code */
  return pre + nbits * data_len; /* 0 when there is nothing to transmit */
}

size_t
wfm_frame_dsss_chips (const uint8_t *acq_code, size_t acq_len, size_t acq_reps,
                      const uint8_t *data_code, size_t data_len,
                      const uint8_t *sync, size_t sync_len,
                      const uint8_t *payload, size_t payload_len, int crc,
                      uint8_t *out)
{
  size_t total = wfm_frame_dsss_nchips (acq_len, acq_reps, data_len, sync_len,
                                        payload_len, crc);
  if (total == 0)
    return 0;

  /* The frame's BITS come from wfm_frame_bits(); this function's remaining job
     is the DSSS-specific part -- prepend the unmodulated repeated preamble,
     and XOR-spread each frame bit by the data code. That split is the point of
     the descriptor: the layout (and the CRC's position, width and bit order)
     is stated once, and DSSS becomes "assemble the frame, then spread it". */
  wfm_frame_t f  = { 0 };
  f.sync.kind    = WFM_SEQ_LITERAL;
  f.sync.bits    = sync;
  f.sync.len     = sync_len;
  f.payload.kind = WFM_SEQ_LITERAL;
  f.payload.bits = payload;
  f.payload.len  = payload_len;
  f.crc          = crc;

  wfm_frame_layout_t l;
  wfm_frame_layout (&f, &l);
  uint8_t *bits = (l.total_bits > 0) ? malloc (l.total_bits) : NULL;
  if (l.total_bits > 0
      && (!bits || wfm_frame_bits (&f, bits, l.total_bits) != l.total_bits))
    {
      free (bits);
      return 0;
    }

  size_t w = 0;
  /* Unmodulated repeated preamble: the acquisition code, verbatim. */
  for (size_t r = 0; r < acq_reps; r++)
    for (size_t i = 0; i < acq_len; i++)
      out[w++] = acq_code[i] & 1u;
  /* Every frame bit spread by the data code: a 0 bit transmits the code as-is,
     a 1 bit transmits it inverted. */
  for (size_t i = 0; i < l.total_bits; i++)
    for (size_t j = 0; j < data_len; j++)
      out[w++] = (uint8_t)(bits[i] ^ (data_code[j] & 1u));

  free (bits);
  return w;
}
