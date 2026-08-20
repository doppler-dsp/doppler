/**
 * @file rs_certify.c
 * @brief The measurements `rs`'s certification report is built from.
 *
 * `docs/dev/contributing/validation.md` puts the evidence layer in Python,
 * because every certified object with a Python face is measured through it.
 * `rs` has none and is not getting one — a binding built only to be measured
 * is a binding nobody uses. So the split is the one `conv` established: **this
 * file measures, and `src/doppler/tests/validation/rs/validate.py` renders and
 * asserts.** Nothing here decides whether a number is acceptable; nothing
 * there computes one.
 *
 * Run with no arguments for a readable sweep (which is what `make validate-c`
 * does), or with `--emit` for the CSV blocks the validator parses.
 *
 * ## What it deliberately does NOT measure
 *
 * Everything `test_rs_core.c` pins by construction: the generator's own
 * roots, the parity as a remainder, the syndrome closed form, exact
 * correction at every count up to `E`, refuse-or-return-a-codeword, and the
 * description carrying no running state. Those are assertions about the
 * code's identity and belong in a test that fails, not in a sweep that
 * reports.
 *
 * What is here is the behaviour that is only visible statistically, and it is
 * all one question: **what happens past the guaranteed radius.** The header
 * is careful about this — "a refusal is not the same claim as more than `E`
 * errors: beyond `E` a bounded-distance decoder can land inside another
 * codeword's sphere and miscorrect — a property of the code, not of this
 * implementation. The protection is accounting at the frame level, which is
 * why this reports a count rather than a verdict." That argument is the
 * reason `ccsds_tm_frame_rx_t` reports counts, and until this file it rested
 * on a comment in a test quoting `1/E!` from memory.
 *
 * ## Where the numbers come from
 *
 * The channel sweep is the library's: bits from `pn`, symbols from
 * `mpsk_map`, noise from `awgn`, decisions from `mpsk_soft_demap`, exactly as
 * `conv_certify.c` does, so a number here is comparable with one from there.
 *
 * The sphere sweep places errors itself, with the same `dp_xs32` the C test
 * places them with. An error PATTERN is not a signal — it is the experiment's
 * independent variable — and using one generator for both keeps a row here
 * and an assertion there talking about the same thing.
 */
#include "awgn/awgn_core.h"
#include "dp_rng_test.h"
#include "mpsk/mpsk_core.h"
#include "pn/pn_core.h"
#include "rs/rs_core.h"
#include "wfm_synth/wfm_synth_core.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Trials per (code, error count) point in the sphere sweep. */
#define TRIALS 6000u

/** @brief Codewords per Es/N0 point in the channel sweep. */
#define NCW 200u

/* The codes the sphere sweep runs, chosen so E doubles across the table:
 * the whole point is how the outcome past the radius moves with E, and one
 * code cannot show that. The last two are the same (255,223) code in the two
 * shapes the tree uses — a textbook first root and stride, and CCSDS 4.3.4's
 * j0 = 112, s = 11 — which is where the header's "only the table changes"
 * gets measured rather than argued. RS(15,11) is the small field. */
static const struct
{
  const char *name;
  rs_code_t   code;
} CODES[] = {
  { "RS(255,253) E=1",
    { .symbol_bits = 8,
      .field_poly  = 0x1Du,
      .nroots      = 2,
      .first_root  = 1,
      .root_stride = 1 } },
  { "RS(255,251) E=2",
    { .symbol_bits = 8,
      .field_poly  = 0x1Du,
      .nroots      = 4,
      .first_root  = 1,
      .root_stride = 1 } },
  { "RS(255,247) E=4",
    { .symbol_bits = 8,
      .field_poly  = 0x1Du,
      .nroots      = 8,
      .first_root  = 1,
      .root_stride = 1 } },
  { "RS(255,239) E=8",
    { .symbol_bits = 8,
      .field_poly  = 0x1Du,
      .nroots      = 16,
      .first_root  = 1,
      .root_stride = 1 } },
  { "RS(255,223) E=16",
    { .symbol_bits = 8,
      .field_poly  = 0x1Du,
      .nroots      = 32,
      .first_root  = 1,
      .root_stride = 1 } },
  { "RS(255,223) CCSDS-shaped E=16",
    { .symbol_bits = 8,
      .field_poly  = 0x87u,
      .nroots      = 32,
      .first_root  = 112,
      .root_stride = 11 } },
  { "RS(15,11) E=2",
    { .symbol_bits = 4,
      .field_poly  = 0x03u,
      .nroots      = 4,
      .first_root  = 1,
      .root_stride = 1 } },
};

