/**
 * @file interleave_burst_gain.c
 * @brief Validation: interleaving converts a BURST an outer code cannot
 * correct into spread errors it can.
 *
 * This is the claim the interleaver exists for, and it is the one a
 * round-trip test cannot make. `test_dp_interleave.c` proves the permutation
 * inverts; that says nothing about why anyone would apply it. What follows
 * measures the thing a link budget is written against: frame error rate
 * against burst length, with and without the interleaver, over a real
 * Reed-Solomon outer code.
 *
 * The setup is the one an interleaver is specified for. A CCSDS RS(255,223)
 * codeword corrects E = 16 symbol errors. Interleaving depth I spreads a
 * burst of up to I consecutive OCTETS one per codeword, so the corrigible
 * burst grows from E octets (all inside one codeword) to E*I.
 *
 * Two predictions, and the second is what makes the first mean something:
 *
 *   1. WITHOUT the interleaver, a burst longer than E octets that lands
 *      inside one codeword is uncorrectable -- FER goes to 1.
 *   2. WITH depth-I interleaving at unit_bits = 8, the same burst is
 *      correctable up to E*I octets, and NOT beyond.
 *
 * The second bound is the honest half. An interleaver does not make a link
 * immune to bursts; it multiplies the length it survives by exactly the
 * depth, and a validation that only showed the good case would be claiming
 * something stronger than the code does.
 *
 * The UNIT is measured too, because it is the parameter most easily got
 * wrong: bit-interleaving an octet-oriented code spreads a burst WITHIN
 * symbols that are already wrong, so it buys nothing at all. That is
 * asserted here rather than only argued in a header.
 *
 * Usage:  interleave_burst_gain [--check]
 */
#include "ccsds_tm/ccsds_tm_rs.h"
#include "dp_interleave.h"
#include "dp_rng_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define E_CORRECT 16 /* RS(255,223) corrects this many symbol errors */

/* One trial: encode N_CW independent RS codewords, put the burst on the WIRE,
 * and report whether every codeword's information came back exactly.
 *
 * The arrangement matters and the first draft of this file got it wrong, so
 * it is worth stating. Interleaving a SINGLE codeword buys nothing at all:
 * Reed-Solomon corrects any E symbol errors wherever they fall, so permuting
 * them inside one codeword changes nothing a decoder can see. The gain only
 * exists when there are several codewords to spread a burst ACROSS.
 *
 * That is also the difference between this and the outer code's own depth.
 * `ccsds_tm_rs_encode_block(depth)` interleaves `depth` codewords and gets
 * exactly this property for free -- its header says so. A block interleaver
 * is the general form: it works over whatever span it is given, including
 * many codeblocks and codes with no interleaving of their own, and it is a
 * separate stage rather than a property of one code.
 *
 * One codeword per ROW is the classic arrangement, and it is what
 * `dp_interleave.h` describes: reading by columns transmits one symbol from
 * each codeword in turn, so a burst of up to N_CW consecutive wire symbols
 * costs each codeword at most one.
 *
 * Returns 1 for a frame error -- ANY codeword failing loses the frame. */
#define N_CW 5 /* codewords interleaved; corrigible burst becomes E*N_CW */

