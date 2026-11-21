/**
 * @file test_wfm_ofdm_bridge.c
 * @brief The wfmgen `ofdm` type through the bridge: a source with a data
 * source renders the PPDUs `wlan` builds from the same octets
 * (docs/design/wlan.md, section 5).
 */
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h"
#include "doppler/wfm/wfm_frame.h"
#include "doppler/wfm/wfm_names.h"
#include "doppler/wfm_compose/wfm_compose_bridge.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "doppler/wlan/wlan_core.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float _Complex cf;

/* 100 octets of a fixed pattern, as 800 literal bits, first bit most
   significant (a file's bytes and a hex Field unpack so): the bits the
   bridge packs back into these octets. */
static uint8_t bits_lit[1600]; /* the 800 bits twice: two frames */
static uint8_t octets[100];
/* a 30-octet MAC header (its first 24 are the 24-octet header's) and its
   bits, the first bit of each eight the most significant */
static uint8_t hdr_oct[30];
static uint8_t hdr_bits[240];

static void
make_data (void)
{
  for (size_t k = 0; k < 100; k++)
    {
      octets[k] = (uint8_t)(k * 7u + 3u);
      for (unsigned j = 0; j < 8u; j++)
        bits_lit[8u * k + j] = bits_lit[800u + 8u * k + j]
            = (octets[k] >> (7u - j)) & 1u;
    }
  for (size_t k = 0; k < 30; k++)
    {
      hdr_oct[k] = (uint8_t)(0x08u + k * 11u);
      for (unsigned j = 0; j < 8u; j++)
        hdr_bits[8u * k + j] = (hdr_oct[k] >> (7u - j)) & 1u;
    }
}

static wfm_source_t
source (void)
{
  wfm_source_t s = WFM_SOURCE_DEFAULTS;
  s.type         = WFM_SYNTH_FRAMES;
  s.snr          = 100.0;
  s.data
      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 1600, .bits = bits_lit };
  s.data_len       = 800;
  s.wlan           = 36;
  s.scrambler_init = 93;
  /* The rectangular window, asked for: the pins below are at window 0. A
     source that keeps the default has its own test (the default window). */
  s.window = 0;
  return s;
}

/* Render `n` samples of a source at fs. */
static cf *
render (const wfm_source_t *s, double fs, size_t n)
{
  dp_wfm_synth_state_t *syn = dp_wfm_source_to_synth (s, fs);
  if (!syn)
    return NULL;
  cf *x = malloc (n * sizeof *x);
  if (x)
    dp_wfm_synth_steps (syn, x, n);
  dp_wfm_synth_destroy (syn);
  return x;
}

/* Relative l2 distance of two renders, and the energy that makes it mean
   something (a silent render would give 0 against 0). */
static double
rel_l2 (const cf *a, const cf *b, size_t n, double *energy)
{
  double num = 0, den = 0;
  for (size_t i = 0; i < n; i++)
    {
      const double d = cabs ((double _Complex)a[i] - b[i]);
      num += d * d;
      den += cabs (b[i]) * cabs (b[i]);
    }
  *energy = den;
  return den > 0.0 ? sqrt (num / den) : 1.0;
}

/* The PPDU wlan builds from the same bits, by the test's own calls: the
   octets, their FCS if `mac` asks for it, the rate pair, M and the window.
   n_oct octets of `octets`. */
static cf *
reference (const dp_wlan_rate_t *rate, dp_wlan_mac_t mac, size_t n_oct,
           size_t hdr_len, unsigned scr, unsigned window, unsigned m,
           size_t *len)
{
  uint8_t psdu[WLAN_PSDU_MAX];
  size_t  n
      = dp_wlan_mpdu (mac, hdr_oct, hdr_len, octets, n_oct, psdu, sizeof psdu);
  if (!n)
    return NULL;
  *len  = dp_wlan_ppdu_len (dp_wlan_rate_n_sym (rate, n), window, m);
  cf *x = malloc (*len * sizeof *x);
  if (x
      && dp_wlan_ppdu (dp_wlan_tables_standard (), rate, psdu, n, scr, window,
                       m, x, *len)
             != *len)
    {
      free (x);
      return NULL;
    }
  return x;
}

/* The render is the PPDU wlan builds from the same octets. */
static int
check_render (const wfm_source_t *s, double fs, const dp_wlan_rate_t *rate,
              dp_wlan_mac_t mac, unsigned window, unsigned m)
{
  size_t len  = 0;
  cf    *want = reference (rate, mac, s->data_len / 8u, s->mac_header.len / 8u,
                           s->scrambler_init, window, m, &len);
  cf    *got  = want ? render (s, fs, len) : NULL;
  DP_REQUIRE (want && got);
  double e;
  DP_CHECK (rel_l2 (got, want, len, &e) < 1e-6 && e > 0.0);
  free (got);
  free (want);
  return 0;
}

static int
test_renders_the_ppdu_wlan_builds (void)
{
  const dp_wlan_rate_t r36 = { 4u, WLAN_CODE_3_4 };
  wfm_source_t         s   = source ();
  const char          *why = dp_wfm_source_to_synth_error (&s, 20e6);
  if (why)
    DP_TEST_ERR ("refused: %s", why);
  DP_REQUIRE (why == NULL);
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_NONE, 0u, 1u) == 0);
  /* the FCS: the source's bits are the MPDU's body, and the PHY appends the
     FCS, so the same bits render the PPDU of body + FCS, which is not the
     PPDU of the body alone */
  s.mac = WLAN_MAC_FCS;
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_FCS, 0u, 1u) == 0);
  /* M = 2 and a window, against wlan at M = 2 */
  s.mac    = WLAN_MAC_NONE;
  s.window = 4;
  DP_CHECK (check_render (&s, 40e6, &r36, WLAN_MAC_NONE, 4u, 2u) == 0);
  /* 5 MHz: bandwidth 5e6 at fs 20e6 is M = 4, the pair named, not the rate */
  s.window        = 0;
  s.wlan          = 0;
  s.bandwidth     = 5e6;
  s.constellation = 2;
  s.code_rate     = WLAN_CODE_3_4;
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_NONE, 0u, 4u) == 0);
  /* 10 MHz at its own clock is M = 1: the 20 MHz samples */
  s.bandwidth = 10e6;
  DP_CHECK (check_render (&s, 10e6, &r36, WLAN_MAC_NONE, 0u, 1u) == 0);
  return 0;
}