/* Structured, non-constant information, encoded. NOT zeros: an all-zero
   block has all-zero parity, so every symbol is identical and a decoder that
   repaired the wrong POSITION would still return the transmitted word. That
   trap has hidden two separate defects in this slice. */
static void
make_codeword (const rs_t *rs, uint8_t *word, uint32_t *seed)
{
  const uint8_t mask = (uint8_t)((1u << rs->code.symbol_bits) - 1u);
  for (unsigned i = 0; i < rs->k; i++)
    word[i] = (uint8_t)(dp_xs32 (seed) & mask);
  rs_encode (rs, word, word + rs->k);
}

/* Corrupt `count` distinct positions with nonzero deltas. */
static void
inject (const rs_t *rs, uint8_t *word, unsigned count, uint32_t *seed)
{
  const uint8_t mask          = (uint8_t)((1u << rs->code.symbol_bits) - 1u);
  uint8_t       hit[RS_N_MAX] = { 0 };

  for (unsigned c = 0; c < count; c++)
    {
      unsigned p;
      do
        p = dp_xs32 (seed) % rs->n;
      while (hit[p]);
      hit[p] = 1u;

      uint8_t delta;
      do
        delta = (uint8_t)(dp_xs32 (seed) & mask);
      while (delta == 0);

      word[p] ^= delta;
    }
}

/** @brief One (code, error count) point: how the decoder ends up. */
typedef struct
{
  unsigned corrected;    /**< the sent word came back                */
  unsigned refused;      /**< -1: too far from any codeword to name  */
  unsigned miscorrected; /**< a DIFFERENT codeword, reported as good */
  unsigned noncodeword;  /**< the outcome the header says cannot be  */
} sphere_t;

static sphere_t
sphere_point (const rs_t *rs, unsigned errs, uint32_t seed)
{
  sphere_t r = { 0, 0, 0, 0 };

  for (unsigned t = 0; t < TRIALS; t++)
    {
      uint8_t sent[RS_N_MAX], rx[RS_N_MAX];
      make_codeword (rs, sent, &seed);
      memcpy (rx, sent, rs->n);
      inject (rs, rx, errs, &seed);

      const int got = rs_decode (rs, rx);
      if (got < 0)
        {
          r.refused++;
          continue;
        }
      if (!rs_codeword_ok (rs, rx))
        r.noncodeword++;
      if (memcmp (rx, sent, rs->n) == 0)
        r.corrected++;
      else
        r.miscorrected++;
    }
  return r;
}

/** @brief One Es/N0 point of the channel sweep. */
typedef struct
{
  size_t   sym_total;       /**< symbols transmitted                   */
  size_t   sym_err_in;      /**< symbols the channel broke             */
  size_t   sym_err_out;     /**< symbols still broken after decoding   */
  unsigned cw_good;         /**< codewords delivered byte-exact        */
  unsigned cw_refused;      /**< codewords the decoder declined        */
  unsigned cw_miscorrected; /**< codewords delivered wrong, silently   */
} chan_t;

/* NCW codewords through BPSK + AWGN at one Es/N0.
 *
 * A Reed-Solomon symbol is eight channel bits, and a symbol is broken if ANY
 * of them is — which is the whole reason a symbol code is the outer one. The
 * per-symbol error rate is therefore not a free parameter here; it falls out
 * of the bit channel, and the report reads the two side by side. */
