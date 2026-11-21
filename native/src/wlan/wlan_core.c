#include "doppler/wlan/wlan_core.h"
#include "doppler/dp_interleave.h"
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/pn/pn_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const dp_wlan_tables_t EMPTY_TABLES;

const dp_wlan_tables_t *
dp_wlan_tables_empty (void)
{
  return &EMPTY_TABLES;
}

/* ── IEEE Std 802.11-2024, Clause 17: the tables ─────────────────────────
 * Each literal is cited to its table,
 * equation or figure.
 * native/tests/wlan/test_wlan_core.c holds each against an independent
 * copy of the printed form and goes red on a one-entry change. */

/* Eq. (17-6): S(-26:26) = sqrt(13/6) * {...}, k = -26..26, DC zero. */
#define SQ 1.47196014f /* sqrt (13/6) */
#define SP (SQ + SQ * I)
#define SM (-SQ - SQ * I)
static const float _Complex STF[53]
    = { 0,  0, SP, 0, 0,  0, SM, 0, 0, 0, SP, 0, 0,  0, SM, 0, 0,  0,
        SM, 0, 0,  0, SP, 0, 0,  0, 0, 0, 0,  0, SM, 0, 0,  0, SM, 0,
        0,  0, SP, 0, 0,  0, SP, 0, 0, 0, SP, 0, 0,  0, SP, 0, 0 };

/* Eq. (17-8): L(-26:26), unity scale, DC zero. */
static const float _Complex LTF[53]
    = { 1,  1,  -1, -1, 1,  1, -1, 1,  -1, 1, 1,  1,  1,  1, 1,  -1, -1, 1,
        1,  -1, 1,  -1, 1,  1, 1,  1,  0,  1, -1, -1, 1,  1, -1, 1,  -1, 1,
        -1, -1, -1, -1, -1, 1, 1,  -1, -1, 1, -1, 1,  -1, 1, 1,  1,  1 };

/* Table 17-6: R1..R4, R1 the most significant bit of the integer. */
static const dp_wlan_signal_rate_t SIGNAL_RATES[8] = {
  { { 1u, WLAN_CODE_1_2 }, 13u, 6u },  /* 6 Mb/s, 1101 */
  { { 1u, WLAN_CODE_3_4 }, 15u, 9u },  /* 9 Mb/s, 1111 */
  { { 2u, WLAN_CODE_1_2 }, 5u, 12u },  /* 12 Mb/s, 0101 */
  { { 2u, WLAN_CODE_3_4 }, 7u, 18u },  /* 18 Mb/s, 0111 */
  { { 4u, WLAN_CODE_1_2 }, 9u, 24u },  /* 24 Mb/s, 1001 */
  { { 4u, WLAN_CODE_3_4 }, 11u, 36u }, /* 36 Mb/s, 1011 */
  { { 6u, WLAN_CODE_2_3 }, 1u, 48u },  /* 48 Mb/s, 0001 */
  { { 6u, WLAN_CODE_3_4 }, 3u, 54u },  /* 54 Mb/s, 0011 */
};

/* Eq. (17-16), (17-17): j = s*floor(i/s) + (i + N_CBPS - floor(16 i /
 * N_CBPS)) mod s, s = max(N_BPSC/2, 1). */
static size_t
standard_perm2 (size_t i, size_t n_cbps, size_t n_bpsc)
{
  const size_t s = n_bpsc / 2 > 1 ? n_bpsc / 2 : 1;
  return s * (i / s) + (i + n_cbps - (16 * i) / n_cbps) % s;
}

/* Figure 17-10, Tables 17-14 to 17-18, as the library's symbol index for each
 * bit group (the first bit of the group is bit 0 of the index). The library's
 * BPSK is 1 - 2b and its QPSK and QAM put the first bit on the sign of I,
 * which the standard's Gray labels do not, so each is a relabel; derived
 * from the standard's levels, held as literals and pinned in the test. */
static const uint16_t RELABEL_BPSK[2] = { 1, 0 };
static const uint16_t RELABEL_QPSK[4] = { 3, 2, 1, 0 };
static const uint16_t RELABEL_QAM16[16]
    = { 5, 4, 7, 6, 1, 0, 3, 2, 13, 12, 15, 14, 9, 8, 11, 10 };