/* mac data: the PSDU is the header, the frame's octets and the FCS, with a
   24-octet header, a 30-octet one, and a header that is a generated Field. */
static int
test_mac_data_renders_header_body_and_fcs (void)
{
  const dp_wlan_rate_t r36 = { 4u, WLAN_CODE_3_4 };
  wfm_source_t         s   = source ();
  s.mac                    = WLAN_MAC_DATA;
  s.mac_header             = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL,
                                          .len  = 8u * 24u,
                                          .bits = hdr_bits };
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_DATA, 0u, 1u) == 0);
  s.mac_header.len = 8u * 30u;
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_DATA, 0u, 1u) == 0);
  /* control: the header is in the PSDU (the Data field, from sample 400),
     so another header is another PPDU */
  wfm_source_t o = s;
  uint8_t      flip[240];
  memcpy (flip, hdr_bits, sizeof flip);
  flip[0] ^= 1u;
  o.mac_header.bits = flip;
  cf *x             = render (&s, 20e6, 700);
  cf *y             = render (&o, 20e6, 700);
  DP_REQUIRE (x && y);
  DP_CHECK (memcmp (x, y, 700 * sizeof *x) != 0);
  free (x);
  free (y);
  /* a generated Field (a PN sequence) is its bits, packed the same way */
  wfm_source_t g = source ();
  g.mac          = WLAN_MAC_DATA;
  g.mac_header   = (wfm_seq_t){
    .kind = WFM_SEQ_PN, .len = 8u * 24u, .reg_bits = 15, .seed = 1
  };
  DP_CHECK (dp_wfm_source_to_synth_error (&g, 20e6) == NULL);
  uint8_t gb[240];
  DP_REQUIRE (dp_wfm_seq_bits (&g.mac_header, gb, 8u * 24u) == 8u * 24u);
  for (size_t k = 0; k < 24; k++)
    {
      uint8_t b = 0;
      for (unsigned j = 0; j < 8u; j++)
        b = (uint8_t)(b | (gb[8u * k + j] << (7u - j)));
      hdr_oct[k] = b;
    }
  DP_CHECK (check_render (&g, 20e6, &r36, WLAN_MAC_DATA, 0u, 1u) == 0);
  return 0;
}

/* The names are held to the values they index: reordering a table in
   wfm_names.h turns this red, which no compiler would. */
static int
test_each_name_is_its_value (void)
{
  DP_CHECK (strcmp (OFDM_CODE_RATE_NAMES[WLAN_CODE_1_2], "1/2") == 0);
  DP_CHECK (strcmp (OFDM_CODE_RATE_NAMES[WLAN_CODE_2_3], "2/3") == 0);
  DP_CHECK (strcmp (OFDM_CODE_RATE_NAMES[WLAN_CODE_3_4], "3/4") == 0);
  DP_CHECK (strcmp (OFDM_CODE_RATE_NAMES[WLAN_CODE_5_6], "5/6") == 0);
  DP_CHECK (strcmp (OFDM_MAC_NAMES[WLAN_MAC_NONE], "none") == 0);
  DP_CHECK (strcmp (OFDM_MAC_NAMES[WLAN_MAC_FCS], "fcs") == 0);
  DP_CHECK (strcmp (OFDM_MAC_NAMES[WLAN_MAC_DATA], "data") == 0);
  static const char *const c[4] = { "bpsk", "qpsk", "qam16", "qam64" };
  static const unsigned    b[4] = { 1, 2, 4, 6 };
  for (int k = 0; k < 4; k++)
    {
      DP_CHECK (strcmp (OFDM_CONSTELLATION_NAMES[k], c[k]) == 0);
      DP_CHECK (dp_wlan_n_bpsc (k) == b[k]);
    }
  /* the table offers only what wlan builds */
  DP_CHECK (sizeof OFDM_CONSTELLATION_NAMES / sizeof *OFDM_CONSTELLATION_NAMES
            == 4);
  DP_CHECK (dp_wlan_n_bpsc (4) == 0 && dp_wlan_n_bpsc (-1) == 0);
  return 0;
}

/* wlan N is a spelling, not a second code path: for each of the eight
   rates the source spelled with it renders the bytes of the source spelled
   with the pair Table 17-6 names for N. The expansion is held, not the
   content (that is test_wlan_core.c). */
static int
test_wlan_n_is_its_pair_for_all_eight (void)
{
  static const unsigned   rates[8] = { 6, 9, 12, 18, 24, 36, 48, 54 };
  const dp_wlan_tables_t *t        = dp_wlan_tables_standard ();
  for (unsigned i = 0; i < 8; i++)
    {
      wfm_source_t a = source ();
      a.wlan         = (int)rates[i];
      dp_wlan_rate_t pair;
      DP_REQUIRE (dp_wlan_rate_from_mbps (t, rates[i], &pair));
      wfm_source_t b = source ();
      b.wlan         = 0;
      for (int k = 0; k < 4; k++)
        if (dp_wlan_n_bpsc (k) == pair.n_bpsc)
          b.constellation = k;
      b.code_rate    = (int)pair.code;
      const size_t n = 600; /* the SIGNAL and Data symbols are in it */
      cf          *x = render (&a, 20e6, n);
      cf          *y = render (&b, 20e6, n);
      DP_REQUIRE (x && y);
      DP_CHECK (memcmp (x, y, n * sizeof *x) == 0);
      /* control: a different rate is not the same bytes */
      wfm_source_t c = a;
      c.wlan         = (int)rates[(i + 1) % 8];
      cf *z          = render (&c, 20e6, n);
      DP_REQUIRE (z);
      DP_CHECK (memcmp (x, z, n * sizeof *x) != 0);
      free (x);
      free (y);
      free (z);
    }
  return 0;
}