static chan_t
chan_point (const rs_t *rs, double esn0_db, uint64_t seed)
{
  chan_t       r     = { 0, 0, 0, 0, 0, 0 };
  const size_t nsym  = (size_t)NCW * rs->n;         /* code symbols  */
  const size_t nbits = nsym * rs->code.symbol_bits; /* channel bits  */

  uint8_t       *info = malloc ((size_t)NCW * rs->k);
  uint8_t       *sent = malloc (nsym);
  uint8_t       *rx   = malloc (nsym);
  uint8_t       *bits = malloc (nbits);
  float complex *mod  = malloc (nbits * sizeof *mod);
  float complex *nz   = malloc (nbits * sizeof *nz);
  float         *llr  = malloc (nbits * sizeof *llr);
  uint8_t       *chip = malloc (nbits);
  pn_state_t    *pn   = pn_create (wfm_synth_mls_poly (15), 5u, 15, 0);

  if (!info || !sent || !rx || !bits || !mod || !nz || !llr || !chip || !pn)
    goto done;

  /* Information symbols from the library's own bit source, packed J to a
     symbol so the same generator drives every configuration. */
  pn_generate (pn, (size_t)NCW * rs->k * rs->code.symbol_bits, chip, nbits);
  for (size_t s = 0; s < (size_t)NCW * rs->k; s++)
    {
      uint8_t v = 0;
      for (unsigned b = 0; b < rs->code.symbol_bits; b++)
        v = (uint8_t)((v << 1) | (chip[s * rs->code.symbol_bits + b] & 1u));
      info[s] = v;
    }

  for (unsigned c = 0; c < NCW; c++)
    {
      uint8_t *w = sent + (size_t)c * rs->n;
      memcpy (w, info + (size_t)c * rs->k, rs->k);
      rs_encode (rs, w, w + rs->k);
    }

  for (size_t s = 0; s < nsym; s++)
    for (unsigned b = 0; b < rs->code.symbol_bits; b++)
      bits[s * rs->code.symbol_bits + b]
          = (uint8_t)((sent[s] >> (rs->code.symbol_bits - 1u - b)) & 1u);

  mpsk_map (bits, nbits, mod, 2);

  const float   sigma = awgn_amplitude_for_snr ((float)esn0_db, 1.0f);
  const float   n0    = 2.0f * sigma * sigma;
  awgn_state_t *ch    = awgn_create (seed, sigma);
  if (!ch)
    goto done;
  awgn_generate (ch, nbits, nz, nbits);
  awgn_destroy (ch);
  for (size_t i = 0; i < nbits; i++)
    mod[i] += nz[i];

  /* The library's demapper, sliced. A positive LLR means symbol 0 — the
     convention test_viterbi_core.c §5b pins — so the hard decision is its
     sign and not a second slicer written here. */
  mpsk_soft_demap (mod, nbits, llr, nbits, 2, n0);
  for (size_t s = 0; s < nsym; s++)
    {
      uint8_t v = 0;
      for (unsigned b = 0; b < rs->code.symbol_bits; b++)
        v = (uint8_t)((v << 1)
                      | (llr[s * rs->code.symbol_bits + b] < 0.0f ? 1u : 0u));
      rx[s] = v;
    }

  r.sym_total = nsym;
  for (size_t s = 0; s < nsym; s++)
    r.sym_err_in += (rx[s] != sent[s]);

  for (unsigned c = 0; c < NCW; c++)
    {
      uint8_t       *w = rx + (size_t)c * rs->n;
      const uint8_t *o = sent + (size_t)c * rs->n;

      const int got  = rs_decode (rs, w);
      const int same = memcmp (w, o, rs->n) == 0;

      if (got < 0)
        r.cw_refused++;
      else if (same)
        r.cw_good++;
      else
        r.cw_miscorrected++;

      /* Counted after the decode whatever it did, including a refusal: a
         refused codeword hands its errors on untouched, and that is what
         the layer above receives. */
      for (unsigned i = 0; i < rs->n; i++)
        r.sym_err_out += (w[i] != o[i]);
    }

done:
  pn_destroy (pn);
  free (info);
  free (sent);
  free (rx);
  free (bits);
  free (mod);
  free (nz);
  free (llr);
  free (chip);
  return r;
}