static const uint16_t RELABEL_QAM64[64]
    = { 9,  8,  11, 10, 13, 12, 15, 14, 1,  0,  3,  2,  5,  4,  7,  6,
        25, 24, 27, 26, 29, 28, 31, 30, 17, 16, 19, 18, 21, 20, 23, 22,
        41, 40, 43, 42, 45, 44, 47, 46, 33, 32, 35, 34, 37, 36, 39, 38,
        57, 56, 59, 58, 61, 60, 63, 62, 49, 48, 51, 50, 53, 52, 55, 54 };

/* Every field is scaled to unit average power: its 52 occupied tones carry
 * 52 units (the L-STF's sqrt(13/6) is what makes 12 tones carry 52), so one
 * factor, 1/sqrt(52), serves the PPDU (design page, goal 5). That is this
 * library's convention (#3148); Annex I's printed samples are the same math
 * with 1/64, i.e. these times sqrt(52)/64. */
static const dp_wlan_tables_t STANDARD_TABLES = {
  .stf            = STF,
  .ltf            = LTF,
  .scale_stf      = 0.13867504905630728, /* 1 / sqrt (52) */
  .scale_ltf      = 0.13867504905630728,
  .scale_data     = 0.13867504905630728,
  .signal_rates   = SIGNAL_RATES,
  .n_signal_rates = 8,
  .perm2          = standard_perm2,
  .relabel = { NULL, RELABEL_BPSK, RELABEL_QPSK, NULL, RELABEL_QAM16, NULL,
               RELABEL_QAM64 },
};

const dp_wlan_tables_t *
dp_wlan_tables_standard (void)
{
  return &STANDARD_TABLES;
}

/* ── Rates ───────────────────────────────────────────────────────────── */

/* The code rate as a fraction: numerator and denominator. */
static int
code_fraction (dp_wlan_code_t code, unsigned *num, unsigned *den)
{
  switch (code)
    {
    case WLAN_CODE_1_2:
      *num = 1;
      *den = 2;
      return 1;
    case WLAN_CODE_2_3:
      *num = 2;
      *den = 3;
      return 1;
    case WLAN_CODE_3_4:
      *num = 3;
      *den = 4;
      return 1;
    case WLAN_CODE_5_6:
      *num = 5;
      *den = 6;
      return 1;
    default:
      return 0;
    }
}

int
dp_wlan_rate_ok (const dp_wlan_rate_t *r)
{
  unsigned num, den;
  if (!r
      || !(r->n_bpsc == 1 || r->n_bpsc == 2 || r->n_bpsc == 4
           || r->n_bpsc == 6)
      || !code_fraction (r->code, &num, &den))
    return 0;
  /* the data bits per symbol, 48 * n_bpsc * num / den, are whole for every
     such pair (the test checks all sixteen): nothing more to refuse */
  return 1;
}

int
dp_wlan_mbps (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
              unsigned *mbps)
{
  for (size_t i = 0; i < t->n_signal_rates; i++)
    if (t->signal_rates[i].mbps != 0
        && t->signal_rates[i].rate.n_bpsc == r->n_bpsc
        && t->signal_rates[i].rate.code == r->code)
      {
        *mbps = t->signal_rates[i].mbps;
        return 1;
      }
  return 0;
}

int
dp_wlan_rate_from_mbps (const dp_wlan_tables_t *t, unsigned mbps,
                        dp_wlan_rate_t *r)
{
  if (mbps == 0)
    return 0;
  for (size_t i = 0; i < t->n_signal_rates; i++)
    if (t->signal_rates[i].mbps == mbps)
      {
        *r = t->signal_rates[i].rate;
        return 1;
      }
  return 0;
}

unsigned
dp_wlan_n_cbps (const dp_wlan_rate_t *r)
{
  return dp_wlan_rate_ok (r) ? WLAN_N_SD * r->n_bpsc : 0u;
}

unsigned
dp_wlan_n_dbps (const dp_wlan_rate_t *r)
{
  unsigned num = 0, den = 1;
  if (!dp_wlan_rate_ok (r) || !code_fraction (r->code, &num, &den))
    return 0u;
  return WLAN_N_SD * r->n_bpsc * num / den;
}

