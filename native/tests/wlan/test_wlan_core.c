/**
 * @file test_wlan_core.c
 * @brief Unit tests for the 802.11 non-HT builders.
 *
 * What this file claims, and what it does not. The standard's literal tables
 * (the RATE bits, the L-STF and L-LTF values and their scales, the
 * constellation labels where they differ from the library's, and the second
 * interleaver permutation) are not in the tree
 * (docs/design/wlan.md section 9), so every test that needs one runs on a
 * SYNTHETIC table built here and claims no conformance with any 802.11 text.
 * What it pins is what is derivable: the counts and lengths from the
 * checked numbers, the field offsets of the layout, the chain's own
 * inverses, the CRC by its catalogue check value, the scrambler by its
 * recurrence, the BCC by its generators' impulse response, and the pilot
 * polarity by an independent recurrence.
 *
 * Sections:
 *   1. Rates: the counts the checked numbers give, and the symbol count
 *   2. The BCC: generators, the impulse response, the puncture patterns
 *   3. The scrambler: the recurrence, the two spellings, the 127 states
 *   4. The minimal MAC and the FCS
 *   5. The SIGNAL and Data bits
 *   6. Interleaving and mapping
 *   7. The points, and the refusals that change nothing
 *   8. The PPDU: lengths, periodicity, prefixes, pilots, the data tones
 */
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/wlan/wlan_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586

/* ── Synthetic tables: NOT the standard's ────────────────────────────── */

static float _Complex g_stf[53], g_ltf[53];
static dp_wlan_signal_rate_t g_rates[16];
static uint16_t              g_relabel_rev[64];

/* A permutation of one symbol that is not the standard's: rotate within
 * groups of s = max(n_bpsc/2, 1) positions. */
static size_t
synth_perm2 (size_t i, size_t n_cbps, size_t n_bpsc)
{
  (void)n_cbps;
  const size_t s = n_bpsc / 2 > 1 ? n_bpsc / 2 : 1;
  return s * (i / s) + (i + 1) % s;
}

/* Not a permutation: every input to position 0. */
static size_t
bad_perm2 (size_t i, size_t n_cbps, size_t n_bpsc)
{
  (void)i;
  (void)n_cbps;
  (void)n_bpsc;
  return 0;
}

static dp_wlan_tables_t
synth_tables (void)
{
  for (int k = -26; k <= 26; k++)
    {
      g_stf[k + 26] = 0.0f;
      g_ltf[k + 26] = 0.0f;
      /* twelve tones at multiples of 4, so the time signal has a period of
       * 16 samples: derivable, whatever the values are */
      if (k != 0 && k % 4 == 0 && abs (k) <= 24)
        g_stf[k + 26] = (float)(1 + (k > 0)) + (float)(1 - (k < 0)) * I;
      if (k != 0)
        g_ltf[k + 26] = (k & 1) ? 1.0f : -1.0f;
    }
  /* an entry per rate combination, bits 0..15 in order: synthetic */
  static const unsigned bpsc[4] = { 1, 2, 4, 6 };
  int                   n       = 0;
  for (int c = 0; c < 4; c++)
    for (int m = 0; m < 4; m++)
      {
        g_rates[n].rate.n_bpsc = bpsc[m];
        g_rates[n].rate.code   = (dp_wlan_code_t)c;
        g_rates[n].bits        = (uint8_t)n;
        n++;
      }
  dp_wlan_tables_t t;
  memset (&t, 0, sizeof t);
  t.stf            = g_stf;
  t.ltf            = g_ltf;
  t.scale_stf      = 1.0 / sqrt (12.0);
  t.scale_ltf      = 1.0 / sqrt (52.0);
  t.scale_data     = 1.0 / sqrt (52.0);
  t.signal_rates   = g_rates;
  t.n_signal_rates = 16;
  t.perm2          = synth_perm2;
  return t;
}

/* The scrambling sequence by the polynomial's recurrence, an independent
 * formulation from the shift register: s[n] = s[n-7] xor s[n-4], seeded with
 * the first seven bits. */
static void
recurrence (unsigned seq7, size_t n, uint8_t *out)
{
  for (int i = 0; i < 7 && (size_t)i < n; i++)
    out[i] = (uint8_t)((seq7 >> (6 - i)) & 1u);
  for (size_t i = 7; i < n; i++)
    out[i] = out[i - 7] ^ out[i - 4];
}

static int
close_c (float _Complex a, float _Complex b, double tol)
{
  return cabsf (a - b) <= tol;
}