typedef struct
{
  const char *name;
  void (*edit) (wfm_source_t *);
  const char *phrase; /* the sentence's own words for the field */
} refusal_t;

static void
r_wlan7 (wfm_source_t *s)
{
  s->wlan = 7;
}
static void
r_wlanneg (wfm_source_t *s)
{
  s->wlan = -36;
}
static void
r_both (wfm_source_t *s)
{
  s->constellation = 2;
}
static void
r_bw (wfm_source_t *s)
{
  s->bandwidth = 10e6;
}
static void
r_bwneg (wfm_source_t *s)
{
  s->wlan      = 0;
  s->bandwidth = -20e6;
}
static void
r_bwodd (wfm_source_t *s)
{
  s->wlan      = 0;
  s->bandwidth = 15e6;
}
static void
r_esno (wfm_source_t *s)
{
  s->snr_mode = 3;
}
static void
r_ebno (wfm_source_t *s)
{
  s->snr_mode = 2;
}
static void
r_macdata (wfm_source_t *s)
{
  s->mac = WLAN_MAC_DATA;
}
static void
set_hdr (wfm_source_t *s, size_t bits)
{
  s->mac_header
      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = bits, .bits = hdr_bits };
}
static void
r_hdrshort (wfm_source_t *s)
{
  s->mac = WLAN_MAC_DATA;
  set_hdr (s, 8u * 20u);
}
static void
r_hdrodd (wfm_source_t *s)
{
  s->mac = WLAN_MAC_DATA;
  set_hdr (s, 197u);
}
static void
r_hdrnone (wfm_source_t *s)
{
  set_hdr (s, 8u * 24u);
}
static void
r_hdrfcs (wfm_source_t *s)
{
  s->mac = WLAN_MAC_FCS;
  set_hdr (s, 8u * 24u);
}
static void
r_hdrdata (wfm_source_t *s)
{
  s->mac        = WLAN_MAC_DATA;
  s->mac_header = (wfm_seq_t){ .kind = WFM_SEQ_DATA, .len = 8u * 24u };
}
static void
r_hdrpn (wfm_source_t *s)
{
  s->mac = WLAN_MAC_DATA;
  s->mac_header
      = (wfm_seq_t){ .kind = WFM_SEQ_PN, .len = 8u * 24u, .reg_bits = 99 };
}
static void
r_macbad (wfm_source_t *s)
{
  s->mac = 7;
}
static void
r_len (wfm_source_t *s)
{
  s->data_len = 12;
}
static void
r_scr (wfm_source_t *s)
{
  s->scrambler_init = 128;
}
static void
r_scrneg (wfm_source_t *s)
{
  s->scrambler_init = -1;
}
static void
r_sps (wfm_source_t *s)
{
  s->sps = 4;
}
static void
r_pulse (wfm_source_t *s)
{
  s->pulse = 1;
}
static void
r_nodata (wfm_source_t *s)
{
  s->data = (wfm_seq_t){ 0 };
}
static void
r_qam256 (wfm_source_t *s)
{
  s->wlan          = 0;
  s->constellation = 4;
}
static void
r_rate4 (wfm_source_t *s)
{
  s->wlan      = 0;
  s->code_rate = 4;
}
static void
r_norow (wfm_source_t *s)
{
  s->wlan          = 0; /* qam16 1/2 is 24 Mb/s; bpsk 2/3 has no row */
  s->constellation = 0;
  s->code_rate     = WLAN_CODE_2_3;
}
static void
r_psdubig (wfm_source_t *s)
{
  s->data_len = 8u * 4096u;
}
static void
r_psdufcs (wfm_source_t *s)
{
  s->mac      = WLAN_MAC_FCS; /* 4092 + 4 = 4096 octets */
  s->data_len = 8u * 4092u;
}
static void
r_win (wfm_source_t *s)
{
  s->window = 81;
}
static void
r_winwrap (wfm_source_t *s)
{
  s->window = (size_t)4294967300ull;
}
static void
r_gapwrap (wfm_source_t *s)
{
  s->gap = (size_t)-1 - 800u;
}
static void
r_gapcap (wfm_source_t *s)
{
  s->gap = WFM_SYNTH_FRAME_MAX;
}