size_t
dp_wlan_rate_n_sym (const dp_wlan_rate_t *r, size_t psdu_len)
{
  const unsigned n_dbps = dp_wlan_n_dbps (r);
  if (n_dbps == 0 || psdu_len < 1 || psdu_len > WLAN_PSDU_MAX)
    return 0;
  const size_t bits = WLAN_SERVICE_BITS + 8 * psdu_len + WLAN_TAIL_BITS;
  return (bits + n_dbps - 1) / n_dbps;
}

size_t
dp_wlan_ppdu_len (size_t n_sym, unsigned window, unsigned oversample)
{
  if (n_sym < 1 || n_sym > WLAN_N_SYM_MAX || oversample < 1
      || oversample > WLAN_OVERSAMPLE_MAX)
    return 0;
  return (size_t)oversample
             * (WLAN_STF_LEN + WLAN_LTF_LEN + WLAN_SYM_LEN
                + (size_t)WLAN_SYM_LEN * n_sym)
         + window;
}

/* The puncture patterns, Figure 17-9: bit i*n+j transmits output j of input
 * i of the period. 2/3 sends A0 B0 A1; 3/4 sends A0 B0 A1 B2. 5/6 is
 * Clause 19's and is not checked against the text. */
int
dp_wlan_bcc (dp_wlan_code_t code, conv_code_t *c)
{
  static const struct
  {
    unsigned period;
    uint32_t mask;
  } PUNCT[4] = { { 0, 0 }, { 2, 0x7 }, { 3, 0x27 }, { 5, 0x267 } };
  if ((unsigned)code > (unsigned)WLAN_CODE_5_6)
    return 0;
  memset (c, 0, sizeof *c);
  c->k            = 7;
  c->n            = 2;
  c->poly[0]      = 0133;
  c->poly[1]      = 0171;
  c->punct_period = PUNCT[code].period;
  c->punct_mask   = PUNCT[code].mask;
  return 1;
}

/* ── The object ──────────────────────────────────────────────────────── */

/* The window ceiling, declared once: the modulator takes no window longer
   than an OFDM symbol of its class, and the shortest is the SIGNAL and Data
   symbol of 80 * M samples. dp_wlan_params_why() and dp_wlan_layout() both
   ask it. */
static int
wlan_window_ok (uint32_t window, uint32_t oversample)
{
  return window <= WLAN_SYM_LEN * oversample;
}

uint32_t
dp_wlan_window_resolve (uint32_t window, uint32_t oversample)
{
  return window == WLAN_WINDOW_STANDARD ? dp_wlan_window_standard (oversample)
                                        : window;
}

const char *
dp_wlan_params_why (int constellation, int code_rate, uint32_t window,
                    uint32_t oversample)
{
  if (constellation < 0 || constellation > 3)
    return "constellation must be 0 to 3";
  if (code_rate < 0 || code_rate > 3)
    return "code_rate must be 0 to 3";
  if (oversample < 1 || oversample > WLAN_OVERSAMPLE_MAX)
    return "oversample must be 1 to 128";
  if (!wlan_window_ok (dp_wlan_window_resolve (window, oversample),
                       oversample))
    return "window must be 0 to 80 * oversample";
  return NULL;
}

unsigned
dp_wlan_n_bpsc (int constellation)
{
  static const unsigned bpsc[4] = { 1, 2, 4, 6 };
  return constellation >= 0 && constellation <= 3 ? bpsc[constellation] : 0u;
}

dp_wlan_state_t *
dp_wlan_create (int constellation, int code_rate, uint32_t window,
                uint32_t oversample)
{
  if (dp_wlan_params_why (constellation, code_rate, window, oversample))
    return NULL;
  const dp_wlan_rate_t r
      = { dp_wlan_n_bpsc (constellation), (dp_wlan_code_t)code_rate };
  dp_wlan_state_t *s = dp_xcalloc (1, sizeof *s);
  s->n_bpsc          = r.n_bpsc;
  s->code            = (uint32_t)code_rate;
  s->n_cbps          = dp_wlan_n_cbps (&r);
  s->n_dbps          = dp_wlan_n_dbps (&r);
  s->window          = dp_wlan_window_resolve (window, oversample);
  s->oversample      = oversample;
  unsigned mbps      = 0;
  s->rate_mbps
      = dp_wlan_mbps (dp_wlan_tables_standard (), &r, &mbps) ? mbps : 0u;
  return s;
}

