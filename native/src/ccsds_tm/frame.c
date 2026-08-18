/*
 * frame.c — the CCSDS frame assembler (131.0-B-3 section 9, table 9-1).
 *
 * Four transforms, three different coverages. The whole of this file is the
 * bookkeeping that keeps them apart: ccsds_tm_frame_layout works out which
 * CADU bits each stage owns, and ccsds_tm_frame_encode runs each stage over
 * exactly that span. See ccsds_tm_frame.h for the table and the citations.
 */
#include "ccsds_tm/ccsds_tm_frame.h"

#include <string.h>

/* 4.3.5.1 enumerates the allowed depths. ccsds_tm_rs_encode_block refuses the
 * others too — the check is repeated here because the layout has to be
 * computable without encoding anything, and a caller sizing a buffer from a
 * depth the encoder will later reject should learn that from the sizing
 * call. */
static int
depth_ok (unsigned depth)
{
  return depth == 1 || depth == 2 || depth == 3 || depth == 4 || depth == 5
         || depth == 8;
}

/* Octets to unpacked bits, MSB-first: figure 9-1 puts the first transmitted
 * bit of the ASM at the top of 0x1A, and 4.3.9.2 orders an R-S symbol the
 * same way, so one direction serves the whole file. */
static void
unpack (const uint8_t *bytes, size_t nbytes, uint8_t *bits)
{
  for (size_t i = 0; i < nbytes; i++)
    {
      for (unsigned b = 0; b < 8u; b++)
        bits[i * 8u + b] = (uint8_t)((bytes[i] >> (7u - b)) & 1u);
    }
}

size_t
ccsds_tm_frame_layout (const ccsds_tm_frame_cfg_t *cfg, size_t frame_len,
                       ccsds_tm_frame_layout_t *out)
{
  if (frame_len == 0)
    return 0;

  size_t block_bytes;
  if (cfg->rs_depth == 0)
    block_bytes = frame_len;
  else
    {
      if (!depth_ok (cfg->rs_depth))
        return 0;
      /* 4.4.1: a codeblock is a whole number of codewords. Virtual fill
         (4.4.2's shortened codeblock) is not implemented, so a frame of any
         other length is refused rather than padded to fit — padding would
         produce a codeblock that encodes and decodes perfectly here and is
         the wrong length for the receiver it was aimed at. */
      if (frame_len != (size_t)CCSDS_TM_RS_K * cfg->rs_depth)
        return 0;
      block_bytes = (size_t)CCSDS_TM_RS_N * cfg->rs_depth;
    }

  ccsds_tm_frame_layout_t lay;
  lay.block_bits = block_bytes * 8u;

  /* 9.4.1: the marker immediately precedes the codeblock, so it occupies the
     front of the CADU and everything else is measured from behind it. */
  lay.marker.first = 0;
  lay.marker.n     = cfg->attach_asm ? (size_t)CCSDS_TM_ASM_BITS : 0u;
  lay.cadu_bits    = lay.marker.n + lay.block_bits;

  /* 9.5.1: the ASM "shall NOT be a part of the encoded data space of the
     Reed-Solomon codeblock". The outer code produced the block and nothing
     in front of it. */
  lay.outer.n     = cfg->rs_depth != 0 ? lay.block_bits : 0u;
  lay.outer.first = lay.outer.n != 0 ? lay.marker.n : 0u;

  /* 10.3.2: the sequence starts "with the first bit of the codeblock", and
     10.3.4's first NOTE says the ASM "was not randomized". Same span as the
     outer code, and for a different reason — which is why they are two
     fields rather than one. */
  lay.randomised.n     = cfg->randomise ? lay.block_bits : 0u;
  lay.randomised.first = lay.randomised.n != 0 ? lay.marker.n : 0u;

  /* 3.2.1: the marker "shall always be inserted before performing
     convolutional encoding", and 9.2.1.4 requires it to be encoded. This is
     the one span that starts at bit 0. */
  lay.inner.n     = cfg->convolutional ? lay.cadu_bits : 0u;
  lay.inner.first = 0;

  lay.out_bits = cfg->convolutional ? ccsds_tm_conv_max_out (lay.cadu_bits)
                                    : lay.cadu_bits;

  if (out != NULL)
    *out = lay;
  return lay.out_bits;
}