static int
test_each_refusal_names_its_field (void)
{
  static const refusal_t t[] = {
    { "wlan not one of the eight", r_wlan7, "wlan is the 20 MHz data rate" },
    { "wlan negative", r_wlanneg, "wlan is the 20 MHz data rate" },
    { "wlan with constellation", r_both, "wlan is a spelling" },
    { "wlan with another bandwidth", r_bw, "wlan is the data rate at 20 MHz" },
    { "bandwidth negative", r_bwneg, "bandwidth is 20e6" },
    { "bandwidth not an 802.11 one", r_bwodd, "bandwidth is 20e6" },
    { "esno", r_esno, "snr_mode ebno and esno" },
    { "ebno", r_ebno, "snr_mode ebno and esno" },
    { "mac data without a header", r_macdata, "mac data needs a mac_header" },
    { "a 20-octet header", r_hdrshort, "mac_header is a data frame's header" },
    { "a header of 197 bits", r_hdrodd, "mac_header is whole octets" },
    { "a header with mac none", r_hdrnone, "mac_header is the MAC header" },
    { "a header with mac fcs", r_hdrfcs, "mac_header is the MAC header" },
    { "a header that is data:LEN", r_hdrdata, "data:LEN has no bits" },
    { "a header Field that does not build", r_hdrpn, "does not build" },
    { "mac outside its three", r_macbad, "mac is one of" },
    { "data_len not whole octets", r_len, "data_len must be a multiple of 8" },
    { "scrambler_init above 127", r_scr, "scrambler_init is the 7-bit" },
    { "scrambler_init negative", r_scrneg, "scrambler_init is the 7-bit" },
    { "sps on a finished waveform", r_sps, "ofdm renders finished samples" },
    { "pulse on a finished waveform", r_pulse, "ofdm renders finished" },
    { "no data source", r_nodata, "an ofdm source sends a data source" },
    { "an index past 64-QAM", r_qam256, "constellation is one of" },
    { "a code_rate index past 5/6", r_rate4, "code_rate is one of" },
    { "a pair with no SIGNAL RATE", r_norow, "no SIGNAL RATE bits" },
    { "a PSDU of 4096 octets", r_psdubig, "PSDU is 1 to 4095" },
    { "a PSDU of 4096 octets with its FCS", r_psdufcs, "PSDU is 1 to 4095" },
    { "window beyond 80 M", r_win, "window is 0 to 80 times" },
    { "window past 2^32", r_winwrap, "window is 0 to 80 times" },
    { "gap that wraps the sum", r_gapwrap, "at most 2^24 samples" },
    { "gap one past the cap", r_gapcap, "at most 2^24 samples" },
  };
  for (size_t k = 0; k < sizeof t / sizeof *t; k++)
    {
      wfm_source_t s = source ();
      t[k].edit (&s);
      const char *why = dp_wfm_source_to_synth_error (&s, 20e6);
      if (!why || !strstr (why, t[k].phrase))
        DP_TEST_ERR ("%s: got \"%s\"", t[k].name, why ? why : "(accepted)");
      DP_CHECK (why && strstr (why, t[k].phrase));
      /* the bridge refuses what the validator refuses, and a refused
         geometry allocates nothing (the wrapped gap would have) */
      DP_CHECK (dp_wfm_source_to_synth (&s, 20e6) == NULL);
    }
  /* a bandwidth that fs is not an integer multiple of, and one that is more
     than 128 times it */
  wfm_source_t s  = source ();
  s.wlan          = 0;
  s.constellation = 1;
  const char *why = dp_wfm_source_to_synth_error (&s, 30e6);
  DP_CHECK (why && strstr (why, "fs must be an integer multiple"));
  why = dp_wfm_source_to_synth_error (&s, 20e6 * 129.0);
  DP_CHECK (why && strstr (why, "at most 128"));
  /* the cap's edge is accepted: the gap that makes the frame exactly 2^24
     samples (decided by the validator, without allocating 128 MB) */
  s                        = source ();
  const dp_wlan_rate_t r36 = { 4u, WLAN_CODE_3_4 };
  const size_t         ppdu
      = dp_wlan_ppdu_len (dp_wlan_rate_n_sym (&r36, 100), 0u, 1u);
  s.gap = WFM_SYNTH_FRAME_MAX - ppdu;
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  s.gap++;
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) != NULL);
  /* the fields of an ofdm source on another type, each by itself */
  for (int k = 0; k < 7; k++)
    {
      s      = source ();
      s.type = WFM_SYNTH_BPSK;
      s.wlan = 0;
      s.data = (wfm_seq_t){ 0 };
      switch (k)
        {
        case 0:
          s.constellation = 1;
          break;
        case 1:
          s.code_rate = 1;
          break;
        case 2:
          s.scrambler_init = 5;
          break;
        case 3:
          s.window = 2;
          break;
        case 4:
          s.mac = WLAN_MAC_FCS;
          break;
        case 6:
          set_hdr (&s, 8u * 24u);
          break;
        default:
          s.gap = 3;
          break;
        }
      why = dp_wfm_source_to_synth_error (&s, 20e6);
      DP_CHECK (why && strstr (why, "are fields of an ofdm source"));
    }
  s      = source ();
  s.type = WFM_SYNTH_BPSK;
  s.data = (wfm_seq_t){ 0 };
  why    = dp_wfm_source_to_synth_error (&s, 20e6);
  DP_CHECK (why && strstr (why, "wlan is the 802.11 data rate of an ofdm"));
  s.wlan      = 0;
  s.bandwidth = 10e6;
  why         = dp_wfm_source_to_synth_error (&s, 20e6);
  DP_CHECK (why && strstr (why, "are fields of an ofdm source"));
  /* controls: the source the table edits is itself accepted, at M = 1 and 2,
     and a C source that skipped the defaults (zero bandwidth) is the 20 MHz */
  s = source ();
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 40e6) == NULL);
  s.bandwidth = 0.0;
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  return 0;
}

/* The accepting side of each ranged rule, at its edge: the refusal table
   above holds one past it, and only the pair says where the edge is. */
static int
test_each_range_accepts_its_edge (void)
{
  /* window is within 80 M samples, at an M above 1 so the M is in it: at
     fs 40 MHz (M = 2) 160 is accepted and 161 refused. A cap of 80 alone
     would refuse 160; a cap with no limit would accept 161. */
  wfm_source_t s = source ();
  s.window       = 160;
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 40e6) == NULL);
  dp_wfm_synth_state_t *syn = dp_wfm_source_to_synth (&s, 40e6);
  DP_CHECK (syn != NULL);
  dp_wfm_synth_destroy (syn);
  s.window        = 161;
  const char *why = dp_wfm_source_to_synth_error (&s, 40e6);
  DP_CHECK (why && strstr (why, "window is 0 to 80 times"));
  /* scrambler_init 1 and 127, the 7-bit state's ends, each builds */
  static const int scr[2] = { 1, 127 };
  for (int k = 0; k < 2; k++)
    {
      s                = source ();
      s.scrambler_init = scr[k];
      DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
      syn = dp_wfm_source_to_synth (&s, 20e6);
      DP_CHECK (syn != NULL);
      dp_wfm_synth_destroy (syn);
    }
  /* M = 128 builds and 129 is refused. One octet is one Data symbol at
     36 Mb/s, so the frame is (400 + 80) * 128 = 61440 samples; the longest
     PSDU would be 14 M. */
  s          = source ();
  s.data_len = 8;
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6 * 128.0) == NULL);
  syn = dp_wfm_source_to_synth (&s, 20e6 * 128.0);
  DP_CHECK (syn != NULL);
  dp_wfm_synth_destroy (syn);
  why = dp_wfm_source_to_synth_error (&s, 20e6 * 129.0);
  DP_CHECK (why && strstr (why, "at most 128"));
  return 0;
}

