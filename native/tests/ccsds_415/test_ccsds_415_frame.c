/*
 * test_ccsds_415_frame.c -- one period of the 415.1-B-1 stimulus, as a
 * frame description, is §1.4's table bit for bit
 * (doppler/ccsds/ccsds_415_frame.h).
 */
#include "doppler/ccsds/ccsds_415_frame.h"
#include "dp_test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* An independent CRC-16 (232.0-B-4 §4.1.4.2: G = X^16+X^12+X^5+1, preset
   to ones, no inversion), bytewise over packed octets, so the FECF is held
   to the standard and not to the code under test. */
static uint16_t
crc16_octets (const uint8_t *o, size_t n)
{
  uint16_t r = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      r ^= (uint16_t)(o[i] << 8);
      for (int k = 0; k < 8; k++)
        r = (uint16_t)((r & 0x8000u) ? ((unsigned)r << 1) ^ 0x1021u
                                     : ((unsigned)r << 1));
    }
  return r;
}

/* `n` bits at `b` as an unsigned, most significant first. */
static unsigned
bits_value (const uint8_t *b, unsigned n)
{
  unsigned v = 0;
  for (unsigned i = 0; i < n; i++)
    v = (v << 1) | (b[i] & 1u);
  return v;
}

/* A frame at `octets` data octets: its layout is §1.4's, every header field
   reads its value at its place, the start sequence is EB90 read from its
   own bit string, and the FECF is the independent CRC over the header and
   the data. Rejects: the CRC stage covering the data alone (the FECF then
   differs), a length field without the minus one, the start sequence sent
   LSB-first, a field out of order. */
static int
check_frame (size_t octets)
{
  dp_ccsds_415_frame_t     f;
  dp_ccsds_415_frame_cfg_t cfg = DP_CCSDS_415_FRAME_CFG_DEFAULTS;
  const char              *why = NULL;
  DP_REQUIRE (dp_ccsds_415_frame_desc (&f, &cfg, octets, &why) == 0);
  wfm_frame_desc_layout_t lay;
  DP_REQUIRE (dp_wfm_frame_desc_layout (&f.desc, &lay) == 0);
  const size_t g = cfg.gap_sym;
  DP_CHECK (lay.frame_bits == g + 128 + 16 + 40 + 8 * octets + 16);

  /* each named field at its place */
  static const struct
  {
    const char *name;
    size_t      off; /* past the gap */
    size_t      bits;
  } want[] = {
    { "acq", 0, 128 },    { "start", 128, 16 },   { "version", 144, 2 },
    { "bypass", 146, 1 }, { "ctrl_cmd", 147, 1 }, { "spare", 148, 2 },
    { "scid", 150, 10 },  { "vcid", 160, 6 },     { "length", 166, 10 },
    { "seq", 176, 8 },    { "data", 184, 0 },
  };
  for (size_t i = 0; i < sizeof want / sizeof *want; i++)
    {
      const int k = dp_wfm_frame_field_index (&f.desc, want[i].name);
      DP_REQUIRE (k >= 0);
      DP_CHECK (lay.field_off[k] == g + want[i].off);
      DP_CHECK (lay.field_bits[k]
                == (want[i].bits ? want[i].bits : 8 * octets));
    }

  /* assemble with known data bits */
  uint8_t *data = malloc (8 * octets);
  uint8_t *out  = malloc (lay.frame_bits);
  DP_REQUIRE (data && out);
  uint32_t lcg = 0x415u;
  for (size_t i = 0; i < 8 * octets; i++)
    {
      lcg     = lcg * 1664525u + 1013904223u;
      data[i] = (uint8_t)(lcg >> 31);
    }
  DP_REQUIRE (
      dp_wfm_frame_assemble_data (&f.desc, NULL, data, out, lay.frame_bits)
      == lay.frame_bits);
  for (size_t i = 0; i < g; i++)
    DP_CHECK (out[i] == 0);
  for (size_t i = 0; i < 128; i++)
    DP_CHECK (out[g + i] == (uint8_t)((i & 1u) ^ 1u)); /* 1010... */
  static const char eb90[] = "1110101110010000";
  for (size_t i = 0; i < 16; i++)
    DP_CHECK (out[g + 128 + i] == (uint8_t)(eb90[i] - '0'));
  const uint8_t *h = out + g + 144;
  DP_CHECK (bits_value (h + 0, 2) == 0u);
  DP_CHECK (bits_value (h + 2, 1) == 1u);
  DP_CHECK (bits_value (h + 3, 1) == 0u);
  DP_CHECK (bits_value (h + 4, 2) == 0u);
  DP_CHECK (bits_value (h + 6, 10) == 42u);
  DP_CHECK (bits_value (h + 16, 6) == 1u);
  DP_CHECK (bits_value (h + 22, 10) == (unsigned)(5 + octets + 2 - 1));
  DP_CHECK (bits_value (h + 32, 8) == 0u);
  DP_CHECK (memcmp (h + 40, data, 8 * octets) == 0);

  /* the FECF against the independent CRC over header and data octets */
  const size_t n_oct = 5 + octets;
  uint8_t     *octs  = malloc (n_oct);
  DP_REQUIRE (octs);
  for (size_t i = 0; i < n_oct; i++)
    octs[i] = (uint8_t)bits_value (h + 8 * i, 8);
  const unsigned fecf = bits_value (h + 8 * n_oct, 16);
  DP_CHECK (fecf == crc16_octets (octs, n_oct));

  /* the round trip: crc_ok and the description's own check pass, and a
     flipped header bit fails both */
  DP_CHECK (dp_wfm_frame_desc_crc_ok (&f.desc, out) == 1);
  wfm_frame_rx_t rx;
  DP_CHECK (dp_wfm_frame_check (&f.desc, NULL, out, &rx) == 1);
  out[g + 150] ^= 1u; /* a bit of the scid */
  DP_CHECK (dp_wfm_frame_desc_crc_ok (&f.desc, out) == 0);
  DP_CHECK (dp_wfm_frame_check (&f.desc, NULL, out, &rx) == 0);
  free (octs);
  free (out);
  free (data);
  return 0;
}

