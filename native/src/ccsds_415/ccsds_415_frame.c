/*
 * ccsds_415_frame.c — one period of the 415.1-B-1 stimulus as a frame
 * description (doppler/ccsds/ccsds_415_frame.h): docs/design/
 * async-dsss-receiver.md §1.4's table as data, handed to the general
 * description, which knows nothing about CCSDS.
 */
#include "doppler/ccsds/ccsds_415_frame.h"

#include <string.h>

/* `v`'s low `n` bits into `out`, one a byte, most significant first: the
   order the header's fields and the start sequence go on the wire. */
static void
put_bits (uint8_t *out, unsigned v, unsigned n)
{
  for (unsigned i = 0; i < n; i++)
    out[i] = (uint8_t)((v >> (n - 1u - i)) & 1u);
}

/* One literal field over `n` bits of `bits`, `reps` times. */
static int
add_literal (wfm_frame_desc_t *d, const char *name, const uint8_t *bits,
             size_t n, size_t reps)
{
  wfm_seq_t s;
  memset (&s, 0, sizeof s);
  s.kind = WFM_SEQ_LITERAL;
  s.bits = bits;
  s.len  = n;
  return dp_wfm_frame_add_field (d, name, &s, reps);
}

/* The gap's refusal sentence names this number; a change to the field
   bound must change the sentence with it. */
_Static_assert (WFM_FIELD_MAX_BITS == 261120,
                "the gap refusal names WFM_FIELD_MAX_BITS as 261,120");

int
dp_ccsds_415_frame_desc (dp_ccsds_415_frame_t           *f,
                         const dp_ccsds_415_frame_cfg_t *cfg,
                         size_t data_octets, const char **why)
{
  static const dp_ccsds_415_frame_cfg_t defaults
      = DP_CCSDS_415_FRAME_CFG_DEFAULTS;
  const dp_ccsds_415_frame_cfg_t *c  = cfg ? cfg : &defaults;
  const char                     *no = NULL;
  if (!f)
    no = "the frame struct is NULL";
  else if (data_octets < DP_CCSDS_415_DATA_MIN
           || data_octets > DP_CCSDS_415_DATA_MAX)
    no = "the data field holds 1 to 1,017 octets (232.0-B-4 §4.1.3.1.2, "
         "with the FECF present)";
  else if (c->scid > 1023u)
    no = "scid is 10 bits: 0 to 1,023";
  else if (c->vcid > 63u)
    no = "vcid is 6 bits: 0 to 63";
  else if (c->seq > 255u)
    no = "seq is 8 bits: 0 to 255";
  else if (c->gap_sym == 0)
    no = "the gap holds at least one symbol";
  /* The gap is one zero bit repeated gap_sym times, a field of gap_sym
     bits, so it is bounded as a field is: a longer run is a stream. */
  else if (c->gap_sym > WFM_FIELD_MAX_BITS)
    no = "the gap holds at most 261,120 symbols (WFM_FIELD_MAX_BITS, the "
         "most bits one field carries)";
  if (no)
    {
      if (why)
        *why = no;
      return -1;
    }

  memset (f, 0, sizeof *f);
  f->zero = 0;
  put_bits (f->start, DP_CCSDS_415_START_SEQ, DP_CCSDS_415_START_BITS);
  /* The primary header, 232.0-B-4 §4.1.2, in transmission order. */
  uint8_t *h = f->header;
  put_bits (h + 0, 0u, 2);       /* version 00   */
  put_bits (h + 2, 1u, 1);       /* bypass, B    */
  put_bits (h + 3, 0u, 1);       /* ctrl_cmd, D  */
  put_bits (h + 4, 0u, 2);       /* spare 00     */
  put_bits (h + 6, c->scid, 10); /* scid         */
  put_bits (h + 16, c->vcid, 6); /* vcid         */
  put_bits (h + 22, (unsigned)(5u + data_octets + 2u - 1u), 10); /* len */
  put_bits (h + 32, c->seq, 8); /* seq          */

  wfm_frame_desc_t *d = &f->desc;
  wfm_seq_t         acq, data;
  memset (&acq, 0, sizeof acq);
  acq.kind = WFM_SEQ_DOTTED;
  acq.len  = DP_CCSDS_415_ACQ_BITS;
  memset (&data, 0, sizeof data);
  data.kind = WFM_SEQ_DATA;
  data.len  = 8u * data_octets;
  const int ok
      = add_literal (d, "gap", &f->zero, 1, c->gap_sym) >= 0
        && dp_wfm_frame_add_field (d, "acq", &acq, 1) >= 0
        && add_literal (d, "start", f->start, DP_CCSDS_415_START_BITS, 1) >= 0
        && add_literal (d, "version", h + 0, 2, 1) >= 0
        && add_literal (d, "bypass", h + 2, 1, 1) >= 0
        && add_literal (d, "ctrl_cmd", h + 3, 1, 1) >= 0
        && add_literal (d, "spare", h + 4, 2, 1) >= 0
        && add_literal (d, "scid", h + 6, 10, 1) >= 0
        && add_literal (d, "vcid", h + 16, 6, 1) >= 0
        && add_literal (d, "length", h + 22, 10, 1) >= 0
        && add_literal (d, "seq", h + 32, 8, 1) >= 0
        && dp_wfm_frame_add_field (d, "data", &data, 1) >= 0
        && dp_wfm_frame_add_derived (d, "fecf", 16) >= 0
        /* the FECF covers the header and the data field (§4.1.4.1.2) */
        && dp_wfm_frame_add_stage (d, WFM_STAGE_CRC16, "version", "fecf") >= 0;
  if (!ok)
    {
      /* every field fits WFM_FRAME_MAX_FIELDS and every name is new, so
         this is a description the general layer refused for its own
         reason */
      if (why)
        *why = "the frame description refused the 415.1-B-1 layout";
      return -1;
    }
  return 0;
}
