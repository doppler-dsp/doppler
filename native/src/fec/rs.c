/*
 * rs.c — CCSDS Reed-Solomon (255,223), the E=16 code of 131.0-B-3 section 4.3.
 *
 * The arithmetic is ordinary GF(2^8) Reed-Solomon. What is not ordinary, and
 * what this file exists to get right, is which field, which roots, and which
 * basis — see fec_rs.h. Each of the three is a choice a reader would make
 * differently from habit, and each produces a self-consistent code that no
 * CCSDS receiver can decode.
 */
#include "fec/fec_rs.h"

/* 4.3.3: F(x) = x^8 + x^7 + x^2 + x + 1. Held as the low eight bits, the
 * x^8 term being implicit in the reduction. */
#define FIELD_POLY 0x87u

/* 4.3.4: the roots are a^(11j). 11 rather than 1 is the whole point. */
#define ROOT_STRIDE 11
/* j runs 128-E .. 127+E; with E=16 that is 112..143. */
#define ROOT_FIRST_J (128 - 16)

/* 4.3.9.3, first equation, one row per u bit from u7 down to u0. Each row is
 * packed with z0 in bit 7, matching 4.3.9.2's transmission order. */
static const uint8_t T_CONV_TO_DUAL[8]
    = { 0x8Du, 0xEFu, 0xECu, 0x86u, 0xFAu, 0x99u, 0xAFu, 0x7Bu };

/* 4.3.9.3, second equation, one row per z bit from z0 to z7, packed with u7
 * in bit 7. */
static const uint8_t T_DUAL_TO_CONV[8]
    = { 0xC5u, 0x42u, 0x2Eu, 0xFDu, 0xF0u, 0x79u, 0xACu, 0xCCu };

static uint8_t exp_tab[512];
static uint8_t log_tab[256];
static uint8_t gen[FEC_RS_2E + 1];
static int     ready = 0;

static uint8_t
gf_mul (uint8_t a, uint8_t b)
{
  if (a == 0 || b == 0)
    return 0;
  return exp_tab[log_tab[a] + log_tab[b]];
}

static void
build (void)
{
  /* a = x = 0x02 generates the field: 4.3.4's note that a^11 is primitive
   * requires a to be primitive too, since gcd(11, 255) = 1. */
  unsigned v = 1;
  for (int i = 0; i < 255; i++)
    {
      exp_tab[i]          = (uint8_t)v;
      log_tab[(uint8_t)v] = (uint8_t)i;
      v <<= 1;
      if (v & 0x100u)
        v ^= 0x100u | FIELD_POLY;
    }
  for (int i = 255; i < 512; i++)
    exp_tab[i] = exp_tab[i - 255];

  /* g(x) = prod_{j} (x - a^(11j)). Over GF(2) subtraction is addition, so
   * each factor is (x + root) and the product is built by convolution. */
  gen[0]  = 1;
  int deg = 0;
  for (int m = 0; m < FEC_RS_2E; m++)
    {
      const uint8_t root = exp_tab[(ROOT_STRIDE * (ROOT_FIRST_J + m)) % 255];
      /* multiply the current gen(x) by (x + root), high term first so the
         in-place update never reads a coefficient it has already written */
      gen[deg + 1] = gen[deg];
      for (int i = deg; i > 0; i--)
        gen[i] = (uint8_t)(gen[i - 1] ^ gf_mul (gen[i], root));
      gen[0] = gf_mul (gen[0], root);
      deg++;
    }
  ready = 1;
}

static void
ensure (void)
{
  if (!ready)
    build ();
}

uint8_t
fec_rs_conv_to_dual (uint8_t u)
{
  uint8_t z = 0;
  for (int i = 0; i < 8; i++)
    {
      if ((u >> (7 - i)) & 1u)
        z ^= T_CONV_TO_DUAL[i];
    }
  return z;
}

uint8_t
fec_rs_dual_to_conv (uint8_t z)
{
  uint8_t u = 0;
  for (int i = 0; i < 8; i++)
    {
      if ((z >> (7 - i)) & 1u)
        u ^= T_DUAL_TO_CONV[i];
    }
  return u;
}

const uint8_t *
fec_rs_generator (void)
{
  ensure ();
  return gen;
}

void
fec_rs_encode (const uint8_t *info, uint8_t *parity)
{
  ensure ();

  /* Figure F-1: transform in, encode conventionally, transform out. */
  uint8_t reg[FEC_RS_2E] = { 0 };

  for (int i = 0; i < FEC_RS_K; i++)
    {
      const uint8_t u  = fec_rs_dual_to_conv (info[i]);
      const uint8_t fb = (uint8_t)(u ^ reg[FEC_RS_2E - 1]);

      for (int j = FEC_RS_2E - 1; j > 0; j--)
        reg[j] = (uint8_t)(reg[j - 1] ^ gf_mul (fb, gen[j]));
      reg[0] = gf_mul (fb, gen[0]);
    }

  /* The register holds the remainder, highest-order coefficient last; parity
     is transmitted highest-order first, immediately after the information. */
  for (int i = 0; i < FEC_RS_2E; i++)
    parity[i] = fec_rs_conv_to_dual (reg[FEC_RS_2E - 1 - i]);
}

int
fec_rs_codeword_ok (const uint8_t *codeword)
{
  ensure ();

  /* S_m = C(a^(11 * (ROOT_FIRST_J + m))), evaluated by Horner over the
   * conventional-basis symbols. Zero for every root is what "is a codeword"
   * means, independently of how the parity was produced. */
  for (int m = 0; m < FEC_RS_2E; m++)
    {
      const uint8_t root = exp_tab[(ROOT_STRIDE * (ROOT_FIRST_J + m)) % 255];
      uint8_t       acc  = 0;
      for (int i = 0; i < FEC_RS_N; i++)
        acc = (uint8_t)(gf_mul (acc, root)
                        ^ fec_rs_dual_to_conv (codeword[i]));
      if (acc != 0)
        return 0;
    }
  return 1;
}

size_t
fec_rs_encode_block (const uint8_t *info, unsigned depth, uint8_t *out)
{
  /* 4.3.5.1 enumerates the allowed depths, and this refuses anything else
   * rather than quietly encoding a block no receiver is configured for. */
  if (depth != 1 && depth != 2 && depth != 3 && depth != 4 && depth != 5
      && depth != 8)
    return 0;

  const size_t k_syms = (size_t)FEC_RS_K * depth;

  /* 4.4.1: S2 reassembles the information symbols "in the same way as they
   * entered", so the information section is a straight copy. Only the check
   * symbols are rearranged. */
  for (size_t i = 0; i < k_syms; i++)
    out[i] = info[i];

  for (unsigned e = 0; e < depth; e++)
    {
      /* S1 gives encoder e every depth-th symbol, starting at e. */
      uint8_t word[FEC_RS_K];
      for (int i = 0; i < FEC_RS_K; i++)
        word[i] = info[(size_t)i * depth + e];

      uint8_t parity[FEC_RS_2E];
      fec_rs_encode (word, parity);

      /* ...and S2 samples the encoders in the same rotation on the way out. */
      for (int p = 0; p < FEC_RS_2E; p++)
        out[k_syms + (size_t)p * depth + e] = parity[p];
    }

  return (size_t)FEC_RS_N * depth;
}