dp_wlan_state_t *
dp_wlan_create_why (int constellation, int code_rate, uint32_t window,
                    uint32_t oversample, const char **why)
{
  const char *w
      = dp_wlan_params_why (constellation, code_rate, window, oversample);
  if (w && why)
    *why = w;
  return w ? NULL
           : dp_wlan_create (constellation, code_rate, window, oversample);
}

void
dp_wlan_destroy (dp_wlan_state_t *state)
{
  free (state);
}

void
dp_wlan_reset (dp_wlan_state_t *state)
{
  (void)state; /* the rate is configuration: nothing runs */
}

size_t
dp_wlan_n_sym (const dp_wlan_state_t *state, size_t psdu_len)
{
  const dp_wlan_rate_t r = { state->n_bpsc, (dp_wlan_code_t)state->code };
  return dp_wlan_rate_n_sym (&r, psdu_len);
}

size_t
dp_wlan_ppdu_max_out (const dp_wlan_state_t *state, size_t n_in)
{
  const size_t n_sym = dp_wlan_n_sym (state, n_in);
  return n_sym && state->rate_mbps
             ? dp_wlan_ppdu_len (n_sym, state->window, state->oversample)
             : 0;
}

int64_t
dp_wlan_ppdu_why (dp_wlan_state_t *state, const uint8_t *in, size_t n_in,
                  uint32_t scrambler_state, float _Complex *out,
                  size_t max_out, const char **why)
{
  const size_t n        = dp_wlan_ppdu_max_out (state, n_in);
  const char  *sentence = NULL;
  if (state->rate_mbps == 0)
    sentence = "this rate has no SIGNAL RATE bits (Table 17-6 lists eight of "
               "the sixteen pairs)";
  else if (n == 0)
    sentence = "the PSDU is 1 to 4095 octets";
  else if (scrambler_state < 1 || scrambler_state > 127)
    sentence = "the scrambler state is 1 to 127";
  else if (max_out < n)
    sentence = "the output buffer is shorter than the PPDU";
  if (sentence)
    {
      if (why)
        *why = sentence;
      return -1;
    }
  /* every refusal dp_wlan_ppdu() could make is tested above (a row, a length,
     a state, a capacity, with the standard tables), so it renders n samples */
  const dp_wlan_rate_t r = { state->n_bpsc, (dp_wlan_code_t)state->code };
  return (int64_t)dp_wlan_ppdu (dp_wlan_tables_standard (), &r, in, n_in,
                                (uint8_t)scrambler_state, state->window,
                                state->oversample, out, max_out);
}

/* ── The scrambler ───────────────────────────────────────────────────── */

/* The scrambling sequence of x^7 + x^4 + 1 is the register's history:
 * s[n] = s[n-7] xor s[n-4]. The register x1..x7 (x1 the most significant bit
 * of a state) holds the seven latest bits, x_j = s[1-j]: the state 1011101
 * holds s[0], s[-1], ..., s[-6], and the first output s[1] is x7 xor x4.
 * h[k] below is s[k - 6], so the history runs h[0] = s[-6] .. h[13] = s[7].
 *
 * The sequence itself is the library's `pn` (Galois, polynomial 0x48, seven
 * bits): any seed gives this recurrence, and the 127 seeds give the 127
 * non-zero first-seven-bit prefixes, so the seed for a wanted prefix is the
 * one whose first seven chips are it. The state and the prefix are related
 * by the recurrence run forward and backward; the prefix and the pn seed are
 * related by a fixed linear map over GF(2) (SCR_SEED_OF_PREFIX_BIT). */

#define SCR_POLY 0x48u

/* The first seven sequence bits s[1]..s[7], s[1] the most significant, from a
 * register state. */
static unsigned
scr_prefix_from_state (unsigned state)
{
  unsigned h[14];
  for (unsigned j = 1; j <= 7; j++)
    h[7 - j] = (state >> (7 - j)) & 1u; /* x_j = s[1-j] = h[7-j] */
  for (unsigned k = 7; k <= 13; k++)    /* s[n] = s[n-7] xor s[n-4] */
    h[k] = h[k - 7] ^ h[k - 4];
  unsigned v = 0;
  for (unsigned k = 7; k <= 13; k++)
    v = (v << 1) | h[k];
  return v;
}