/* dp_wfm_ofdm_fs_error answers only for an ofdm source its rate-free rules
   accept: another type, and a source those rules refuse, answer NULL at a
   sample rate that would refuse an accepted ofdm source. */
static int
test_the_fs_rules_answer_only_an_accepted_ofdm_source (void)
{
  wfm_source_t s = source ();
  /* control: the accepted source at 30 MHz is refused by this rule */
  const char *why = dp_wfm_ofdm_fs_error (&s, 30e6);
  DP_CHECK (why && strstr (why, "fs must be an integer multiple"));
  DP_CHECK (dp_wfm_ofdm_fs_error (&s, 20e6) == NULL);
  /* the same rate on a source its own rules refuse */
  s.scrambler_init = 128;
  DP_CHECK (dp_wfm_ofdm_fs_error (&s, 30e6) == NULL);
  /* and on another type, one its own rules accept (the defaults), so the
     type alone answers */
  s      = (wfm_source_t)WFM_SOURCE_DEFAULTS;
  s.type = WFM_SYNTH_BPSK;
  DP_CHECK (dp_wfm_source_error (&s) == NULL);
  DP_CHECK (dp_wfm_ofdm_fs_error (&s, 30e6) == NULL);
  return 0;
}

/* The frame renderer's contract is unit mean power over the frame, its gap
   not counted (wfm_synth_frame_fn). The ofdm renderer meets it as wlan
   does: every field at unit power over its useful samples. At 6 Mb/s
   (BPSK) every Data symbol is exact too, so the whole check is a rounding
   bound, not a statistic: 1e-4, from test_wlan_core.c section 10 (a
   sample's error is at most the 64-point transform's 9.2e-6, which moves a
   mean power by at most twice that). At M = 2 every length doubles; the
   error it must catch is a frame at another scale, such as the inverse
   transform's own 1/64, which reads 52 / 64^2. */
static int
test_the_frame_has_unit_power_over_its_useful_samples (void)
{
  wfm_source_t s             = source ();
  s.wlan                     = 6;
  s.gap                      = 200;
  const unsigned       m     = 2;
  const dp_wlan_rate_t r6    = { 1u, WLAN_CODE_1_2 };
  const size_t         n_sym = dp_wlan_rate_n_sym (&r6, 100);
  const size_t         len   = dp_wlan_ppdu_len (n_sym, 0u, m);
  DP_REQUIRE (len == (400 + 80 * n_sym) * m);
  cf *x = render (&s, 20e6 * m, len + s.gap);
  DP_REQUIRE (x);
  /* L-STF: 160 samples, no prefix; L-LTF: 128 useful after a 32 prefix;
     SIGNAL: 64 after a 16 prefix; at 20 MHz, times M here */
  size_t first[3] = { 0, 192, 336 }, n[3] = { 160, 128, 64 };
  for (size_t f = 0; f < 3 + n_sym; f++)
    {
      const size_t a = f < 3 ? first[f] : 400 + 80 * (f - 3) + 16;
      const size_t k = f < 3 ? n[f] : 64;
      double       p = 0;
      for (size_t i = a * m; i < (a + k) * m; i++)
        p += cabs (x[i]) * cabs (x[i]);
      p /= (double)(k * m);
      if (fabs (p - 1.0) >= 1e-4)
        DP_TEST_ERR ("field %zu: mean power %.6f", f, p);
      DP_CHECK (fabs (p - 1.0) < 1e-4);
    }
  for (size_t i = len; i < len + s.gap; i++)
    DP_CHECK (x[i] == 0.0f);
  free (x);
  return 0;
}

/* The gap is noise at the source's noise variance, like every sample of the
   source: with snr 10 dB against a unit-power PPDU the gap's variance is 0.1.
   A clean source's gap is exact zeros. */
static int
test_the_gap_carries_noise_at_the_noise_variance (void)
{
  wfm_source_t s            = source ();
  s.gap                     = 20000;
  const dp_wlan_rate_t rate = { 4u, WLAN_CODE_3_4 };
  const size_t         len
      = dp_wlan_ppdu_len (dp_wlan_rate_n_sym (&rate, 100), 0u, 1u);
  cf *clean = render (&s, 20e6, len + s.gap);
  DP_REQUIRE (clean);
  for (size_t i = len; i < len + s.gap; i++)
    DP_CHECK (clean[i] == 0.0f);
  free (clean);
  s.snr = 10.0;
  cf *x = render (&s, 20e6, len + s.gap);
  DP_REQUIRE (x);
  double p = 0;
  for (size_t i = len; i < len + s.gap; i++)
    p += cabs (x[i]) * cabs (x[i]);
  p /= (double)s.gap;
  /* 20000 complex samples: the sample variance is within 3 % (sigma of the
     estimate is 1/sqrt(20000) = 0.7 %), so 5 sigma is the bound. */
  DP_CHECK (fabs (p / 0.1 - 1.0) < 5.0 / sqrt ((double)s.gap));
  free (x);
  return 0;
}

