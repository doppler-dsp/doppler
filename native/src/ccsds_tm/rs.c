/*
 * rs.c — the CCSDS configuration of the outer code: 131.0-B-3 section 4.3.
 *
 * The arithmetic is `rs/rs_core.h`'s and none of it is here. What IS here is
 * everything 131.0-B-3 adds that is not a property of the code: which field,
 * which roots, which basis, and the interleaver. Each of the first three is a
 * choice a reader would make differently from habit, and each produces a
 * self-consistent code that no CCSDS receiver can decode — which is why they
 * are a configuration a test can hold to Annex G rather than constants inside
 * an encoder.
 */
#include "ccsds_tm/ccsds_tm_rs.h"

#include <pthread.h>

/* 4.3.3: F(x) = x^8 + x^7 + x^2 + x + 1, held as the low eight bits, the x^8
 * term being implicit in the reduction. 4.3.4: the roots are a^(11j) with j
 * running 128-E .. 127+E. 11 rather than 1 is the whole point. */
const rs_code_t CCSDS_TM_RS = { .symbol_bits = 8,
                                .field_poly  = 0x87u,
                                .nroots      = CCSDS_TM_RS_2E,
                                .first_root  = 128u - CCSDS_TM_RS_E,
                                .root_stride = 11u };

/* 4.3.9.3, first equation, one row per u bit from u7 down to u0. Each row is
 * packed with z0 in bit 7, matching 4.3.9.2's transmission order. */
static const uint8_t T_CONV_TO_DUAL[8]
    = { 0x8Du, 0xEFu, 0xECu, 0x86u, 0xFAu, 0x99u, 0xAFu, 0x7Bu };

/* 4.3.9.3, second equation, one row per z bit from z0 to z7, packed with u7
 * in bit 7. */
static const uint8_t T_DUAL_TO_CONV[8]
    = { 0xC5u, 0x42u, 0x2Eu, 0xFDu, 0xF0u, 0x79u, 0xACu, 0xCCu };

/* The field tables are derived once, on first use, and every public entry
 * point below goes through `ensure()` to get them.
 *
 * `pthread_once` rather than a `ready` flag, because the flag version was a
 * DATA RACE and not a benign double-initialisation: two threads reaching any
 * entry point first would both see `ready == 0`, both call `rs_init`, and --
 * the part that makes it undefined rather than merely wasteful -- one could
 * read the half-written tables the other was still filling (gh-817).
 *
 * It was unreachable when written, which is why it survived: nothing called
 * the encoder. Two things already in the tree make the first call the racy
 * one. `dp_parallel.h` fans independent per-source signal builds across
 * cores, and a coded source is a per-source build; and every block method in
 * this project declares `nogil = true`, so a Python encoder driven from a
 * thread pool is the same race with a different scheduler. A first call is
 * exactly what a freshly imported module makes.
 *
 * Precomputing the tables as `static const` would also be thread-safe, and
 * was rejected: it moves g(x) from something DERIVED to something
 * transcribed, and the derivation is what `test_ccsds_tm_rs` holds to Annex
 * G. Thread safety should not cost the evidence.
 *
 * POSIX-only, which matches `[project] platforms = ["linux", "macos"]`. */
static rs_t           ccsds;
static pthread_once_t ccsds_once = PTHREAD_ONCE_INIT;

static void
ccsds_build (void)
{
  rs_init (&ccsds, &CCSDS_TM_RS);
}

static const rs_t *
ensure (void)
{
  pthread_once (&ccsds_once, ccsds_build);
  return &ccsds;
}

uint8_t
ccsds_tm_rs_conv_to_dual (uint8_t u)
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
ccsds_tm_rs_dual_to_conv (uint8_t z)
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
ccsds_tm_rs_generator (void)
{
  return rs_generator (ensure ());
}

void
ccsds_tm_rs_encode (const uint8_t *info, uint8_t *parity)
{
  const rs_t *rs = ensure ();

  /* Figure F-1: transform in, encode conventionally, transform out. */
  uint8_t conv[CCSDS_TM_RS_K];
  for (int i = 0; i < CCSDS_TM_RS_K; i++)
    conv[i] = ccsds_tm_rs_dual_to_conv (info[i]);

  uint8_t check[CCSDS_TM_RS_2E];
  rs_encode (rs, conv, check);

  for (int i = 0; i < CCSDS_TM_RS_2E; i++)
    parity[i] = ccsds_tm_rs_conv_to_dual (check[i]);
}