int
main (int argc, char **argv)
{
  const int emit = (argc > 1 && strcmp (argv[1], "--emit") == 0);

  /* Es/N0 per CHANNEL BIT at the matched-filter output. The report converts
     to Eb/N0 with the code rate, because a coding claim that does not pay
     for its rate is not a coding claim. */
  static const double ESN0[] = { 3.0, 4.0, 4.5, 5.0, 5.5, 6.0, 7.0 };

  if (!emit)
    printf ("rs — certification sweeps (%u trials/point, %u codewords/point)"
            "\n\n",
            TRIALS, NCW);

  /* ── past the radius: refuse, correct, or miscorrect ─────────────────── */
  if (emit)
    printf ("# sphere\ncode,symbol_bits,nroots,e,errors,trials,corrected,"
            "refused,miscorrected,noncodeword\n");
  else
    printf ("  %-30s errs  corrected  refused  miscorrected\n"
            "  %-30s ----  ---------  -------  ------------\n",
            "code", "");

  for (size_t ci = 0; ci < sizeof CODES / sizeof CODES[0]; ci++)
    {
      rs_t rs;
      if (!rs_init (&rs, &CODES[ci].code))
        {
          fprintf (stderr, "rs_certify: %s is not a code\n", CODES[ci].name);
          return 1;
        }

      /* E, then just past it, then FAR past it. The last one is the point:
         if the miscorrection rate is flat in the error count, a link cannot
         escape a silent failure by being much worse than E — which is the
         difference between "usually detected" and "detected". */
      const unsigned WHERE[] = { rs.e, rs.e + 1u, rs.e + 2u, 4u * rs.e + 1u };

      for (unsigned d = 0; d < sizeof WHERE / sizeof WHERE[0]; d++)
        {
          const unsigned errs = WHERE[d];
          const sphere_t s
              = sphere_point (&rs, errs, 20260818u + (uint32_t)(ci * 8u + d));

          if (emit)
            printf ("\"%s\",%u,%u,%u,%u,%u,%u,%u,%u,%u\n", CODES[ci].name,
                    CODES[ci].code.symbol_bits, CODES[ci].code.nroots, rs.e,
                    errs, TRIALS, s.corrected, s.refused, s.miscorrected,
                    s.noncodeword);
          else
            printf ("  %-30s %4u  %9.5f  %7.5f  %12.5f\n",
                    d == 0 ? CODES[ci].name : "", errs,
                    (double)s.corrected / TRIALS, (double)s.refused / TRIALS,
                    (double)s.miscorrected / TRIALS);
        }
    }

  /* ── the channel it is actually bought for ───────────────────────────── */
  if (emit)
    printf ("\n# channel\nesn0_db,sym_total,sym_err_in,sym_err_out,cw_good,"
            "cw_refused,cw_miscorrected\n");
  else
    printf ("\n  Es/N0   symbol SER in   symbol SER out   codewords good\n"
            "  -----   -------------   --------------   --------------\n");

  {
    rs_t rs;
    if (!rs_init (&rs, &CODES[4].code)) /* textbook RS(255,223) */
      return 1;

    for (size_t i = 0; i < sizeof ESN0 / sizeof ESN0[0]; i++)
      {
        const chan_t c = chan_point (&rs, ESN0[i], 771u + (uint64_t)i);
        if (emit)
          printf ("%.1f,%zu,%zu,%zu,%u,%u,%u\n", ESN0[i], c.sym_total,
                  c.sym_err_in, c.sym_err_out, c.cw_good, c.cw_refused,
                  c.cw_miscorrected);
        else
          printf ("  %5.1f   %13.3e   %14.3e   %5u/%u\n", ESN0[i],
                  (double)c.sym_err_in / (double)c.sym_total,
                  (double)c.sym_err_out / (double)c.sym_total, c.cw_good, NCW);
      }
  }

  if (!emit)
    printf ("\nRun with --emit for the CSV the validator parses.\n");
  return 0;
}