uint8_t
dp_wlan_scrambler_state_from_sequence (uint8_t seq7)
{
  if (seq7 == 0 || seq7 > 127)
    return 0;
  unsigned h[14];
  for (unsigned k = 7; k <= 13;
       k++) /* s[1]..s[7], s[1] the most significant */
    h[k] = (seq7 >> (13 - k)) & 1u;
  for (int k = 6; k >= 0; k--) /* the recurrence backward: s[n-7] */
    h[k] = h[k + 7] ^ h[k + 3];
  unsigned state = 0;
  for (unsigned j = 1; j <= 7; j++)
    state |= h[7 - j] << (7 - j);
  return (uint8_t)state;
}

/* The pn seed whose first seven chips are a wanted prefix. The first seven
 * chips are a linear (over GF(2)) and invertible function of the seed, so the
 * inverse is linear too: the seed for a prefix is the XOR of the seeds of its
 * set bits. The seven below are the seeds of the unit prefixes, found once by
 * searching all 127 seeds, and the test pins the whole map against that
 * search for every one of the 127 prefixes. */
static const uint8_t SCR_SEED_OF_PREFIX_BIT[7] = { 64, 32, 16, 8, 68, 34, 17 };

size_t
dp_wlan_scrambler_sequence (uint8_t state, size_t n, uint8_t *out)
{
  if (state == 0 || state > 127)
    return 0;
  const unsigned want = scr_prefix_from_state (state);
  unsigned       seed = 0;
  for (int b = 0; b < 7; b++)
    if ((want >> b) & 1u)
      seed ^= SCR_SEED_OF_PREFIX_BIT[b];
  dp_pn_state_t *p = dp_pn_create (SCR_POLY, seed, 7, PN_GALOIS);
  if (!p)
    return 0;
  dp_pn_generate (p, n, out, n);
  dp_pn_destroy (p);
  return n;
}

/* ── The minimal MAC ─────────────────────────────────────────────────── */

size_t
dp_wlan_mpdu_len (dp_wlan_mac_t mode, size_t header_len, size_t body_len)
{
  size_t n;
  switch (mode)
    {
    case WLAN_MAC_NONE:
      n = body_len;
      break;
    case WLAN_MAC_FCS:
      n = body_len + 4;
      break;
    case WLAN_MAC_DATA:
      n = header_len + body_len + 4;
      break;
    default:
      return 0;
    }
  return (n >= 1 && n <= WLAN_PSDU_MAX) ? n : 0;
}

size_t
dp_wlan_mpdu (dp_wlan_mac_t mode, const uint8_t *header, size_t header_len,
              const uint8_t *body, size_t body_len, uint8_t *out,
              size_t out_cap)
{
  const size_t n = dp_wlan_mpdu_len (mode, header_len, body_len);
  if (n == 0 || n > out_cap)
    return 0;
  size_t off = 0;
  if (mode == WLAN_MAC_DATA)
    {
      memcpy (out, header, header_len);
      off = header_len;
    }
  memcpy (out + off, body, body_len);
  off += body_len;
  if (mode != WLAN_MAC_NONE)
    {
      const dp_crc_t model = DP_CRC_32_ISO_HDLC;
      const uint64_t fcs   = dp_crc_bytes (&model, out, off);
      for (int i = 0; i < 4; i++) /* least significant octet first */
        out[off + (size_t)i] = (uint8_t)(fcs >> (8 * i));
    }
  return n;
}

/* ── The coding chain ────────────────────────────────────────────────── */

size_t
dp_wlan_signal_bits (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                     size_t length, uint8_t *out)
{
  if (length < 1 || length > WLAN_PSDU_MAX || !t || !t->signal_rates)
    return 0;
  int rate_bits = -1;
  for (size_t i = 0; i < t->n_signal_rates; i++)
    if (t->signal_rates[i].rate.n_bpsc == r->n_bpsc
        && t->signal_rates[i].rate.code == r->code)
      rate_bits = t->signal_rates[i].bits;
  if (rate_bits < 0 || rate_bits > 15)
    return 0;
  memset (out, 0, WLAN_SIG_BITS);
  for (int i = 0; i < 4; i++) /* R1 first (Figure 17-5) */
    out[i] = (uint8_t)((rate_bits >> (3 - i)) & 1);
  /* out[4] is the reserved bit, 0 */
  for (int i = 0; i < 12; i++) /* LENGTH, least significant bit first */
    out[5 + i] = (uint8_t)((length >> i) & 1u);
  unsigned parity = 0;
  for (int i = 0; i < 17; i++)
    parity ^= out[i];
  out[17] = (uint8_t)parity; /* even parity over the first 17 */
  /* out[18..23] are the six tail bits, 0 */
  return WLAN_SIG_BITS;
}