size_t
ccsds_tm_frame_encode (const ccsds_tm_frame_cfg_t *cfg, conv_enc_t *conv,
                       const uint8_t *frame, size_t frame_len, uint8_t *out,
                       size_t max_out)
{
  ccsds_tm_frame_layout_t lay;
  const size_t out_bits = ccsds_tm_frame_layout (cfg, frame_len, &lay);
  if (out_bits == 0 || max_out < out_bits)
    return 0;

  /* The CADU is built in the TAIL of the caller's buffer and the inner code
     then encodes it forward into the head, which is what lets this run with
     no allocation and no bound on the frame length.

     It is safe because the inner code expands by exactly two: with the CADU
     at `out + cadu_bits`, step i reads out[cadu_bits + i] and writes
     out[2i] and out[2i + 1]. The write can only reach the read when
     2i + 1 >= cadu_bits + i, i.e. i >= cadu_bits - 1 — the final step, where
     conv_encode has already consumed in[i] before writing either symbol.
     With no inner code out_bits == cadu_bits, so `cadu` is `out` itself and
     the question does not arise. */
  uint8_t *const cadu  = out + (out_bits - lay.cadu_bits);
  uint8_t *const block = cadu + lay.marker.n;

  if (cfg->rs_depth != 0)
    {
      uint8_t codeblock[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];
      if (ccsds_tm_rs_encode_block (frame, cfg->rs_depth, codeblock) == 0)
        return 0;
      unpack (codeblock, (size_t)CCSDS_TM_RS_N * cfg->rs_depth, block);
    }
  else
    unpack (frame, frame_len, block);

  /* Both of the next two stages are handed a span rather than "the frame",
     and that is the whole mechanism: the randomiser is given the block, the
     inner code is given the CADU. Nothing here depends on the order the two
     lines are written in. */
  if (cfg->randomise)
    ccsds_tm_randomise (block, lay.block_bits);

  if (cfg->attach_asm)
    ccsds_tm_asm_bits (cadu);

  if (cfg->convolutional)
    {
      /* 3.3.2 fixes the output as one uninterrupted sequence, so the register
         carries from the last bit of one CADU into the first bit of the next.
         Owning it here would restart it every frame -- a discontinuity in the
         first 6 symbols of every frame after the first, landing on the ASM,
         and invisible to a matched decoder. It is the caller's, and a NULL
         says "this frame stands alone" rather than "I forgot". */
      conv_enc_t  own;
      conv_enc_t *s = conv;
      if (s == NULL)
        {
          conv_enc_init (&own);
          s = &own;
        }
      /* The capacity is the WHOLE buffer: the encode reads the CADU from the
         tail and writes the expanded stream from out[0]. */
      conv_encode (s, &CCSDS_TM_CONV, cadu, lay.cadu_bits, out, out_bits);
    }

  return out_bits;
}

/* ── the receive direction ─────────────────────────────────────────────────
 */

/* Bits to octets MSB-first, optionally XORing the randomising sequence on the
 * way through. The inverse of `unpack`, and the same sentence in figure 9-1
 * and 4.3.9.2 that justifies that one.
 *
 * Derandomising HERE rather than in a separate pass is what keeps this
 * O(1) in scratch memory for a frame of any length: ccsds_tm_randomise wants
 * a mutable bit run, and the CADU belongs to the caller.
 *
 * The generator is STEPPED alongside the pack rather than pre-computed into a
 * table. It used to be a 255-entry table indexed by `k % 255`, which was the
 * same sequence and free; at 10.4.1's period that table is 128 KB and is
 * LONGER THAN ANY CADU, so it would never wrap and every byte of it would be
 * a byte of the sequence held for no reason. One word of state serves both
 * randomisers and any frame length. */