int
main (void)
{
  /* the independent CRC is the catalogue's CRC-16/IBM-3740: check 0x29B1 */
  DP_CHECK (crc16_octets ((const uint8_t *)"123456789", 9) == 0x29B1u);

  DP_CHECK (check_frame (DP_CCSDS_415_DATA_MIN) == 0);
  DP_CHECK (check_frame (17) == 0);
  DP_CHECK (check_frame (DP_CCSDS_415_DATA_MAX) == 0);

  /* Field 0 is the gap, so the receiver reader that takes its sync word
     from field 0 refuses this description, as the header says. */
  dp_ccsds_415_frame_t f;
  DP_REQUIRE (dp_ccsds_415_frame_desc (&f, NULL, 17, NULL) == 0);
  uint8_t     w[32];
  size_t      n = 0, syms = 0;
  const char *why = NULL;
  DP_CHECK (dp_wfm_frame_desc_rx (&f.desc, w, sizeof w, &n, &syms, &why)
            == -1);
  DP_CHECK (why != NULL);

  /* refusals, each with its sentence */
  dp_ccsds_415_frame_cfg_t c = DP_CCSDS_415_FRAME_CFG_DEFAULTS;
  why                        = NULL;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 0, &why) == -1 && why);
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 1018, &why) == -1 && why);
  c.scid = 1024;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == -1 && why);
  c      = (dp_ccsds_415_frame_cfg_t)DP_CCSDS_415_FRAME_CFG_DEFAULTS;
  c.vcid = 64;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == -1 && why);
  c     = (dp_ccsds_415_frame_cfg_t)DP_CCSDS_415_FRAME_CFG_DEFAULTS;
  c.seq = 256;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == -1 && why);
  c         = (dp_ccsds_415_frame_cfg_t)DP_CCSDS_415_FRAME_CFG_DEFAULTS;
  c.gap_sym = 0;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == -1 && why);
  /* the gap is one field: WFM_FIELD_MAX_BITS symbols build, one more is
     refused with its own sentence, before any field stores the count */
  c.gap_sym = WFM_FIELD_MAX_BITS;
  why       = NULL;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == 0);
  c.gap_sym = (size_t)WFM_FIELD_MAX_BITS + 1u;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == -1 && why
            && strstr (why, "at most 261,120 symbols") != NULL);
  c.gap_sym = SIZE_MAX;
  why       = NULL;
  DP_CHECK (dp_ccsds_415_frame_desc (&f, &c, 17, &why) == -1 && why
            && strstr (why, "at most 261,120 symbols") != NULL);
  DP_TEST_END ("test_ccsds_415_frame");
}