size_t
dp_wlan_data_bits (const dp_wlan_rate_t *r, const uint8_t *psdu, size_t len,
                   uint8_t state, uint8_t *out, size_t cap)
{
  const size_t n_sym = dp_wlan_rate_n_sym (r, len);
  if (n_sym == 0 || state == 0 || state > 127)
    return 0;
  const size_t total = n_sym * dp_wlan_n_dbps (r);
  if (total > cap)
    return 0;
  memset (out, 0, total);
  for (size_t i = 0; i < len; i++)
    for (int b = 0; b < 8; b++) /* each octet least significant bit first */
      out[WLAN_SERVICE_BITS + 8 * i + (size_t)b]
          = (uint8_t)((psdu[i] >> b) & 1u);
  uint8_t *seq = dp_xmalloc (total);
  dp_wlan_scrambler_sequence (state, total, seq);
  for (size_t i = 0; i < total; i++)
    out[i] ^= seq[i];
  free (seq);
  /* the six tail bits are reset to zero after scrambling */
  memset (out + WLAN_SERVICE_BITS + 8 * len, 0, WLAN_TAIL_BITS);
  return total;
}

size_t
dp_wlan_encode (dp_wlan_code_t code, const uint8_t *in, size_t n, uint8_t *out,
                size_t cap)
{
  conv_code_t c;
  if (!dp_wlan_bcc (code, &c))
    return 0;
  conv_enc_t enc;
  dp_conv_enc_init (&enc);
  return dp_conv_encode (&enc, &c, in, n, out, cap);
}

size_t
dp_wlan_interleave (const dp_wlan_tables_t *t, unsigned n_bpsc,
                    const uint8_t *in, size_t n, uint8_t *out)
{
  if (!t || !t->perm2
      || !(n_bpsc == 1 || n_bpsc == 2 || n_bpsc == 4 || n_bpsc == 6))
    return 0;
  const size_t n_cbps = (size_t)WLAN_N_SD * n_bpsc;
  if (n == 0 || n % n_cbps != 0)
    return 0;
  /* the composite permutation of one symbol, checked to be a permutation: the
   * second one is the caller's data */
  size_t  *dest = dp_xmallocn (n_cbps, sizeof *dest);
  uint8_t *seen = dp_xcalloc (n_cbps, 1);
  int      ok   = 1;
  for (size_t k = 0; k < n_cbps && ok; k++)
    {
      const size_t i = dp_interleave_index (k, n_cbps / 16, 16);
      const size_t j = t->perm2 (i, n_cbps, n_bpsc);
      if (j >= n_cbps || seen[j])
        ok = 0;
      else
        {
          seen[j] = 1;
          dest[k] = j;
        }
    }
  if (ok)
    for (size_t s = 0; s < n / n_cbps; s++)
      for (size_t k = 0; k < n_cbps; k++)
        out[s * n_cbps + dest[k]] = in[s * n_cbps + k];
  free (dest);
  free (seen);
  return ok ? n : 0;
}

size_t
dp_wlan_map (const dp_wlan_tables_t *t, unsigned n_bpsc, const uint8_t *bits,
             size_t n, float _Complex *out)
{
  if (!(n_bpsc == 1 || n_bpsc == 2 || n_bpsc == 4 || n_bpsc == 6) || n == 0
      || n % n_bpsc != 0)
    return 0;
  const size_t    n_sym   = n / n_bpsc;
  const uint16_t *relabel = t ? t->relabel[n_bpsc] : NULL;
  uint16_t       *sym     = dp_xmallocn (n_sym, sizeof *sym);
  for (size_t s = 0; s < n_sym; s++)
    {
      unsigned v = 0;
      for (unsigned b = 0; b < n_bpsc; b++) /* the first bit is bit 0 */
        v |= (unsigned)(bits[s * n_bpsc + b] & 1u) << b;
      sym[s] = (uint16_t)(relabel ? relabel[v] : v);
    }
  if (n_bpsc <= 2)
    {
      uint8_t *s8 = dp_xmalloc (n_sym);
      for (size_t s = 0; s < n_sym; s++)
        s8[s] = (uint8_t)sym[s];
      dp_mpsk_map (s8, n_sym, out, n_bpsc == 1 ? 2 : 4);
      free (s8);
    }
  else
    dp_qam_map (sym, n_sym, out, n_bpsc == 4 ? 16 : 64);
  free (sym);
  return n_sym;
}