static int
trial (unsigned n_cw, unsigned interleaved, size_t unit_bits, size_t burst,
       size_t burst_at, unsigned seed)
{
  const size_t k_syms = (size_t)CCSDS_TM_RS_K; /* per codeword    */
  const size_t n_syms = (size_t)CCSDS_TM_RS_N; /* per codeword    */
  const size_t total  = n_syms * n_cw;         /* on the wire     */
  const size_t k_tot  = k_syms * n_cw;

  uint8_t *info = malloc (k_tot);
  uint8_t *tx   = malloc (total);
  uint8_t *rx   = malloc (total);
  if (!info || !tx || !rx)
    {
      free (info);
      free (tx);
      free (rx);
      return 1;
    }

  /* dp_rng_test.h is the suite's ONE random source -- the single point where
     one edit moves every BER and FER number at once. A private generator here
     would be a second one that drifts. */
  uint32_t st = seed * 2654435761u + 1u;
  for (size_t i = 0; i < k_tot; i++)
    info[i] = (uint8_t)(dp_xs32 (&st) >> 16);

  /* Each codeword encoded ALONE (depth 1), so the only burst protection in
     play is the interleaver under test and not the outer code's own. */
  for (unsigned c = 0; c < n_cw; c++)
    if (ccsds_tm_rs_encode_block (info + (size_t)c * k_syms, 1u,
                                  tx + (size_t)c * n_syms)
        == 0)
      {
        free (info);
        free (tx);
        free (rx);
        return 1;
      }

  const size_t n_bits = total * 8u;
  uint8_t     *tb = malloc (n_bits), *pb = malloc (n_bits);
  if (!tb || !pb)
    {
      free (tb);
      free (pb);
      free (info);
      free (tx);
      free (rx);
      return 1;
    }
  for (size_t i = 0; i < n_bits; i++)
    tb[i] = (uint8_t)((tx[i / 8] >> (7 - i % 8)) & 1u);

  if (interleaved)
    {
      const size_t rows = n_cw;
      /* A geometry that does not divide would permute a PREFIX and leave a
         tail, and the inverse would not invert -- a wrong number that looks
         like a result. Loud rather than silent. */
      if ((n_bits % (rows * unit_bits)) != 0)
        {
          fprintf (stderr, "geometry does not divide\n");
          abort ();
        }
      dp_interleave_u8 (tb, pb, rows, n_bits / (rows * unit_bits), unit_bits);
    }
  else
    memcpy (pb, tb, n_bits);

  /* The burst goes on the WIRE, which is what the interleaver was applied
     to. Corrupting the codewords directly would measure nothing. */
  for (size_t b = 0; b < burst * 8u; b++)
    pb[(burst_at * 8u + b) % n_bits] ^= 1u;

  if (interleaved)
    {
      const size_t rows = n_cw;
      dp_deinterleave_u8 (pb, tb, rows, n_bits / (rows * unit_bits),
                          unit_bits);
    }
  else
    memcpy (tb, pb, n_bits);

  memset (rx, 0, total);
  for (size_t i = 0; i < n_bits; i++)
    rx[i / 8] = (uint8_t)(rx[i / 8] | (tb[i] << (7 - i % 8)));

  /* ANY codeword failing is a frame error. The memcmp is the verdict rather
     than the decoder's own report, because a decoder that "succeeded" while
     miscorrecting has still lost the frame and only the comparison sees it. */
  int bad = 0;
  for (unsigned c = 0; c < n_cw && !bad; c++)
    {
      uint8_t *cw = rx + (size_t)c * n_syms;
      if (ccsds_tm_rs_decode_block (cw, 1u, NULL) == 0)
        bad = 1;
      else if (memcmp (cw, info + (size_t)c * k_syms, k_syms) != 0)
        bad = 1;
    }

  free (tb);
  free (pb);
  free (info);
  free (tx);
  free (rx);
  return bad;
}

/* Frame error rate over a sweep of burst positions. */
static double
fer (unsigned n_cw, unsigned interleaved, size_t unit_bits, size_t burst,
     unsigned trials)
{
  const size_t total = (size_t)CCSDS_TM_RS_N * n_cw;
  unsigned     bad   = 0;
  for (unsigned t = 0; t < trials; t++)
    bad += (unsigned)trial (n_cw, interleaved, unit_bits, burst,
                            (size_t)t * 37u % total, t + 1u);
  return (double)bad / (double)trials;
}