/* A resume mid-PPDU, in the second frame, continues byte for byte. */
static int
test_a_resume_mid_ppdu_continues (void)
{
  wfm_source_t s   = source ();
  s.scrambler_init = 0; /* drawn per PPDU, so the draw's frame index travels */
  s.seed           = 5;
  const size_t          cut = 880 + 333, n = 880 * 2 + 200;
  dp_wfm_synth_state_t *a = dp_wfm_source_to_synth (&s, 20e6);
  DP_REQUIRE (a);
  cf *want = malloc (n * sizeof *want);
  DP_REQUIRE (want);
  dp_wfm_synth_steps (a, want, cut);
  const size_t nb   = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (nb);
  DP_REQUIRE (blob);
  dp_wfm_synth_get_state (a, blob);
  dp_wfm_synth_steps (a, want + cut, n - cut);
  dp_wfm_synth_state_t *b = dp_wfm_source_to_synth (&s, 20e6);
  DP_REQUIRE (b && dp_wfm_synth_set_state (b, blob) == DP_OK);
  cf *got = malloc ((n - cut) * sizeof *got);
  DP_REQUIRE (got);
  dp_wfm_synth_steps (b, got, n - cut);
  DP_CHECK (memcmp (got, want + cut, (n - cut) * sizeof *got) == 0);
  /* control: the second PPDU is not the first (the scrambler was drawn anew,
     from the frame index) */
  DP_CHECK (memcmp (want, want + 880, 880 * sizeof *want) != 0);
  free (want);
  free (got);
  free (blob);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  return 0;
}

/* Each key the renderer's triplet writes decides a restore on its own: the
   synth's geometry check (n_bits, n_out) must NOT be what refuses, so each
   row changes one key and keeps n_out. A short PSDU (10 octets) is one Data
   symbol at every rate from 18 Mb/s, so the rate, the constellation and the
   mac fit one n_out; a window and an M change the PPDU's length, which the
   gap pays back. The gap itself is not a key: it is n_out less the PPDU's
   length, so it cannot differ at an equal n_out and an equal rate, window,
   M, mac and bit count; those keys and the synth's own hold it. */
typedef struct
{
  const char *key;
  void (*edit) (wfm_source_t *, size_t pad); /* pad: the gap to add back */
} key_row_t;

static size_t
ppdu_samples (const wfm_source_t *s, unsigned m)
{
  const dp_wlan_rate_t r
      = { dp_wlan_n_bpsc (s->constellation), (dp_wlan_code_t)s->code_rate };
  const size_t n_oct = s->data_len / 8u + (s->mac == WLAN_MAC_FCS ? 4u : 0u);
  return dp_wlan_ppdu_len (dp_wlan_rate_n_sym (&r, n_oct), (unsigned)s->window,
                           m);
}

static void
k_rate (wfm_source_t *s, size_t p)
{
  (void)p;
  s->code_rate = WLAN_CODE_2_3;
}
static void
k_cons (wfm_source_t *s, size_t p)
{
  (void)p;
  s->constellation = 2;
}
static void
k_scr (wfm_source_t *s, size_t p)
{
  (void)p;
  s->scrambler_init = 94;
}
static void
k_seed (wfm_source_t *s, size_t p)
{
  (void)p;
  s->seed = 9;
}
static void
k_win (wfm_source_t *s, size_t p)
{
  s->window = 4;
  s->gap -= p;
}
/* 14 octets against 10: one Data symbol either way, so the same n_out */
static void
k_mac (wfm_source_t *s, size_t p)
{
  (void)p;
  s->mac = WLAN_MAC_FCS;
}

static int
test_each_key_decides_a_restore_alone (void)
{
  /* 10 octets of data, 64-QAM 3/4 (54 Mb/s) as the base, scrambler 93,
     window 0, gap 100, M = 2 (fs 40 MHz, bandwidth 20 MHz) */
  uint8_t lit[160];
  for (size_t k = 0; k < 160; k++)
    lit[k] = bits_lit[k];
  wfm_source_t base  = source ();
  base.wlan          = 0;
  base.constellation = 3;
  base.code_rate     = WLAN_CODE_3_4;
  base.data = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 160, .bits = lit };
  base.data_len              = 80;
  base.gap                   = 100;
  base.window                = 0;
  const size_t n_base        = ppdu_samples (&base, 2) + base.gap;
  wfm_source_t w4            = base;
  w4.window                  = 4;
  const size_t           pad = ppdu_samples (&w4, 2) - ppdu_samples (&base, 2);
  static const key_row_t rows[] = {
    { "code_rate", k_rate },     { "constellation", k_cons },
    { "scrambler_init", k_scr }, { "seed", k_seed },
    { "window", k_win },         { "mac", k_mac },
  };
  base.seed               = 5;
  dp_wfm_synth_state_t *a = dp_wfm_source_to_synth (&base, 40e6);
  DP_REQUIRE (a);
  cf *skip = malloc (50 * sizeof *skip);
  DP_REQUIRE (skip);
  dp_wfm_synth_steps (a, skip, 50);
  free (skip);
  const size_t nb   = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (nb);
  DP_REQUIRE (blob);
  dp_wfm_synth_get_state (a, blob);
  /* control: the same configuration accepts its own blob */
  dp_wfm_synth_state_t *same = dp_wfm_source_to_synth (&base, 40e6);
  DP_REQUIRE (same && dp_wfm_synth_set_state (same, blob) == DP_OK);
  dp_wfm_synth_destroy (same);
  for (size_t k = 0; k < sizeof rows / sizeof *rows; k++)
    {
      wfm_source_t o = base;
      rows[k].edit (&o, pad);
      /* the row differs in its key only: the frame's geometry is the same,
         so the synth's own n_bits / n_out keys do not refuse it */
      if (ppdu_samples (&o, 2) + o.gap != n_base)
        DP_TEST_ERR ("%s: n_out moved", rows[k].key);
      DP_CHECK (ppdu_samples (&o, 2) + o.gap == n_base);
      DP_CHECK (o.data_len == base.data_len);
      dp_wfm_synth_state_t *c = dp_wfm_source_to_synth (&o, 40e6);
      if (!c)
        DP_TEST_ERR ("%s: not built", rows[k].key);
      DP_REQUIRE (c);
      if (dp_wfm_synth_set_state (c, blob) != DP_ERR_INVALID)
        DP_TEST_ERR ("%s: blob accepted", rows[k].key);
      DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_ERR_INVALID);
      dp_wfm_synth_destroy (c);
    }
  /* the oversampling, at the same n_out: M = 1 with the gap that pays back
     the PPDU's extra length at M = 2 */
  wfm_source_t m1 = base;
  m1.gap = base.gap + ppdu_samples (&base, 2) - ppdu_samples (&base, 1);
  DP_CHECK (ppdu_samples (&m1, 1) + m1.gap == n_base);
  dp_wfm_synth_state_t *c = dp_wfm_source_to_synth (&m1, 20e6);
  DP_REQUIRE (c);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_ERR_INVALID);
  dp_wfm_synth_destroy (c);
  free (blob);
  dp_wfm_synth_destroy (a);
  return 0;
}