/* One field's points: bits -> BCC -> interleave -> map. */
static size_t
field_points (const dp_wlan_tables_t *t, dp_wlan_code_t code, unsigned n_bpsc,
              const uint8_t *bits, size_t n, float _Complex *out)
{
  const size_t coded_cap = n * 2;
  uint8_t     *coded     = dp_xmalloc (coded_cap);
  uint8_t     *inter     = dp_xmalloc (coded_cap);
  size_t       got       = 0;
  const size_t nc        = dp_wlan_encode (code, bits, n, coded, coded_cap);
  if (nc > 0 && dp_wlan_interleave (t, n_bpsc, coded, nc, inter) == nc)
    got = dp_wlan_map (t, n_bpsc, inter, nc, out);
  free (coded);
  free (inter);
  return got;
}

size_t
dp_wlan_points (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                const uint8_t *psdu, size_t len, uint8_t state,
                float _Complex *out, size_t cap)
{
  const size_t n_sym = dp_wlan_rate_n_sym (r, len);
  if (n_sym == 0 || cap < WLAN_N_SD + (size_t)WLAN_N_SD * n_sym)
    return 0;
  uint8_t sig[WLAN_SIG_BITS];
  if (dp_wlan_signal_bits (t, r, len, sig) != WLAN_SIG_BITS)
    return 0;
  /* the SIGNAL field: BPSK at rate 1/2, not scrambled */
  if (field_points (t, WLAN_CODE_1_2, 1, sig, WLAN_SIG_BITS, out) != WLAN_N_SD)
    return 0;
  const size_t total = n_sym * dp_wlan_n_dbps (r);
  uint8_t     *bits  = dp_xmalloc (total);
  size_t       got   = 0;
  if (dp_wlan_data_bits (r, psdu, len, state, bits, total) == total)
    {
      const size_t n
          = field_points (t, r->code, r->n_bpsc, bits, total, out + WLAN_N_SD);
      got = (n == WLAN_N_SD * n_sym) ? WLAN_N_SD + n : 0;
    }
  free (bits);
  return got;
}

/* ── The layout and the PPDU ─────────────────────────────────────────── */

