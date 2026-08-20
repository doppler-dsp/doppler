/**
 * @file conv_certify.c
 * @brief The measurements `conv`'s certification report is built from.
 *
 * `docs/dev/contributing/validation.md` puts the evidence layer in Python,
 * because every certified object so far has a Python face and the report is
 * what a caller reads. `conv` has none and is not getting one — a binding
 * built only to be measured is a binding nobody uses. So the split is: **this
 * file measures, and `src/doppler/tests/validation/conv/validate.py` renders
 * and asserts.** Nothing here decides whether a number is acceptable; nothing
 * there computes one.
 *
 * Run with no arguments for a readable sweep (which is what `make validate-c`
 * does), or with `--emit` for the CSV blocks the validator parses.
 *
 * ## Everything measured here comes from the library
 *
 * bits from `pn`, symbols from `mpsk_map`, noise from `awgn`, soft decisions
 * from `mpsk_soft_demap`, and the codec under test from `conv`. This file
 * owns no random number generator, no pulse and no error model — which is the
 * same rule `native/tests/dp_tx_test.h` exists to enforce one layer up, and
 * the reason a number here is comparable with one from the receiver battery.
 *
 * ## What it deliberately does NOT measure
 *
 * The claims `test_conv_core.c` already pins by construction — the impulse
 * response, the trellis run by hand, `d_free`, the LLR sign convention, the
 * state round trip. Those are assertions about the code's identity and belong
 * in a test that fails, not in a sweep that reports. What is here is the
 * behaviour that is only visible statistically: how the error rate moves with
 * Es/N0, with traceback depth, and with hard versus soft decisions, plus how
 * far node synchronization separates its hypotheses.
 */
#include "awgn/awgn_core.h"
#include "conv/conv_core.h"
#include "mpsk/mpsk_core.h"
#include "pn/pn_core.h"
#include "viterbi/viterbi_core.h"
#include "wfm_synth/wfm_synth_core.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CCSDS 131.0-B-3 section 3.3 — the code every number below is measured on
   unless a row says otherwise. It is a CONFIGURATION here, exactly as it is
   everywhere else in the tree. */
static const conv_code_t CCSDS
    = { .k = 7u, .n = 2u, .poly = { 0171u, 0133u }, .invert = 0x2u };

/** @brief Information bits per BER point. */
#define NBITS 120000u

/** @brief The measured traceback depth (`docs/design/viterbi.md` §4). */
#define DEPTH 60u

/* One BER point: bits -> conv -> BPSK -> AWGN -> soft demap -> Viterbi.
 *
 * `hard` quantises the LLRs to +-1 before the decoder sees them, which is
 * what a hard-decision decoder IS: the same trellis search over a two-level
 * input. Measuring it any other way would compare two decoders instead of two
 * inputs. */
static double
ber_point (double esn0_db, unsigned depth, int hard, uint64_t seed,
           size_t *bits_out, size_t *errs_out)
{
  const size_t nbits = NBITS;
  const size_t nsym  = nbits * 2u;

  uint8_t       *in   = malloc (nbits);
  uint8_t       *sym  = malloc (nsym);
  uint8_t       *dec  = malloc (nbits);
  float complex *mod  = malloc (nsym * sizeof *mod);
  float complex *nz   = malloc (nsym * sizeof *nz);
  float         *llr  = malloc (nsym * sizeof *llr);
  uint8_t       *chip = malloc (nbits);
  pn_state_t    *pn   = pn_create (wfm_synth_mls_poly (15), 1u, 15, 0);

  double errs = -1.0;
  if (!in || !sym || !dec || !mod || !nz || !llr || !chip || !pn)
    goto done;

  pn_generate (pn, nbits, chip, nbits);
  for (size_t i = 0; i < nbits; i++)
    in[i] = (uint8_t)(chip[i] & 1u);

  conv_enc_t e;
  conv_enc_init (&e);
  conv_encode (&e, &CCSDS, in, nbits, sym, nsym);
  mpsk_map (sym, nsym, mod, 2);

  const float   sigma = awgn_amplitude_for_snr ((float)esn0_db, 1.0f);
  const float   n0    = 2.0f * sigma * sigma;
  awgn_state_t *ch    = awgn_create (seed, sigma);
  if (!ch)
    goto done;
  awgn_generate (ch, nsym, nz, nsym);
  awgn_destroy (ch);
  for (size_t i = 0; i < nsym; i++)
    mod[i] += nz[i];

  mpsk_soft_demap (mod, nsym, llr, nsym, 2, n0);
  if (hard)
    {
      for (size_t i = 0; i < nsym; i++)
        llr[i] = llr[i] < 0.0f ? -1.0f : 1.0f;
    }

  viterbi_state_t *v = viterbi_create_code (&CCSDS, depth);
  if (!v)
    goto done;
  const size_t got = viterbi_decode (v, llr, nsym, dec, nbits);
  viterbi_destroy (v);

  size_t bad = 0;
  for (size_t i = 0; i < got; i++)
    bad += (dec[i] != in[i]);

  if (bits_out)
    *bits_out = got;
  if (errs_out)
    *errs_out = bad;
  errs = got ? (double)bad / (double)got : -1.0;

done:
  pn_destroy (pn);
  free (in);
  free (sym);
  free (dec);
  free (mod);
  free (nz);
  free (llr);
  free (chip);
  return errs;
}