int
main (int argc, char **argv)
{
  const int      check  = (argc > 1 && strcmp (argv[1], "--check") == 0);
  const unsigned NCW    = N_CW; /* codewords, one per interleaver row */
  const unsigned NONE   = 0u;
  const unsigned ILV    = 1u;
  const unsigned TRIALS = 24;
  int            fail   = 0;

  printf ("interleave burst gain — %d x RS(255,223), E=%d each,\n"
          "  one codeword per interleaver row\n",
          N_CW, E_CORRECT);
  printf ("  burst  FER(none)  FER(ilv,unit=8)  FER(ilv,unit=1)\n");

  /* Below the bare code's limit both must pass; above it, only the
     interleaved one, and only up to E*I. */
  const size_t bursts[] = { 8, 16, 17, 40, 80, 81 };
  double       f_none[6], f_u8[6], f_u1[6];
  for (unsigned i = 0; i < 6; i++)
    {
      f_none[i] = fer (NCW, NONE, 8, bursts[i], TRIALS);
      f_u8[i]   = fer (NCW, ILV, 8, bursts[i], TRIALS);
      f_u1[i]   = fer (NCW, ILV, 1, bursts[i], TRIALS);
      printf ("  %5zu  %8.3f  %15.3f  %15.3f\n", bursts[i], f_none[i], f_u8[i],
              f_u1[i]);
    }

  /* 1. A burst inside the code's own correcting power is fine either way --
        the control, without which "interleaving helped" could be measuring
        a code that never failed. */
  if (f_none[0] != 0.0 || f_u8[0] != 0.0)
    {
      printf ("FAIL: an %zu-octet burst is within E=%d and must always "
              "correct\n",
              bursts[0], E_CORRECT);
      fail = 1;
    }

  /* 2. Past E, the bare code fails and the interleaved one does not. This is
        the gain, and it is stated as BOTH halves: a validation showing only
        the second would not distinguish an interleaver from a code that
        never failed in the first place. */
  /* Not 1.0, and the reason is physical rather than statistical: a burst of
     E+1 octets that STRADDLES a codeword boundary splits into two runs of at
     most E, and both codewords correct. So the bare code fails most of the
     time and not always, and asserting 1.0 here would be asserting something
     untrue about where bursts land. At E*N it is 1.0, and that is asserted
     below. */
  if (f_none[2] < 0.5)
    {
      printf ("FAIL: a %zu-octet burst exceeds E=%d and must break the bare "
              "code most of the time (got %.3f)\n",
              bursts[2], E_CORRECT, f_none[2]);
      fail = 1;
    }
  if (f_none[4] < 1.0)
    {
      printf ("FAIL: a %zu-octet burst cannot fit under E=%d anywhere and "
              "must ALWAYS break the bare code\n",
              bursts[4], E_CORRECT);
      fail = 1;
    }
  if (f_u8[2] != 0.0 || f_u8[3] != 0.0)
    {
      printf ("FAIL: depth %u must spread a burst of %zu octets one per "
              "codeword\n",
              NCW, bursts[3]);
      fail = 1;
    }

  /* 3. And it stops where the arithmetic says. E*I = 64 octets is the last
        length depth I survives; 65 is not. An interleaver multiplies the
        corrigible burst by the depth, it does not remove the bound. */
  if (f_u8[4] != 0.0)
    {
      printf ("FAIL: E*I = %d octets must still correct\n",
              E_CORRECT * (int)NCW);
      fail = 1;
    }
  if (f_u8[5] <= 0.0)
    {
      printf ("FAIL: past E*I = %d the interleaver must stop helping — a "
              "bound that never bites is not a bound\n",
              E_CORRECT * (int)NCW);
      fail = 1;
    }

  /* 4. The UNIT is load-bearing, and the measurement says exactly how. A
        first draft predicted that bit-interleaving an octet code would be
        catastrophic -- 8x as many symbol errors -- and the measurement
        refused it. Each row still receives its share of a burst as a
        CONTIGUOUS run of bits, so a B-octet burst costs each codeword about
        8B/N consecutive bits, which is about B/N octets either way.

        The difference is ALIGNMENT. At unit=8 each codeword gets exactly
        ceil(B/N) whole symbols; at unit=1 the run is not octet-aligned, so
        it can touch one more symbol at each end. That is invisible until the
        burst is near the E*N bound, where one extra symbol is the difference
        between correcting and not -- which is precisely where it shows up:
        unit=8 corrects E*N and unit=1 does not.

        Stated as the comparison rather than as a threshold, because the
        claim is that the unit matters, not that some particular FER is
        reached. */
  if (f_u8[4] != 0.0 || f_u1[4] <= f_u8[4])
    {
      printf ("FAIL: at the E*N = %d bound, unit=8 must correct and unit=1 "
              "must not (got %.3f vs %.3f) — if they agree, the unit is not "
              "doing what it claims\n",
              E_CORRECT * (int)NCW, f_u8[4], f_u1[4]);
      fail = 1;
    }

  printf ("\n%s\n", fail ? "FAILED" : "PASSED");
  (void)check;
  return fail;
}
