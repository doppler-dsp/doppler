/*
 * ccsds_frame.c — the CCSDS frame assembler (131.0-B-3 section 9, table 9-1).
 *
 * Four transforms, three different coverages. The whole of this file is the
 * bookkeeping that keeps them apart: fec_frame_layout works out which CADU
 * bits each stage owns, and fec_frame_encode runs each stage over exactly
 * that span. See fec_frame.h for the table and the citations.
 */
#include "fec/fec_frame.h"

/* 4.3.5.1 enumerates the allowed depths. fec_rs_encode_block refuses the
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
fec_frame_layout (const fec_frame_cfg_t *cfg, size_t frame_len,
                  fec_frame_layout_t *out)
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
      if (frame_len != (size_t)FEC_RS_K * cfg->rs_depth)
        return 0;
      block_bytes = (size_t)FEC_RS_N * cfg->rs_depth;
    }

  fec_frame_layout_t lay;
  lay.block_bits = block_bytes * 8u;

  /* 9.4.1: the marker immediately precedes the codeblock, so it occupies the
     front of the CADU and everything else is measured from behind it. */
  lay.marker.first = 0;
  lay.marker.n     = cfg->attach_asm ? (size_t)FEC_CCSDS_ASM_BITS : 0u;
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

  lay.out_bits
      = cfg->convolutional ? fec_conv_max_out (lay.cadu_bits) : lay.cadu_bits;

  if (out != NULL)
    *out = lay;
  return lay.out_bits;
}

size_t
fec_frame_encode (const fec_frame_cfg_t *cfg, conv_enc_t *conv,
                  const uint8_t *frame, size_t frame_len, uint8_t *out,
                  size_t max_out)
{
  fec_frame_layout_t lay;
  const size_t       out_bits = fec_frame_layout (cfg, frame_len, &lay);
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
      uint8_t codeblock[FEC_RS_N * FEC_RS_MAX_DEPTH];
      if (fec_rs_encode_block (frame, cfg->rs_depth, codeblock) == 0)
        return 0;
      unpack (codeblock, (size_t)FEC_RS_N * cfg->rs_depth, block);
    }
  else
    unpack (frame, frame_len, block);

  /* Both of the next two stages are handed a span rather than "the frame",
     and that is the whole mechanism: the randomiser is given the block, the
     inner code is given the CADU. Nothing here depends on the order the two
     lines are written in. */
  if (cfg->randomise)
    fec_ccsds_randomise (block, lay.block_bits);

  if (cfg->attach_asm)
    fec_ccsds_asm_bits (cadu);

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
      conv_encode (s, &FEC_CCSDS_CONV, cadu, lay.cadu_bits, out, out_bits);
    }

  return out_bits;
}