int
ccsds_tm_rs_codeword_ok (const uint8_t *codeword)
{
  const rs_t *rs = ensure ();

  uint8_t conv[CCSDS_TM_RS_N];
  for (int i = 0; i < CCSDS_TM_RS_N; i++)
    conv[i] = ccsds_tm_rs_dual_to_conv (codeword[i]);

  return rs_codeword_ok (rs, conv);
}

int
ccsds_tm_rs_decode (uint8_t *codeword)
{
  const rs_t *rs = ensure ();

  /* 4.3.9: the wire carries dual-basis symbols and the algebra is
     conventional. Correcting in the transmitted basis would produce a
     decoder that repairs its own encoder's output perfectly and nothing
     else, which is the failure this whole file exists to prevent. */
  uint8_t conv[CCSDS_TM_RS_N];
  for (int i = 0; i < CCSDS_TM_RS_N; i++)
    conv[i] = ccsds_tm_rs_dual_to_conv (codeword[i]);

  const int fixed = rs_decode (rs, conv);
  if (fixed <= 0)
    return fixed;

  for (int i = 0; i < CCSDS_TM_RS_N; i++)
    codeword[i] = ccsds_tm_rs_conv_to_dual (conv[i]);
  return fixed;
}

/* 4.3.5.1 enumerates the allowed depths; anything else is refused rather
 * than quietly coded as a block no receiver is configured for. */
static int
depth_ok (unsigned depth)
{
  return depth == 1 || depth == 2 || depth == 3 || depth == 4 || depth == 5
         || depth == 8;
}

size_t
ccsds_tm_rs_encode_block (const uint8_t *info, unsigned depth, uint8_t *out)
{
  if (!depth_ok (depth))
    return 0;

  const size_t k_syms = (size_t)CCSDS_TM_RS_K * depth;

  /* 4.4.1: S2 reassembles the information symbols "in the same way as they
     entered", so the information section is a straight copy. Only the check
     symbols are rearranged. */
  for (size_t i = 0; i < k_syms; i++)
    out[i] = info[i];

  for (unsigned e = 0; e < depth; e++)
    {
      /* S1 gives encoder e every depth-th symbol, starting at e. */
      uint8_t word[CCSDS_TM_RS_K];
      for (int i = 0; i < CCSDS_TM_RS_K; i++)
        word[i] = info[(size_t)i * depth + e];

      uint8_t parity[CCSDS_TM_RS_2E];
      ccsds_tm_rs_encode (word, parity);

      /* ...and S2 samples the encoders in the same rotation on the way out. */
      for (int p = 0; p < CCSDS_TM_RS_2E; p++)
        out[k_syms + (size_t)p * depth + e] = parity[p];
    }

  return (size_t)CCSDS_TM_RS_N * depth;
}

size_t
ccsds_tm_rs_decode_block (uint8_t *block, unsigned depth,
                          ccsds_tm_rs_block_rx_t *rx)
{
  if (!depth_ok (depth))
    return 0;

  const size_t           k_syms = (size_t)CCSDS_TM_RS_K * depth;
  ccsds_tm_rs_block_rx_t out    = { depth, 0u, 0u, 0u };

  for (unsigned e = 0; e < depth; e++)
    {
      /* Undo S1/S2: encoder e saw every depth-th symbol starting at e, in
         both sections. This is the same rotation ccsds_tm_rs_encode_block
         wrote, read from the one description rather than a second one. */
      uint8_t word[CCSDS_TM_RS_N];
      for (int i = 0; i < CCSDS_TM_RS_K; i++)
        word[i] = block[(size_t)i * depth + e];
      for (int p = 0; p < CCSDS_TM_RS_2E; p++)
        word[CCSDS_TM_RS_K + p] = block[k_syms + (size_t)p * depth + e];

      const int fixed = ccsds_tm_rs_decode (word);
      if (fixed < 0)
        {
          out.uncorrectable++;
          continue;
        }
      if (fixed == 0)
        continue;

      out.corrected++;
      out.symbols += (unsigned)fixed;

      /* Only a repaired codeword is written back, so a block that decodes
         clean is not rewritten symbol by symbol. */
      for (int i = 0; i < CCSDS_TM_RS_K; i++)
        block[(size_t)i * depth + e] = word[i];
      for (int p = 0; p < CCSDS_TM_RS_2E; p++)
        block[k_syms + (size_t)p * depth + e] = word[CCSDS_TM_RS_K + p];
    }

  if (rx != NULL)
    *rx = out;
  return k_syms;
}