int
dp_wlan_layout (dp_ofdm_state_t *o, const dp_wlan_tables_t *t, size_t n_sym,
                unsigned window, unsigned oversample)
{
  if (!t || !t->stf || !t->ltf || t->scale_stf <= 0.0 || t->scale_ltf <= 0.0
      || t->scale_data <= 0.0 || n_sym == 0 || n_sym > WLAN_N_SYM_MAX
      || oversample < 1 || oversample > WLAN_OVERSAMPLE_MAX
      || !wlan_window_ok (window, oversample))
    return -1;
  const uint32_t       m        = oversample;
  const uint32_t       n_fft    = WLAN_N_FFT * m;
  uint8_t             *null_map = dp_xcalloc (n_fft, 1);
  uint8_t             *data_map = dp_xcalloc (n_fft, 1);
  static const int32_t pk[4]    = WLAN_PILOT_K;
  /* every subcarrier but DC may carry data; the four pilots, added below as
   * fixed entries, displace data on their subcarriers, leaving 48 */
  for (int32_t k = -26; k <= 26; k++)
    if (k != 0)
      data_map[(size_t)(k + (int32_t)n_fft / 2)] = 1;
  /* L-STF: ten short periods read as 160 * M samples of the 64 * M-point
   * transform, no prefix; L-LTF: 128 * M samples (two long periods) behind a
   * 32 * M-sample prefix; SIGNAL and Data share one class so the pilot
   * polarity runs on. Every length is M times its 20 MHz value, and the
   * scale is unchanged: the transform is unnormalized, so the same waveform
   * is sampled M times as finely. */
  const int stf = dp_ofdm_add_class (
      o, n_fft, null_map, n_fft, WLAN_STF_LEN * m, 0, window, t->scale_stf, 1);
  const int ltf = dp_ofdm_add_class (o, n_fft, null_map, n_fft, 128 * m,
                                     32 * m, window, t->scale_ltf, 1);
  const int sym = dp_ofdm_add_class (o, n_fft, data_map, n_fft, n_fft,
                                     WLAN_GI * m, window, t->scale_data, 1);
  free (null_map);
  free (data_map);
  if (stf < 0 || ltf < 0 || sym < 0)
    return -1;
  for (int32_t k = -26; k <= 26; k++)
    {
      const float _Complex vs = t->stf[k + 26], vl = t->ltf[k + 26];
      if (vs != 0.0f
          && dp_ofdm_add_fixed (o, (uint32_t)stf, OFDM_ALL_PHASES, k, vs) != 0)
        return -1;
      if (vl != 0.0f
          && dp_ofdm_add_fixed (o, (uint32_t)ltf, OFDM_ALL_PHASES, k, vl) != 0)
        return -1;
    }
  static const double pv[4] = WLAN_PILOT_V;
  for (int i = 0; i < 4; i++)
    if (dp_ofdm_add_fixed (o, (uint32_t)sym, OFDM_ALL_PHASES, pk[i], pv[i])
        != 0)
      return -1;
  /* the pilot polarity: the scrambler's sequence from the all-ones state,
   * with 0 read as +1 and 1 as -1 */
  uint8_t seq[WLAN_POLARITY_LEN];
  float _Complex pol[WLAN_POLARITY_LEN];
  dp_wlan_scrambler_sequence (127, WLAN_POLARITY_LEN, seq);
  for (size_t i = 0; i < WLAN_POLARITY_LEN; i++)
    pol[i] = seq[i] ? -1.0f : 1.0f;
  if (dp_ofdm_add_polarity (o, (uint32_t)sym, pol, WLAN_POLARITY_LEN) != 0)
    return -1;
  if (dp_ofdm_add_run (o, (uint32_t)stf, 1, OFDM_CP_DEFAULT, 0, 0) < 0
      || dp_ofdm_add_run (o, (uint32_t)ltf, 1, OFDM_CP_DEFAULT, 0, 0) < 0
      || dp_ofdm_add_run (o, (uint32_t)sym, 1, OFDM_CP_DEFAULT, 0, 0) < 0
      || dp_ofdm_add_run (o, (uint32_t)sym, (uint32_t)n_sym, OFDM_CP_DEFAULT,
                          1, 0)
             < 0)
    return -1;
  return dp_ofdm_check (o);
}

size_t
dp_wlan_ppdu (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
              const uint8_t *psdu, size_t len, uint8_t state, unsigned window,
              unsigned oversample, float _Complex *out, size_t cap)
{
  const size_t n_sym = dp_wlan_rate_n_sym (r, len);
  if (n_sym == 0)
    return 0;
  const size_t n_samples = dp_wlan_ppdu_len (n_sym, window, oversample);
  if (n_samples == 0 || cap < n_samples)
    return 0;
  const size_t    n_pts = WLAN_N_SD + (size_t)WLAN_N_SD * n_sym;
  float _Complex *pts   = dp_xmallocn (n_pts, sizeof *pts);
  size_t          got   = 0;
  if (dp_wlan_points (t, r, psdu, len, state, pts, n_pts) == n_pts)
    {
      dp_ofdm_state_t *o = dp_ofdm_create ();
      if (dp_wlan_layout (o, t, n_sym, window, oversample) == 0)
        {
          const size_t a = dp_ofdm_modulate (o, pts, n_pts, out, n_samples);
          const size_t b
              /* o->tail_len is read from the modulator's public struct: the
               * modulator has no getter for it (ofdm_core.h) */
              = a ? dp_ofdm_finish (o, o->tail_len, out + a, n_samples - a)
                  : 0;
          if (a + b == n_samples)
            got = n_samples;
        }
      dp_ofdm_destroy (o);
    }
  free (pts);
  return got;
}