int
main (void)
{
  /* 1 ── Rates ──────────────────────────────────────────────────────────── */
  {
    static const struct
    {
      unsigned       bpsc;
      dp_wlan_code_t code;
      unsigned       cbps, dbps;
    } rates[] = {
      { 1, WLAN_CODE_1_2, 48, 24 },   { 1, WLAN_CODE_3_4, 48, 36 },
      { 2, WLAN_CODE_1_2, 96, 48 },   { 2, WLAN_CODE_3_4, 96, 72 },
      { 4, WLAN_CODE_1_2, 192, 96 },  { 4, WLAN_CODE_3_4, 192, 144 },
      { 6, WLAN_CODE_2_3, 288, 192 }, { 6, WLAN_CODE_3_4, 288, 216 },
    };
    for (size_t i = 0; i < sizeof rates / sizeof *rates; i++)
      {
        const dp_wlan_rate_t r = { rates[i].bpsc, rates[i].code };
        DP_CHECK (dp_wlan_rate_ok (&r));
        DP_CHECK (dp_wlan_n_cbps (&r) == rates[i].cbps);
        DP_CHECK (dp_wlan_n_dbps (&r) == rates[i].dbps);
      }
    const dp_wlan_rate_t bad1 = { 3, WLAN_CODE_1_2 }, bad2 = { 1, 7 };
    DP_CHECK (!dp_wlan_rate_ok (&bad1) && !dp_wlan_rate_ok (&bad2));
    DP_CHECK (!dp_wlan_rate_ok (NULL));
    DP_CHECK (dp_wlan_n_cbps (&bad1) == 0 && dp_wlan_n_dbps (&bad2) == 0);

    /* 100 octets at 16-QAM 3/4: (16 + 800 + 6) / 144 = 5.7 -> 6 symbols */
    const dp_wlan_rate_t r36 = { 4, WLAN_CODE_3_4 };
    DP_CHECK (dp_wlan_rate_n_sym (&r36, 100) == 6);
    DP_CHECK (dp_wlan_rate_n_sym (&r36, 1) == 1); /* 30 bits in 144 */
    DP_CHECK (dp_wlan_rate_n_sym (&r36, 0) == 0);
    DP_CHECK (dp_wlan_rate_n_sym (&r36, WLAN_PSDU_MAX) > 0);
    DP_CHECK (dp_wlan_rate_n_sym (&r36, WLAN_PSDU_MAX + 1) == 0);
    /* the symbol count is exactly enough, and one fewer is not */
    for (size_t len = 1; len < 400; len += 7)
      {
        const size_t n = dp_wlan_rate_n_sym (&r36, len);
        DP_CHECK (n * 144 >= 22 + 8 * len);
        DP_CHECK ((n - 1) * 144 < 22 + 8 * len);
      }
    DP_CHECK (dp_wlan_ppdu_len (6, 0) == 160 + 160 + 80 + 480);
    DP_CHECK (dp_wlan_ppdu_len (6, 2) == 160 + 160 + 80 + 480 + 2);

    /* the object */
    dp_wlan_state_t *w = dp_wlan_create (2, 2);
    DP_REQUIRE (w != NULL);
    DP_CHECK (w->n_bpsc == 4 && w->n_cbps == 192 && w->n_dbps == 144);
    DP_CHECK (dp_wlan_n_sym (w, 100) == 6);
    dp_wlan_reset (w);
    dp_wlan_destroy (w);
    dp_wlan_destroy (NULL);
    DP_CHECK (dp_wlan_create (-1, 0) == NULL);
    DP_CHECK (dp_wlan_create (4, 0) == NULL);
    DP_CHECK (dp_wlan_create (0, 4) == NULL);
    /* every constellation at every rate that gives whole bits: all do here */
    for (int m = 0; m < 4; m++)
      for (int c = 0; c < 4; c++)
        {
          dp_wlan_state_t *x = dp_wlan_create (m, c);
          DP_CHECK (x != NULL);
          dp_wlan_destroy (x);
        }
  }

  /* 2 ── The BCC ────────────────────────────────────────────────────────── */
  {
    conv_code_t c;
    DP_CHECK (dp_wlan_bcc (WLAN_CODE_1_2, &c));
    DP_CHECK (c.k == 7 && c.n == 2 && c.poly[0] == 0133 && c.poly[1] == 0171
              && c.invert == 0 && c.punct_period == 0);
    DP_CHECK (!dp_wlan_bcc ((dp_wlan_code_t)9, &c));

    /* the impulse response of the generators, newest input at the left of the
     * polynomial: a one then zeros gives each output the polynomial's bits in
     * order, here interleaved A B: (1,1)(0,1)(1,1)(1,1)(0,0)(1,0)(1,1) */
    uint8_t       in[16]   = { 1 }, out[64];
    const uint8_t want[14] = { 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1 };
    DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, in, 16, out, sizeof out) == 32);
    DP_CHECK (memcmp (out, want, sizeof want) == 0);
    for (int i = 14; i < 32; i++)
      DP_CHECK (out[i] == 0); /* then the register is empty */

    /* the punctured stream is the unpunctured one with the deleted positions
     * removed: lengths, and the pattern, from the masks' meaning */
    uint8_t  bits[120], full[240], p[240];
    uint32_t rng = 12345u;
    for (int i = 0; i < 120; i++)
      {
        (void)dp_xs32 (&rng);
        bits[i] = (uint8_t)((rng >> 16) & 1u);
      }
    DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, bits, 120, full, sizeof full)
              == 240);
    static const struct
    {
      dp_wlan_code_t code;
      size_t         period, kept;
      uint32_t       mask;
    } pat[] = { { WLAN_CODE_2_3, 2, 3, 0x7 },
                { WLAN_CODE_3_4, 3, 4, 0x27 },
                { WLAN_CODE_5_6, 5, 6, 0x267 } };
    for (size_t q = 0; q < 3; q++)
      {
        const size_t n = dp_wlan_encode (pat[q].code, bits, 120, p, sizeof p);
        DP_CHECK (n == 120 / pat[q].period * pat[q].kept);
        size_t m  = 0;
        int    ok = 1;
        for (size_t i = 0; i < 120; i++)
          for (int j = 0; j < 2; j++)
            if ((pat[q].mask >> ((i % pat[q].period) * 2 + (size_t)j)) & 1u)
              ok &= (m < n && p[m++] == full[2 * i + (size_t)j]);
        DP_CHECK (ok && m == n);
      }
    DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, bits, 120, out, 10) == 0);
  }

  /* 3 ── The scrambler ──────────────────────────────────────────────────── */
  {
    /* Annex I's start, two spellings: the sequence 0110110 is 54, and the
     * register state that produces it is 1011101 */
    DP_CHECK (dp_wlan_scrambler_state_from_sequence (54) == 93);
    uint8_t seq[300], ref[300];
    DP_CHECK (dp_wlan_scrambler_sequence (93, 300, seq) == 300);
    recurrence (54, 300, ref);
    DP_CHECK (memcmp (seq, ref, 300) == 0);
    static const uint8_t first[11] = { 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0 };
    DP_CHECK (memcmp (seq, first, 11) == 0); /* 0110110 0000... */

    /* every state matches the recurrence seeded with its own first seven
     * bits, has period 127, and the 127 prefixes are distinct */
    int seen[128] = { 0 };
    for (unsigned s = 1; s < 128; s++)
      {
        uint8_t a[260], b[260];
        DP_CHECK (dp_wlan_scrambler_sequence ((uint8_t)s, 260, a) == 260);
        unsigned v = 0;
        for (int i = 0; i < 7; i++)
          v = (v << 1) | a[i];
        recurrence (v, 260, b);
        DP_CHECK (memcmp (a, b, 260) == 0);
        DP_CHECK (memcmp (a, a + 127, 127) == 0); /* period 127 */
        DP_CHECK (!seen[v]);
        seen[v] = 1;
        DP_CHECK (dp_wlan_scrambler_state_from_sequence ((uint8_t)v)
                  == (uint8_t)s);
      }
    /* the sequence is the library's `pn`: for every state some seed of the
     * Galois register with polynomial 0x48 (x^7 + x^4 + 1) reproduces it */
    for (unsigned s = 1; s < 128; s++)
      {
        uint8_t a[254], b[254];
        DP_CHECK (dp_wlan_scrambler_sequence ((uint8_t)s, 254, a) == 254);
        int found = 0;
        for (uint64_t seed = 1; seed < 128 && !found; seed++)
          {
            dp_pn_state_t *p = dp_pn_create (0x48, seed, 7, PN_GALOIS);
            DP_REQUIRE (p != NULL);
            dp_pn_generate (p, 254, b, 254);
            dp_pn_destroy (p);
            found = (memcmp (a, b, 254) == 0);
          }
        DP_CHECK (found);
      }
    unsigned ones = 0;
    uint8_t  one[127];
    dp_wlan_scrambler_sequence (127, 127, one);
    for (int i = 0; i < 127; i++)
      ones += one[i];
    DP_CHECK (ones == 64); /* a maximal-length sequence */
    DP_CHECK (dp_wlan_scrambler_sequence (0, 4, seq) == 0);
    DP_CHECK (dp_wlan_scrambler_sequence (128, 4, seq) == 0);
    DP_CHECK (dp_wlan_scrambler_state_from_sequence (0) == 0);
    DP_CHECK (dp_wlan_scrambler_state_from_sequence (128) == 0);
  }

  /* 4 ── The minimal MAC and the FCS ────────────────────────────────────── */
  {
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_NONE, 0, 100) == 100);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_FCS, 0, 100) == 104);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_DATA, 24, 100) == 128);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_NONE, 0, 0) == 0);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_FCS, 0, WLAN_PSDU_MAX - 3) == 0);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_FCS, 0, WLAN_PSDU_MAX - 4)
              == WLAN_PSDU_MAX);
    DP_CHECK (dp_wlan_mpdu_len ((dp_wlan_mac_t)7, 0, 10) == 0);

    /* the FCS is CRC-32/ISO-HDLC, whose catalogue check value over the nine
     * ASCII digits is CBF43926, least significant octet first */
    const uint8_t digits[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    uint8_t       out[64];
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, digits, 9, out, sizeof out)
              == 13);
    DP_CHECK (memcmp (out, digits, 9) == 0);
    DP_CHECK (out[9] == 0x26 && out[10] == 0x39 && out[11] == 0xF4
              && out[12] == 0xCB);

    /* appended that way, the CRC over the whole frame is one constant,
     * whatever the frame: the property the wire order exists to give */
    const dp_crc_t model = DP_CRC_32_ISO_HDLC;
    uint8_t        f1[40], f2[40];
    uint32_t       rng = 99u;
    for (int i = 0; i < 20; i++)
      {
        (void)dp_xs32 (&rng);
        f1[i] = (uint8_t)(rng >> 24);
        (void)dp_xs32 (&rng);
        f2[i] = (uint8_t)(rng >> 24);
      }
    uint8_t m1[32], m2[32];
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, f2, 20, m2, sizeof m2)
              == 24);
    for (int i = 0; i < 20; i++)
      {
        (void)dp_xs32 (&rng);
        f1[i] = (uint8_t)(rng >> 24);
      }
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, f1, 20, m1, sizeof m1)
              == 24);
    DP_CHECK (dp_crc_bytes (&model, m1, 24, 1)
              == dp_crc_bytes (&model, m2, 24, 1));

    /* DATA mode is the header then the body then the FCS: the FCS mode of the
     * concatenation */
    uint8_t hdr[24], body[50], cat[74], d1[80], d2[80];
    for (int i = 0; i < 24; i++)
      hdr[i] = (uint8_t)(i * 7 + 1);
    for (int i = 0; i < 50; i++)
      body[i] = (uint8_t)(i * 3 + 2);
    memcpy (cat, hdr, 24);
    memcpy (cat + 24, body, 50);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_DATA, hdr, 24, body, 50, d1, sizeof d1)
              == 78);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, cat, 74, d2, sizeof d2)
              == 78);
    DP_CHECK (memcmp (d1, d2, 78) == 0);
    /* NONE is the caller's octets, untouched */
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_NONE, NULL, 0, body, 50, d1, sizeof d1)
              == 50);
    DP_CHECK (memcmp (d1, body, 50) == 0);
    /* a refusal leaves the buffer alone */
    memset (d1, 0xAB, sizeof d1);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_DATA, hdr, 24, body, 50, d1, 77) == 0);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_NONE, NULL, 0, body, 0, d1, 80) == 0);
    for (int i = 0; i < 80; i++)
      DP_CHECK (d1[i] == 0xAB);
  }

  /* 5 ── The SIGNAL and Data bits ───────────────────────────────────────── */
  {
    const dp_wlan_tables_t t = synth_tables ();
    const dp_wlan_rate_t   r = { 4, WLAN_CODE_3_4 };
    uint8_t                sig[WLAN_SIG_BITS];
    for (size_t len = 1; len <= WLAN_PSDU_MAX; len += 311)
      {
        DP_CHECK (dp_wlan_signal_bits (&t, &r, len, sig) == WLAN_SIG_BITS);
        unsigned rate_bits = 0, length = 0, parity = 0;
        for (int i = 0; i < 4; i++)
          rate_bits = (rate_bits << 1) | sig[i];
        for (int i = 0; i < 12; i++)
          length |= (unsigned)sig[5 + i] << i;
        for (int i = 0; i < 18; i++)
          parity ^= sig[i];
        /* the entry of this rate in the synthetic table: 2*4 + 2 */
        DP_CHECK (rate_bits == 10u);
        DP_CHECK (sig[4] == 0);   /* reserved */
        DP_CHECK (length == len); /* LENGTH, least significant first */
        DP_CHECK (parity == 0);   /* even parity over the first 17 */
        for (int i = 18; i < 24; i++)
          DP_CHECK (sig[i] == 0); /* tail */
      }
    DP_CHECK (dp_wlan_signal_bits (&t, &r, 0, sig) == 0);
    DP_CHECK (dp_wlan_signal_bits (&t, &r, WLAN_PSDU_MAX + 1, sig) == 0);
    DP_CHECK (dp_wlan_signal_bits (dp_wlan_tables_empty (), &r, 10, sig) == 0);
    dp_wlan_tables_t none = t;
    none.n_signal_rates   = 0;
    DP_CHECK (dp_wlan_signal_bits (&none, &r, 10, sig) == 0);

    /* the Data bits: unscramble, and SERVICE is zero, the PSDU is back, the
     * tail was reset, the pad is zero */
    uint8_t psdu[100];
    for (int i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 37 + 11);
    const size_t n_sym = 6, total = n_sym * 144;
    uint8_t      bits[6 * 144], seq[6 * 144];
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 93, bits, sizeof bits)
              == total);
    dp_wlan_scrambler_sequence (93, total, seq);
    for (size_t i = 0; i < 16; i++)
      DP_CHECK ((bits[i] ^ seq[i]) == 0); /* SERVICE */
    for (size_t i = 0; i < 100; i++)
      for (int b = 0; b < 8; b++)
        DP_CHECK ((bits[16 + 8 * i + (size_t)b] ^ seq[16 + 8 * i + (size_t)b])
                  == ((psdu[i] >> b) & 1)); /* each octet LSB first */
    for (size_t i = 16 + 800; i < 16 + 800 + 6; i++)
      DP_CHECK (bits[i] == 0); /* the tail, after scrambling */
    for (size_t i = 16 + 806; i < total; i++)
      DP_CHECK ((bits[i] ^ seq[i]) == 0); /* the pad */
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 93, bits, total - 1) == 0);
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 0, bits, sizeof bits) == 0);
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 0, 93, bits, sizeof bits) == 0);
    /* a different state gives different scrambled bits */
    uint8_t bits2[6 * 144];
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 5, bits2, sizeof bits2)
              == total);
    DP_CHECK (memcmp (bits, bits2, total) != 0);
  }

  /* 6 ── Interleaving and mapping ───────────────────────────────────────── */
  {
    dp_wlan_tables_t      t       = synth_tables ();
    static const unsigned bpsc[4] = { 1, 2, 4, 6 };
    for (int m = 0; m < 4; m++)
      {
        const unsigned nb = bpsc[m];
        const size_t   nc = 48u * nb, n = 3 * nc;
        uint8_t        in[3 * 288], mid[3 * 288], back[3 * 288];
        for (size_t i = 0; i < n; i++)
          in[i] = (uint8_t)((i * 2654435761u >> 7) & 1u);
        DP_CHECK (dp_wlan_interleave (&t, nb, in, n, mid) == n);
        /* the permutation is the first one then the second, checked bit by
         * bit against the library's index functions */
        int ok = 1;
        for (size_t s = 0; s < 3; s++)
          for (size_t k = 0; k < nc; k++)
            {
              const size_t i = (k % 16) * (nc / 16) + k / 16;
              const size_t j = synth_perm2 (i, nc, nb);
              ok &= (mid[s * nc + j] == in[s * nc + k]);
            }
        DP_CHECK (ok);
        /* and it inverts: undo the second then the first */
        for (size_t s = 0; s < 3; s++)
          for (size_t k = 0; k < nc; k++)
            {
              const size_t i   = (k % 16) * (nc / 16) + k / 16;
              const size_t j   = synth_perm2 (i, nc, nb);
              back[s * nc + k] = mid[s * nc + j];
            }
        DP_CHECK (memcmp (back, in, n) == 0);
        DP_CHECK (dp_wlan_interleave (&t, nb, in, n - 1, mid) == 0);
        DP_CHECK (dp_wlan_interleave (&t, nb, in, 0, mid) == 0);
      }
    DP_CHECK (
        dp_wlan_interleave (&t, 3, (uint8_t[48]){ 0 }, 48, (uint8_t[48]){ 0 })
        == 0);
    DP_CHECK (dp_wlan_interleave (dp_wlan_tables_empty (), 1,
                                  (uint8_t[48]){ 0 }, 48, (uint8_t[48]){ 0 })
              == 0);     /* no second permutation: refused */
    t.perm2 = bad_perm2; /* not a permutation: refused, out untouched */
    uint8_t in48[48] = { 1 }, out48[48];
    memset (out48, 0x5A, sizeof out48);
    DP_CHECK (dp_wlan_interleave (&t, 1, in48, 48, out48) == 0);
    for (int i = 0; i < 48; i++)
      DP_CHECK (out48[i] == 0x5A);
    t.perm2 = synth_perm2;

    /* the mapper: every label gives a point; the points have unit average
     * power; the first bit is bit 0 of the label */
    for (int m = 0; m < 4; m++)
      {
        const unsigned nb   = bpsc[m];
        const size_t   nlab = (size_t)1 << nb;
        uint8_t        bits[6 * 64];
        float _Complex pts[64];
        for (size_t v = 0; v < nlab; v++)
          for (unsigned b = 0; b < nb; b++)
            bits[v * nb + b] = (uint8_t)((v >> b) & 1u);
        DP_CHECK (dp_wlan_map (&t, nb, bits, nlab * nb, pts) == nlab);
        double power = 0.0;
        for (size_t v = 0; v < nlab; v++)
          power += (double)crealf (pts[v]) * crealf (pts[v])
                   + (double)cimagf (pts[v]) * cimagf (pts[v]);
        DP_CHECK_NEAR (power / (double)nlab, 1.0, 1e-5);
        /* a relabeling table permutes the points the labels give */
        for (size_t v = 0; v < nlab; v++)
          g_relabel_rev[v] = (uint16_t)(nlab - 1 - v);
        dp_wlan_tables_t tr = t;
        tr.relabel[nb]      = g_relabel_rev;
        float _Complex rp[64];
        DP_CHECK (dp_wlan_map (&tr, nb, bits, nlab * nb, rp) == nlab);
        int ok = 1;
        for (size_t v = 0; v < nlab; v++)
          ok &= close_c (rp[v], pts[nlab - 1 - v], 1e-6);
        DP_CHECK (ok);
      }
    /* the first bit is bit 0 of the library's label: the bits (1, 0) are
     * label 1 and (0, 1) label 2; in 16-QAM (1,0,0,0) is 1 and (0,0,1,0) is
     * 4 */
    {
      float _Complex got[1], lib[1];
      const uint8_t q10[2] = { 1, 0 }, q01[2] = { 0, 1 };
      const uint8_t l1 = 1, l2 = 2;
      DP_CHECK (dp_wlan_map (&t, 2, q10, 2, got) == 1);
      dp_mpsk_map (&l1, 1, lib, 4);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
      DP_CHECK (dp_wlan_map (&t, 2, q01, 2, got) == 1);
      dp_mpsk_map (&l2, 1, lib, 4);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
      const uint8_t  b1000[4] = { 1, 0, 0, 0 }, b0010[4] = { 0, 0, 1, 0 };
      const uint16_t s1 = 1, s4 = 4;
      DP_CHECK (dp_wlan_map (&t, 4, b1000, 4, got) == 1);
      dp_qam_map (&s1, 1, lib, 16);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
      DP_CHECK (dp_wlan_map (&t, 4, b0010, 4, got) == 1);
      dp_qam_map (&s4, 1, lib, 16);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
    }
    uint8_t one_bit[2] = { 0, 1 };
    float _Complex two[2];
    DP_CHECK (dp_wlan_map (&t, 1, one_bit, 2, two) == 2);
    DP_CHECK (close_c (two[0], -two[1], 1e-6));           /* BPSK: antipodal */
    DP_CHECK (dp_wlan_map (&t, 4, one_bit, 2, two) == 0); /* a partial group */
    DP_CHECK (dp_wlan_map (&t, 5, one_bit, 2, two) == 0);
    DP_CHECK (dp_wlan_map (&t, 1, one_bit, 0, two) == 0);
  }

  /* 7 ── The points, and refusals that change nothing ───────────────────── */
  {
    const dp_wlan_tables_t t = synth_tables ();
    uint8_t                psdu[100];
    for (int i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 29 + 3);
    static const dp_wlan_rate_t rs[] = { { 1, WLAN_CODE_1_2 },
                                         { 2, WLAN_CODE_3_4 },
                                         { 4, WLAN_CODE_3_4 },
                                         { 6, WLAN_CODE_2_3 } };
    for (size_t q = 0; q < 4; q++)
      {
        const size_t n_sym = dp_wlan_rate_n_sym (&rs[q], 100);
        const size_t n_pts = 48 + 48 * n_sym;
        float _Complex pts[48 + 48 * 400];
        DP_CHECK (dp_wlan_points (&t, &rs[q], psdu, 100, 93, pts, n_pts)
                  == n_pts);
        /* the SIGNAL field's 48: the stages composed here, by hand */
        uint8_t sig[WLAN_SIG_BITS], coded[48], inter[48];
        float _Complex hand[48];
        DP_CHECK (dp_wlan_signal_bits (&t, &rs[q], 100, sig) == 24);
        DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, sig, 24, coded, 48) == 48);
        DP_CHECK (dp_wlan_interleave (&t, 1, coded, 48, inter) == 48);
        DP_CHECK (dp_wlan_map (&t, 1, inter, 48, hand) == 48);
        int ok = 1;
        for (int i = 0; i < 48; i++)
          ok &= close_c (pts[i], hand[i], 1e-6);
        DP_CHECK (ok);
        /* the SIGNAL field is BPSK: every point real, unit magnitude */
        for (int i = 0; i < 48; i++)
          DP_CHECK (fabsf (cimagf (pts[i])) < 1e-6
                    && fabsf (cabsf (pts[i]) - 1.0f) < 1e-6);
        /* a smaller capacity is refused */
        DP_CHECK (dp_wlan_points (&t, &rs[q], psdu, 100, 93, pts, n_pts - 1)
                  == 0);
      }
    /* an empty table refuses and writes nothing */
    float _Complex poison[600];
    for (int i = 0; i < 600; i++)
      poison[i] = 7.0f + 7.0f * I;
    const dp_wlan_rate_t r = { 4, WLAN_CODE_3_4 };
    DP_CHECK (dp_wlan_points (dp_wlan_tables_empty (), &r, psdu, 100, 93,
                              poison, 600)
              == 0);
    DP_CHECK (dp_wlan_ppdu (dp_wlan_tables_empty (), &r, psdu, 100, 93, 0,
                            poison, 600)
              == 0);
    for (int i = 0; i < 48; i++)
      DP_CHECK (poison[i] == 7.0f + 7.0f * I);
    dp_ofdm_state_t *o = dp_ofdm_create ();
    DP_CHECK (dp_wlan_layout (o, dp_wlan_tables_empty (), 6, 0) == -1);
    dp_ofdm_destroy (o);
  }

  /* 8 ── The PPDU ───────────────────────────────────────────────────────── */
  {
    const dp_wlan_tables_t t = synth_tables ();
    const dp_wlan_rate_t   r = { 4, WLAN_CODE_3_4 };
    uint8_t                psdu[100];
    for (int i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 41 + 7);
    const size_t    n_sym = dp_wlan_rate_n_sym (&r, 100); /* 6 */
    const size_t    n     = dp_wlan_ppdu_len (n_sym, 0);
    float _Complex *x     = malloc (n * sizeof *x);
    DP_REQUIRE (x != NULL);
    DP_CHECK (dp_wlan_ppdu (&t, &r, psdu, 100, 93, 0, x, n) == n);
    DP_CHECK (n == 160 + 160 + 80 + 480);
    DP_CHECK (dp_wlan_ppdu (&t, &r, psdu, 100, 93, 0, x, n - 1) == 0);

    /* L-STF: tones at multiples of four give a period of 16 samples */
    for (size_t i = 0; i + 16 < 160; i++)
      DP_CHECK (close_c (x[i], x[i + 16], 1e-5));
    DP_CHECK (cabsf (x[0]) > 0.0f);
    /* L-LTF: a 32-sample prefix that is the last 32 of the body, two equal
     * 64-sample periods */
    for (int i = 0; i < 32; i++)
      DP_CHECK (close_c (x[160 + i], x[160 + 32 + 96 + i], 1e-6));
    for (int i = 0; i < 64; i++)
      DP_CHECK (close_c (x[192 + i], x[192 + 64 + i], 1e-6));
    /* every later OFDM symbol: a 16-sample prefix copied from the end */
    for (size_t s = 0; s < 1 + n_sym; s++)
      {
        const size_t b = 320 + 80 * s;
        for (int i = 0; i < 16; i++)
          DP_CHECK (
              close_c (x[b + (size_t)i], x[b + 16 + 48 + (size_t)i], 1e-6));
      }

    /* the Data symbols, demodulated by a naive DFT: the pilots are the
     * checked values times the polarity from an independent recurrence, and
     * the data tones are the points the builder returns */
    uint8_t seq[127];
    dp_wlan_scrambler_sequence (127, 127, seq); /* the all-ones start */
    float _Complex pts[48 + 48 * 6];
    DP_REQUIRE (dp_wlan_points (&t, &r, psdu, 100, 93, pts, 48 + 48 * 6)
                == 48 + 48 * 6);
    /* the checked pilots, as literals and not the header's macros: -21, -7
     * and 7 carry +1 and 21 carries -1 (17.3.5.9, 17.3.5.10) */
    static const int    pk[4]     = { -21, -7, 7, 21 };
    static const double pv[4]     = { 1.0, 1.0, 1.0, -1.0 };
    int                 pilots_ok = 1, tones_ok = 1;
    for (size_t s = 0; s < 1 + n_sym; s++)
      {
        const float _Complex *body = x + 320 + 80 * s + 16;
        const double          pol  = seq[s] ? -1.0 : 1.0;
        size_t                di   = 0;
        for (int k = -26; k <= 26; k++)
          {
            float _Complex acc = 0.0f;
            for (int m = 0; m < 64; m++)
              acc += body[m] * cexpf ((float)(-TWOPI * k * m / 64.0) * I);
            acc /= (float)(64.0 * t.scale_data);
            int ip = -1;
            for (int i = 0; i < 4; i++)
              if (k == pk[i])
                ip = i;
            if (ip >= 0)
              pilots_ok &= close_c (acc, (float)(pv[ip] * pol), 1e-4);
            else if (k == 0)
              pilots_ok &= (cabsf (acc) < 1e-4); /* DC carries nothing */
            else
              tones_ok &= close_c (acc, pts[48 * s + di++], 1e-4);
          }
        tones_ok &= (di == 48);
      }
    DP_CHECK (pilots_ok);
    DP_CHECK (tones_ok);

    /* a window adds its tail and leaves the interior alone */
    float _Complex *xw = malloc ((n + 4) * sizeof *xw);
    DP_REQUIRE (xw != NULL);
    DP_CHECK (dp_wlan_ppdu (&t, &r, psdu, 100, 93, 4, xw, n + 4) == n + 4);
    for (size_t i = 200; i < n - 200; i++)
      if (i % 80 > 8 && i % 80 < 72)
        DP_CHECK (close_c (xw[i], x[i], 1e-4));
    free (xw);
    free (x);
  }

  /* ── 9. The standard's tables, each against its printed form ──────────
   * Every expectation below is typed here in the form the standard prints
   * it (IEEE Std 802.11-2024, as transcribed on #3109), not derived from
   * the table under test. */
  {
    const dp_wlan_tables_t *T = dp_wlan_tables_standard ();

    /* Table 17-6: the RATE bits R1 R2 R3 R4 for each (N_BPSC, code). */
    static const struct
    {
      unsigned       n_bpsc;
      dp_wlan_code_t code;
      const char    *bits;
    } rate_rows[] = {
      { 1, WLAN_CODE_1_2, "1101" }, { 1, WLAN_CODE_3_4, "1111" },
      { 2, WLAN_CODE_1_2, "0101" }, { 2, WLAN_CODE_3_4, "0111" },
      { 4, WLAN_CODE_1_2, "1001" }, { 4, WLAN_CODE_3_4, "1011" },
      { 6, WLAN_CODE_2_3, "0001" }, { 6, WLAN_CODE_3_4, "0011" },
    };
    for (size_t i = 0; i < sizeof rate_rows / sizeof *rate_rows; i++)
      {
        dp_wlan_rate_t r = { rate_rows[i].n_bpsc, rate_rows[i].code };
        uint8_t        sig[WLAN_SIG_BITS];
        DP_CHECK (dp_wlan_signal_bits (T, &r, 100, sig) == WLAN_SIG_BITS);
        for (int b = 0; b < 4; b++)
          DP_CHECK (sig[b] == (uint8_t)(rate_rows[i].bits[b] - '0'));
      }
    /* A pair Clause 17 has no RATE for is refused, not guessed. */
    {
      dp_wlan_rate_t r56 = { 6, WLAN_CODE_5_6 }, odd = { 1, WLAN_CODE_2_3 };
      uint8_t        sig[WLAN_SIG_BITS];
      DP_CHECK (dp_wlan_signal_bits (T, &r56, 100, sig) == 0);
      DP_CHECK (dp_wlan_signal_bits (T, &odd, 100, sig) == 0);
    }

    /* Eq. (17-6) and (17-8), k = -26..26, as printed: the L-STF's sign of
     * (1+j) per tone, scaled by sqrt(13/6), and the L-LTF's +-1. */
    static const int stf_sign[53]
        = { 0,  0, 1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0,  0, -1, 0, 0,  0,
            -1, 0, 0, 0, 1, 0, 0,  0, 0, 0, 0, 0, -1, 0, 0,  0, -1, 0,
            0,  0, 1, 0, 0, 0, 1,  0, 0, 0, 1, 0, 0,  0, 1,  0, 0 };
    static const int ltf_val[53]
        = { 1,  1,  -1, -1, 1,  1, -1, 1,  -1, 1, 1,  1,  1,  1, 1,  -1, -1, 1,
            1,  -1, 1,  -1, 1,  1, 1,  1,  0,  1, -1, -1, 1,  1, -1, 1,  -1, 1,
            -1, -1, -1, -1, -1, 1, 1,  -1, -1, 1, -1, 1,  -1, 1, 1,  1,  1 };
    const double sq = sqrt (13.0 / 6.0);
    for (int i = 0; i < 53; i++)
      {
        DP_CHECK (fabs (crealf (T->stf[i]) - stf_sign[i] * sq) < 1e-6);
        DP_CHECK (fabs (cimagf (T->stf[i]) - stf_sign[i] * sq) < 1e-6);
        DP_CHECK (crealf (T->ltf[i]) == (float)ltf_val[i]);
        DP_CHECK (cimagf (T->ltf[i]) == 0.0f);
      }
    DP_CHECK (T->stf[26] == 0.0f && T->ltf[26] == 0.0f); /* DC */
    /* One scale for every field: 52 occupied tones of unit power. */
    DP_CHECK (T->scale_stf == T->scale_ltf && T->scale_ltf == T->scale_data);
    DP_CHECK (fabs (T->scale_data - 1.0 / sqrt (52.0)) < 1e-15);

    /* Eq. (17-16), (17-17): bijective on every Clause 17 (N_CBPS, N_BPSC),
     * the identity for BPSK (s = 1), and the standard's own worked example
     * (Annex I.1.6.2: k = 17 goes to i = 13, then j = 12, at 192 / 4). */
    static const unsigned pairs[4][2]
        = { { 48, 1 }, { 96, 2 }, { 192, 4 }, { 288, 6 } };
    for (int p = 0; p < 4; p++)
      {
        const size_t n = pairs[p][0], b = pairs[p][1];
        uint8_t     *seen = calloc (n, 1);
        for (size_t i = 0; i < n; i++)
          {
            const size_t j = T->perm2 (i, n, b);
            DP_REQUIRE (j < n);
            DP_CHECK (!seen[j]);
            seen[j] = 1;
            if (b == 1)
              DP_CHECK (j == i);
          }
        free (seen);
      }
    {
      const size_t k = 17, i1 = (192 / 16) * (k % 16) + k / 16;
      DP_CHECK (i1 == 13);
      DP_CHECK (T->perm2 (i1, 192, 4) == 12);
    }

    /* Figure 17-10, Tables 17-14 to 17-18: the bit encodings as the
     * standard prints them (B0 is the first bit of the group), each point
     * d = (I + jQ) * K_MOD, against dp_wlan_map() through the relabels. */
    static const int lv16[4] = { -3, -1, 3, 1 }; /* B0B1: 00 01 10 11 */
    static const int lv64[8]
        = { -7, -5, -1, -3, 7, 5, 1, 3 }; /* B0B1B2 as MSB-first */
    static const unsigned nbs[4] = { 1, 2, 4, 6 };
    for (int ni = 0; ni < 4; ni++)
      {
        const unsigned nb       = nbs[ni];
        const unsigned n_groups = 1u << nb;
        for (unsigned v = 0; v < n_groups; v++)
          {
            uint8_t bits[6];
            for (unsigned b = 0; b < nb; b++)
              bits[b] = (uint8_t)((v >> b) & 1u); /* B0 first */
            double li = 0, lq = 0, K = 1;
            if (nb == 1)
              li = bits[0] ? 1 : -1;
            else if (nb == 2)
              {
                K  = 1 / sqrt (2.0);
                li = bits[0] ? 1 : -1;
                lq = bits[1] ? 1 : -1;
              }
            else if (nb == 4)
              {
                K  = 1 / sqrt (10.0);
                li = lv16[bits[0] * 2 + bits[1]];
                lq = lv16[bits[2] * 2 + bits[3]];
              }
            else
              {
                K  = 1 / sqrt (42.0);
                li = lv64[bits[0] * 4 + bits[1] * 2 + bits[2]];
                lq = lv64[bits[3] * 4 + bits[4] * 2 + bits[5]];
              }
            float _Complex pt;
            DP_CHECK (dp_wlan_map (T, nb, bits, nb, &pt) == 1);
            DP_CHECK (fabs (crealf (pt) - li * K) < 1e-5);
            DP_CHECK (fabs (cimagf (pt) - lq * K) < 1e-5);
          }
      }
  }

  DP_TEST_END ("test_wlan_core");
}