/* One node-sync point: how far the right alignment sits from the best wrong
   one, over a window, at an Es/N0. Reported as a FRACTION of the symbols
   scored, because that is the quantity a caller sizes a window with. */
static void
node_point (double esn0_db, size_t win, uint64_t seed, double *in_sync,
            double *wrong, double *margin)
{
  const size_t nbits = win;
  const size_t nsym  = nbits * 2u;

  uint8_t       *in   = malloc (nbits);
  uint8_t       *sym  = malloc (nsym);
  float complex *mod  = malloc (nsym * sizeof *mod);
  float complex *nz   = malloc (nsym * sizeof *nz);
  float         *llr  = malloc (nsym * sizeof *llr);
  uint8_t       *chip = malloc (nbits);
  pn_state_t    *pn   = pn_create (wfm_synth_mls_poly (15), 3u, 15, 0);

  *in_sync = *wrong = *margin = -1.0;
  if (!in || !sym || !mod || !nz || !llr || !chip || !pn)
    goto done;

  pn_generate (pn, nbits, chip, nbits);
  for (size_t i = 0; i < nbits; i++)
    in[i] = (uint8_t)(chip[i] & 1u);

  conv_enc_t e;
  conv_enc_init (&e);
  conv_encode (&e, &CCSDS, in, nbits, sym, nsym);
  mpsk_map (sym, nsym, mod, 2);

  const float   sigma = awgn_amplitude_for_snr ((float)esn0_db, 1.0f);
  const float   n0    = 2.0f * sigma * sigma;
  awgn_state_t *ch    = awgn_create (seed, sigma);
  if (!ch)
    goto done;
  awgn_generate (ch, nsym, nz, nsym);
  awgn_destroy (ch);
  for (size_t i = 0; i < nsym; i++)
    mod[i] += nz[i];
  mpsk_soft_demap (mod, nsym, llr, nsym, 2, n0);

  viterbi_state_t *v = viterbi_create_code (&CCSDS, DEPTH);
  if (!v)
    goto done;
  node_sync_t ns;
  if (node_sync_scan (v, llr, nsym, &ns) && ns.symbols)
    {
      *in_sync = (double)ns.errors / (double)ns.symbols;
      *wrong   = (double)ns.next / (double)ns.symbols;
      *margin  = (double)(ns.next - ns.errors) / (double)ns.symbols;
    }
  viterbi_destroy (v);

done:
  pn_destroy (pn);
  free (in);
  free (sym);
  free (mod);
  free (nz);
  free (llr);
  free (chip);
}