/* The one-sample step plays what the block path does, sample for sample. */
static int
test_step_is_steps (void)
{
  wfm_source_t          s    = source ();
  const size_t          n    = 400;
  cf                   *want = render (&s, 20e6, n);
  dp_wfm_synth_state_t *syn  = dp_wfm_source_to_synth (&s, 20e6);
  DP_REQUIRE (want && syn);
  for (size_t i = 0; i < n; i++)
    DP_CHECK (dp_wfm_synth_step (syn) == want[i]);
  free (want);
  dp_wfm_synth_destroy (syn);
  return 0;
}

/* A C caller that skips the validator and hands a refused source to the
   attach gets -1, and a synth it can destroy. */
static int
test_the_attach_refuses_what_the_validator_refuses (void)
{
  wfm_source_t s            = source ();
  s.wlan                    = 7;
  dp_wfm_synth_state_t *syn = dp_wfm_synth_create (
      WFM_SYNTH_FRAMES, 20e6, 0.0, 100.0, 0, 1u, 1, 0, 0, 0, 0.0);
  DP_REQUIRE (syn != NULL);
  DP_CHECK (dp_wfm_source_attach_ofdm (syn, &s, 20e6, 1u) == -1);
  /* another type is no work at all */
  s.type = WFM_SYNTH_TONE;
  DP_CHECK (dp_wfm_source_attach_ofdm (syn, &s, 20e6, 1u) == 0);
  dp_wfm_synth_destroy (syn);
  return 0;
}

/* A data file that is there at validation and gone at build refuses the
   build, on the standalone bridge and the composer's own, rather than
   playing silence. */
static int
test_a_data_file_gone_between_validation_and_build (void)
{
  static const char path[] = "wfm_ofdm_bridge_data.bin";
  FILE             *f      = fopen (path, "wb");
  DP_REQUIRE (f != NULL);
  DP_REQUIRE (fwrite (octets, 1, 100, f) == 100);
  fclose (f);
  wfm_source_t s   = source ();
  s.data           = (wfm_seq_t){ 0 };
  s.data_from_file = path;
  s.data_len       = 800;
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  dp_wfm_synth_state_t *ok = dp_wfm_source_to_synth (&s, 20e6);
  DP_CHECK (ok != NULL);
  dp_wfm_synth_destroy (ok);
  /* the file's bytes are the PSDU's octets: the render is the PPDU wlan
     builds from them (a file unpacks most significant bit first) */
  const dp_wlan_rate_t r36 = { 4u, WLAN_CODE_3_4 };
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_NONE, 0u, 1u) == 0);
  remove (path);
  DP_CHECK (dp_wfm_source_to_synth (&s, 20e6) == NULL);
  DP_CHECK (dp_wfm_compose_build_synth (&s, 20e6, 0, 0.0, 100.0, 0.0, 0, 0, 0)
            == NULL);
  return 0;
}

/* The MAC header is a key of its own: the same n_out and the same rate, the
   header differing in one bit, refuses the other's blob (the header is in the
   PSDU, so a restore into another header would re-render another PPDU). */
static int
test_the_mac_header_is_a_key (void)
{
  uint8_t lit[160], hb2[240];
  for (size_t k = 0; k < 160; k++)
    lit[k] = bits_lit[k];
  memcpy (hb2, hdr_bits, sizeof hb2);
  hb2[7] ^= 1u; /* the last bit of the first octet */
  wfm_source_t a  = source ();
  a.wlan          = 0;
  a.constellation = 3;
  a.code_rate     = WLAN_CODE_3_4;
  a.data     = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 160, .bits = lit };
  a.data_len = 80;
  a.mac      = WLAN_MAC_DATA;
  a.mac_header            = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL,
                                         .len  = 8u * 24u,
                                         .bits = hdr_bits };
  wfm_source_t b          = a;
  b.mac_header.bits       = hb2;
  dp_wfm_synth_state_t *x = dp_wfm_source_to_synth (&a, 20e6);
  dp_wfm_synth_state_t *y = dp_wfm_source_to_synth (&b, 20e6);
  dp_wfm_synth_state_t *z = dp_wfm_source_to_synth (&a, 20e6);
  DP_REQUIRE (x && y && z);
  cf skip[30];
  dp_wfm_synth_steps (x, skip, 30);
  const size_t nb   = dp_wfm_synth_state_bytes (x);
  uint8_t     *blob = malloc (nb);
  DP_REQUIRE (nb != 0 && blob);
  dp_wfm_synth_get_state (x, blob);
  DP_CHECK (dp_wfm_synth_state_bytes (y) == nb);
  DP_CHECK (dp_wfm_synth_set_state (z, blob) == DP_OK); /* the control */
  DP_CHECK (dp_wfm_synth_set_state (y, blob) == DP_ERR_INVALID);
  free (blob);
  dp_wfm_synth_destroy (x);
  dp_wfm_synth_destroy (y);
  dp_wfm_synth_destroy (z);
  return 0;
}

/* The default window: a source nobody gave a window renders with the
   standard's 100 ns transition, `2 * fs / bandwidth` samples, which is what
   wlan's own default gives at the same M; an explicit 0 stays rectangular. */