static void
pack_derand (const uint8_t *bits, size_t nbytes, ccsds_tm_rand_state_t *pn,
             uint8_t *bytes)
{
  for (size_t i = 0; i < nbytes; i++)
    {
      uint8_t v = 0;
      for (unsigned b = 0; b < 8u; b++)
        {
          unsigned x = bits[i * 8u + b] & 1u;
          if (pn != NULL)
            x ^= ccsds_tm_rand_step (pn);
          v = (uint8_t)((v << 1) | x);
        }
      bytes[i] = v;
    }
}

size_t
ccsds_tm_frame_decode (const ccsds_tm_frame_cfg_t *cfg, const uint8_t *cadu,
                       size_t n_cadu, uint8_t *frame, size_t max_frame,
                       ccsds_tm_frame_rx_t *rx)
{
  const size_t marker_bits = cfg->attach_asm ? (size_t)CCSDS_TM_ASM_BITS : 0u;
  if (n_cadu <= marker_bits || (n_cadu - marker_bits) % 8u != 0u)
    return 0;

  /* Work back to the Transfer Frame length the CADU implies, then let
     ccsds_tm_frame_layout confirm it. Deriving the shape twice -- once
     forwards in the encoder and once backwards here -- is how the two
     directions come to disagree about a span, so the backward derivation
     produces only a frame_len and the FORWARD function remains the single
     description. */
  const size_t block_bytes = (n_cadu - marker_bits) / 8u;
  size_t       frame_len;
  if (cfg->rs_depth != 0)
    {
      if (block_bytes != (size_t)CCSDS_TM_RS_N * cfg->rs_depth)
        return 0;
      frame_len = (size_t)CCSDS_TM_RS_K * cfg->rs_depth;
    }
  else
    frame_len = block_bytes;

  ccsds_tm_frame_layout_t lay;
  if (ccsds_tm_frame_layout (cfg, frame_len, &lay) == 0
      || lay.cadu_bits != n_cadu || max_frame < frame_len)
    return 0;

  ccsds_tm_rand_state_t  rand_state;
  ccsds_tm_rand_state_t *pn = NULL;
  if (cfg->randomise)
    {
      ccsds_tm_rand_init (&rand_state, NULL);
      pn = &rand_state;
    }

  /* 10.3.2 starts the sequence at the first bit of the CODEBLOCK, and 10.3.4
     note 1 says the marker was never randomised -- so the span begins behind
     the marker, exactly as lay.randomised.first says it did on the way out. */
  const uint8_t *block = cadu + lay.marker.n;

  ccsds_tm_frame_rx_t out = { frame_len, 0u, 0u, 0u, 0u };

  if (cfg->rs_depth == 0)
    {
      pack_derand (block, frame_len, pn, frame);
      if (rx != NULL)
        *rx = out;
      return frame_len;
    }

  uint8_t codeblock[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];
  pack_derand (block, block_bytes, pn, codeblock);

  /* The outer code CORRECTS here, in place, before anything reads the
     information section -- so the copy below is of the repaired block. The
     de-interleave belongs to ccsds_tm_rs_decode_block rather than to this
     function, because it is the same S1/S2 rotation the encoder wrote and
     one description of it is the point. */
  ccsds_tm_rs_block_rx_t rs;
  ccsds_tm_rs_decode_block (codeblock, cfg->rs_depth, &rs);

  /* 4.4.1: S2 reassembles the information symbols "in the same way as they
     entered", so the Transfer Frame is the information section verbatim and
     only the CHECK symbols were rotated. */
  memcpy (frame, codeblock, frame_len);

  out.rs_codewords = rs.codewords;
  out.rs_ok        = rs.codewords - rs.uncorrectable;
  out.rs_corrected = rs.corrected;
  out.rs_symbols   = rs.symbols;

  if (rx != NULL)
    *rx = out;
  return frame_len;
}