int
main (int argc, char **argv)
{
  const int emit = (argc > 1 && strcmp (argv[1], "--emit") == 0);

  /* Es/N0 at the matched-filter output; Eb/N0 = Es/N0 + 3.01 dB at rate 1/2,
     and the report quotes Eb/N0 because that is what a coding gain is read
     in. */
  static const double   ESN0[]      = { -3.0, -2.0, -1.0, 0.0, 1.0, 2.0 };
  static const unsigned DEPTHS[]    = { 15u, 20u, 30u, 35u, 45u, 60u, 90u };
  static const double   NODE_ESN0[] = { -2.0, 0.0, 2.0 };
  static const size_t   NODE_WIN[]  = { 250u, 500u, 1000u, 2000u };

  if (!emit)
    printf ("conv — certification sweeps (CCSDS K=7 r=1/2, %u bits/point)\n\n",
            NBITS);

  /* ── soft vs hard, against Es/N0 ─────────────────────────────────────── */
  if (emit)
    printf ("# ber\nesn0_db,ebn0_db,bits,soft_errors,soft_ber,hard_errors,"
            "hard_ber\n");
  else
    printf ("  Es/N0  Eb/N0   soft BER    hard BER   ratio\n"
            "  -----  -----  ---------   ---------   -----\n");

  for (size_t i = 0; i < sizeof ESN0 / sizeof ESN0[0]; i++)
    {
      size_t       sb = 0, se = 0, hb = 0, he = 0;
      const double s = ber_point (ESN0[i], DEPTH, 0, 20260817u + i, &sb, &se);
      const double h = ber_point (ESN0[i], DEPTH, 1, 20260817u + i, &hb, &he);
      const double ebn0 = ESN0[i] + 3.0103;

      if (emit)
        printf ("%.1f,%.4f,%zu,%zu,%.6e,%zu,%.6e\n", ESN0[i], ebn0, sb, se, s,
                he, h);
      else
        printf ("  %5.1f  %5.2f  %9.3e   %9.3e   %5.1fx\n", ESN0[i], ebn0, s,
                h, s > 0.0 ? h / s : 0.0);
    }

  /* ── traceback depth, where depth can still be SEEN ──────────────────
   *
   * Es/N0 = -2 dB, i.e. Eb/N0 = 1 dB, which is where docs/design/viterbi.md
   * §4 measured it. The first attempt swept at Es/N0 = +1 dB and every depth
   * from 30 up returned zero errors in 120 000 bits: a sweep can only see a
   * parameter at an operating point where the answer is not already zero. */
  if (emit)
    printf ("\n# depth\ndepth,esn0_db,ber\n");
  else
    printf ("\n  depth   BER at Es/N0 = -2.0 dB (Eb/N0 1.0 dB)\n"
            "  -----   -------------------------------------\n");

  for (size_t i = 0; i < sizeof DEPTHS / sizeof DEPTHS[0]; i++)
    {
      const double b = ber_point (-2.0, DEPTHS[i], 0, 4242u, NULL, NULL);
      if (emit)
        printf ("%u,%.1f,%.6e\n", DEPTHS[i], -2.0, b);
      else
        printf ("  %5u   %9.3e\n", DEPTHS[i], b);
    }

  /* ── node synchronization: separation against window and Es/N0 ───────── */
  if (emit)
    printf ("\n# nodesync\nesn0_db,window_bits,in_sync,wrong,margin\n");
  else
    printf ("\n  Es/N0  window   in-sync    wrong     margin\n"
            "  -----  ------   -------   -------   -------\n");

  for (size_t e = 0; e < sizeof NODE_ESN0 / sizeof NODE_ESN0[0]; e++)
    {
      for (size_t w = 0; w < sizeof NODE_WIN / sizeof NODE_WIN[0]; w++)
        {
          double is = 0.0, wr = 0.0, mg = 0.0;
          node_point (NODE_ESN0[e], NODE_WIN[w], 77u + 10u * e + w, &is, &wr,
                      &mg);
          if (emit)
            printf ("%.1f,%zu,%.6f,%.6f,%.6f\n", NODE_ESN0[e], NODE_WIN[w], is,
                    wr, mg);
          else
            printf ("  %5.1f  %6zu   %7.4f   %7.4f   %7.4f\n", NODE_ESN0[e],
                    NODE_WIN[w], is, wr, mg);
        }
    }

  if (!emit)
    printf ("\nRun with --emit for the CSV the validator parses.\n");
  return 0;
}