static int
test_the_default_window_is_the_standards_100_ns (void)
{
  const dp_wlan_rate_t r36 = { 4u, WLAN_CODE_3_4 };
  wfm_source_t         s   = source ();
  const wfm_source_t   d   = WFM_SOURCE_DEFAULTS;
  DP_CHECK (d.window == WFM_WINDOW_UNSET); /* the manifest default */
  s.window = d.window;
  /* the count, at each M the faces can reach: 2 M, by fs / bandwidth */
  DP_CHECK (dp_wfm_source_window (&s, 20e6) == 2u);
  DP_CHECK (dp_wfm_source_window (&s, 40e6) == 4u);
  DP_CHECK (dp_wfm_source_window (&s, 20e6 * 128.0) == 256u);
  /* above 128 there is no count either: the bound is the constructor's */
  DP_CHECK (dp_wfm_source_window (&s, 20e6 * 129.0) == WFM_WINDOW_UNSET);
  s.bandwidth = 5e6;
  DP_CHECK (dp_wfm_source_window (&s, 20e6) == 8u); /* M = 4 */
  s.bandwidth = 0.0;                                /* the preset's 20 MHz */
  DP_CHECK (dp_wfm_source_window (&s, 40e6) == 4u);
  /* a count is returned as it is, 0 included: the rectangular window */
  s.window = 0;
  DP_CHECK (dp_wfm_source_window (&s, 40e6) == 0u);
  s.window = 3;
  DP_CHECK (dp_wfm_source_window (&s, 40e6) == 3u);
  /* a source that is not ofdm renders with no window, at any rate */
  {
    wfm_source_t t = WFM_SOURCE_DEFAULTS;
    t.type         = WFM_SYNTH_TONE;
    DP_CHECK (dp_wfm_source_window (&t, 20e6) == 0u);
    DP_CHECK (dp_wfm_source_window (&t, 40e6) == 0u);
    DP_CHECK (dp_wfm_source_window (&t, 1e6) == 0u);
  }
  /* no whole M: no count, left for the fs rule to refuse by its sentence */
  s.window = WFM_WINDOW_UNSET;
  DP_CHECK (dp_wfm_source_window (&s, 30e6) == WFM_WINDOW_UNSET);
  const char *why = dp_wfm_source_to_synth_error (&s, 30e6);
  DP_CHECK (why && strstr (why, "fs must be an integer multiple"));
  /* never the range sentence: the sentinel is not a count above 80 M */
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6 * 128.0) == NULL);

  /* the render is wlan's PPDU at that window, at M = 1, 2 and 4 */
  s.wlan = 36;
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_NONE,
                          dp_wlan_window_resolve (WLAN_WINDOW_STANDARD, 1u),
                          1u)
            == 0);
  DP_CHECK (check_render (&s, 40e6, &r36, WLAN_MAC_NONE,
                          dp_wlan_window_resolve (WLAN_WINDOW_STANDARD, 2u),
                          2u)
            == 0);
  s.wlan          = 0;
  s.bandwidth     = 5e6;
  s.constellation = 2;
  s.code_rate     = WLAN_CODE_3_4;
  DP_CHECK (check_render (&s, 20e6, &r36, WLAN_MAC_NONE,
                          dp_wlan_window_resolve (WLAN_WINDOW_STANDARD, 4u),
                          4u)
            == 0);
  /* negative control: that is not the rectangular PPDU. The window adds `2 M`
     samples, and the same count rendered at 0 differs. */
  s.bandwidth     = 0.0;
  s.constellation = 0;
  s.code_rate     = 0;
  s.wlan          = 36;
  size_t len0     = 0;
  cf    *rect     = reference (&r36, WLAN_MAC_NONE, s.data_len / 8u, 0,
                               s.scrambler_init, 0u, 1u, &len0);
  cf    *def      = rect ? render (&s, 20e6, len0) : NULL;
  DP_REQUIRE (rect && def);
  double e;
  DP_CHECK (rel_l2 (def, rect, len0, &e) > 1e-3);
  free (rect);
  free (def);

  /* a C caller that zero-fills a source gets 0, the rectangular window, by
     its own spelling */
  const wfm_source_t from = source ();
  wfm_source_t       z;
  memset (&z, 0, sizeof z);
  z.type           = from.type;
  z.snr            = from.snr;
  z.data           = from.data;
  z.data_len       = from.data_len;
  z.wlan           = from.wlan;
  z.scrambler_init = from.scrambler_init;
  DP_CHECK (z.window == 0u); /* nobody set it: the struct's own zero */
  DP_CHECK (check_render (&z, 20e6, &r36, WLAN_MAC_NONE, 0u, 1u) == 0);

  /* a window on another type is still refused; the unset one is not */
  wfm_source_t b = WFM_SOURCE_DEFAULTS;
  b.type         = WFM_SYNTH_BPSK;
  DP_CHECK (dp_wfm_source_error (&b) == NULL);
  b.window = 4;
  why      = dp_wfm_source_error (&b);
  DP_CHECK (why && strstr (why, "are fields of an ofdm source"));
  return 0;
}

int
main (void)
{
  make_data ();
  int rc = 0;
  rc |= test_renders_the_ppdu_wlan_builds ();
  rc |= test_the_default_window_is_the_standards_100_ns ();
  rc |= test_mac_data_renders_header_body_and_fcs ();
  rc |= test_each_name_is_its_value ();
  rc |= test_wlan_n_is_its_pair_for_all_eight ();
  rc |= test_each_refusal_names_its_field ();
  rc |= test_each_range_accepts_its_edge ();
  rc |= test_the_fs_rules_answer_only_an_accepted_ofdm_source ();
  rc |= test_the_frame_has_unit_power_over_its_useful_samples ();
  rc |= test_the_gap_carries_noise_at_the_noise_variance ();
  rc |= test_a_resume_mid_ppdu_continues ();
  rc |= test_each_key_decides_a_restore_alone ();
  rc |= test_the_mac_header_is_a_key ();
  rc |= test_step_is_steps ();
  rc |= test_the_attach_refuses_what_the_validator_refuses ();
  rc |= test_a_data_file_gone_between_validation_and_build ();
  if (rc)
    return 1;
  DP_TEST_END ("wfm_ofdm_bridge");
  return 0;
}
