/**
 * @file ccsds_tm_certify.c
 * @brief The measurements `ccsds_tm`'s certification report is built from.
 *
 * `ccsds_tm` has no Python face and is not getting one, so it follows the
 * split `conv` established and `rs` refined: **this file measures, and
 * `src/doppler/tests/validation/ccsds_tm/validate.py` renders and asserts.**
 * Nothing here decides whether a number is acceptable; nothing there computes
 * one.
 *
 * Run with no arguments for a readable sweep (`make validate-c`), or with
 * `--emit` for the CSV blocks the validator parses.
 *
 * ## What it deliberately does NOT measure
 *
 * Everything the four C tests pin against a PUBLISHED value: the ASM pattern
 * (figure 9-1), the generator polynomial (Annex G), both dual-basis matrices
 * (4.3.9.3), the inner code's impulse response, and both randomiser prefixes.
 * Those are the component's identity, and identity belongs in a test that
 * fails rather than a sweep that reports.
 *
 * It also does not re-measure what `rs`'s own certification already owns —
 * miscorrection past the radius, the sphere model, the codeword-level channel
 * knee. `ccsds_tm` configures that code; it does not re-derive it.
 *
 * What is left is the three things this component adds ON TOP of the code it
 * configures, none of which is visible from a single codeword:
 *
 *   1. **the interleaver** — 4.4.1 buys burst tolerance, and the header
 *      states the exchange rate (`ceil(B/depth)` errors per codeword) without
 *      anything measuring where it stops.
 *   2. **the sync marker as a DETECTOR** — `max_errors` is, in the header's
 *      words, "the whole of the trade", and a trade with no numbers beside it
 *      is a caller guessing.
 *   3. **the randomiser's spectrum** — B-6 demoted the 255-bit sequence for a
 *      stated reason ("may introduce spectral lines at 1/255 of the symbol
 *      rate"). doppler ships both and a spectrum analyser; the rationale is
 *      checkable rather than quotable.
 *
 * ## Where the numbers come from
 *
 * Error patterns are placed with `dp_xs32`, the same generator the C tests
 * place them with, so a row here and an assertion there describe the same
 * experiment. The spectrum is `psd_core`'s — the shipped meter, not a private
 * periodogram, so a number here is one a caller can reproduce from Python.
 */
#include "ccsds_tm/ccsds_tm.h"
#include "ccsds_tm/ccsds_tm_frame.h"
#include "ccsds_tm/ccsds_tm_rs.h"
#include "dp_rng_test.h"
#include "psd/psd_core.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Blocks per (depth, burst) point in the interleaver sweep. */
#define BURST_TRIALS 200u

/** @brief Bit offsets searched per false-alarm point. */
#define FA_BITS 2000000u

/** @brief Marker instances per (BER, threshold) detection point. */
#define PD_TRIALS 4000u

/**
 * @brief Random bits preceding the marker in the H1 sweep.
 *
 * Load-bearing, not padding. `asm_find` reports the FIRST offset under
 * threshold, so every preceding bit is another chance for a false hit to win
 * the race against the real marker — which is why the detection rate at a
 * loose threshold is capped by the lead-in rather than by the channel. A
 * synchroniser searching a live stream has a lead-in; a measurement without
 * one would report a detector that does not exist.
 */
#define ASM_LEAD_BITS 96

/** @brief 4.3.5.1's allowed interleaving depths. */
static const unsigned DEPTHS[] = { 1u, 2u, 3u, 4u, 5u, 8u };
#define N_DEPTHS (sizeof DEPTHS / sizeof DEPTHS[0])

/* ── 1. the interleaver ──────────────────────────────────────────────────
 *
 * A contiguous burst of B symbols spread over `depth` interleaved codewords
 * lands as `ceil(B/depth)` errors in the worst-hit one, so the block survives
 * exactly while that is at most E. The predicted edge is therefore
 * `B_max = depth * E`, and one symbol more must break the worst-hit codeword
 * and only that one.
 *
 * Measured on structured information, never zeros: at depth > 1 an all-zero
 * block gives every codeword identical symbols, so a de-interleaver that
 * rotated the wrong way would still hand each decoder a valid word.
 */
typedef struct
{
  unsigned blocks;        /**< blocks run at this point                  */
  unsigned all_ok;        /**< every codeword decoded                    */
  unsigned uncorrectable; /**< codewords refused, summed over blocks     */
  unsigned symbols;       /**< symbol errors repaired, summed            */
  unsigned frame_exact;   /**< information section recovered byte-exact  */
} burst_t;

static burst_t
burst_point (unsigned depth, unsigned burst, uint32_t seed)
{
  burst_t      r      = { 0 };
  const size_t k_syms = (size_t)CCSDS_TM_RS_K * depth;
  const size_t n_syms = (size_t)CCSDS_TM_RS_N * depth;

  uint8_t *info  = malloc (k_syms);
  uint8_t *block = malloc (n_syms);
  uint8_t *sent  = malloc (n_syms);
  if (!info || !block || !sent)
    exit (1);

  for (unsigned t = 0; t < BURST_TRIALS; t++)
    {
      for (size_t i = 0; i < k_syms; i++)
        info[i] = (uint8_t)(dp_xs32 (&seed) & 0xFFu);
      if (ccsds_tm_rs_encode_block (info, depth, block) != n_syms)
        exit (1);
      memcpy (sent, block, n_syms);

      /* One contiguous burst, placed so it lies wholly inside the block. */
      const size_t at = burst >= n_syms
                            ? 0u
                            : (size_t)(dp_xs32 (&seed) % (n_syms - burst));
      for (unsigned i = 0; i < burst; i++)
        {
          /* Never a no-op: a zero delta would leave the symbol correct and
             quietly shorten the burst under test. */
          uint8_t d = (uint8_t)(dp_xs32 (&seed) & 0xFFu);
          block[at + i] ^= (uint8_t)(d ? d : 1u);
        }

      ccsds_tm_rs_block_rx_t rx = { 0 };
      ccsds_tm_rs_decode_block (block, depth, &rx);

      r.blocks++;
      r.uncorrectable += rx.uncorrectable;
      r.symbols += rx.symbols;
      if (rx.uncorrectable == 0u)
        r.all_ok++;
      /* The claim a caller cares about is the INFORMATION coming back, which
         is not the same as every codeword decoding: a miscorrection decodes
         and is wrong. Compare against what was sent. */
      if (memcmp (block, sent, n_syms) == 0)
        r.frame_exact++;
    }

  free (info);
  free (block);
  free (sent);
  return r;
}

/* ── 2. the marker as a detector ─────────────────────────────────────────
 *
 * `ccsds_tm_asm_find` reports the first offset whose Hamming distance to the
 * 32-bit marker is at most `max_errors`, in either polarity. That makes it a
 * detector with a threshold, and a threshold has two tails.
 *
 * H0 is random data. Per offset and per polarity the distance is Binomial(32,
 * 1/2), so the false-alarm probability at threshold t is
 *
 *     P_fa = 2 * sum_{i=0..t} C(32,i) / 2^32
 *
 * with the factor two for the complement search — the closed form the
 * validator compares this against. It is worth measuring rather than only
 * computing because `asm_find` scans EVERY bit offset, so the per-stream rate
 * is what a frame synchroniser actually experiences.
 */
static unsigned
fa_point (unsigned max_errors, uint32_t seed)
{
  uint8_t *bits = malloc (FA_BITS);
  if (!bits)
    exit (1);
  for (size_t i = 0; i < FA_BITS; i++)
    bits[i] = (uint8_t)(dp_xs32 (&seed) & 1u);

  /* Count every hit, not just the first: the rate is per offset, and
     stopping at the first one measures the stream length instead. */
  unsigned           hits = 0;
  size_t             from = 0;
  ccsds_tm_asm_hit_t hit;
  while (from + CCSDS_TM_ASM_BITS <= FA_BITS
         && ccsds_tm_asm_find (bits + from, FA_BITS - from, max_errors, &hit))
    {
      hits++;
      from += hit.offset + 1u;
    }
  free (bits);
  return hits;
}

/* H1 is a marker that crossed a channel. The bits are already Viterbi output
 * by the time a synchroniser sees them, so the independent variable is the
 * DECODED bit error rate, not Es/N0 — which also keeps this measurement from
 * silently re-running `conv`'s. */
typedef struct
{
  unsigned trials;
  unsigned found;       /**< reported a marker anywhere                     */
  unsigned correct;     /**< at the offset it was actually put              */
  unsigned polarity_ok; /**< and said the right thing about inversion   */
} pd_t;

static pd_t
pd_point (double ber, unsigned max_errors, int invert, uint32_t seed)
{
  enum
  {
    PAD = ASM_LEAD_BITS,
    N   = PAD + CCSDS_TM_ASM_BITS + PAD
  };
  pd_t    r = { 0 };
  uint8_t bits[N];

  for (unsigned t = 0; t < PD_TRIALS; t++)
    {
      for (int i = 0; i < N; i++)
        bits[i] = (uint8_t)(dp_xs32 (&seed) & 1u);
      ccsds_tm_asm_bits (bits + PAD);
      if (invert)
        for (int i = 0; i < N; i++)
          bits[i] ^= 1u;

      /* The channel touches the whole window, marker and surroundings
         alike — corrupting only the marker would measure a cleaner problem
         than the one a synchroniser has. */
      for (int i = 0; i < N; i++)
        if (dp_uni (&seed) < ber)
          bits[i] ^= 1u;

      ccsds_tm_asm_hit_t hit;
      r.trials++;
      if (ccsds_tm_asm_find (bits, N, max_errors, &hit))
        {
          r.found++;
          if (hit.offset == (size_t)PAD)
            {
              r.correct++;
              if (!hit.inverted == !invert)
                r.polarity_ok++;
            }
        }
    }
  return r;
}

/* ── 3. the randomiser's spectrum ────────────────────────────────────────
 *
 * B-6 keeps the 255-bit sequence only for legacy systems, and says why: it
 * "may introduce spectral lines at 1/255 of the symbol rate" and so "could
 * not guarantee full compliance with ITU power flux density limits". That is
 * a claim about a waveform, and this tree has a spectrum analyser.
 *
 * The measurement is the worst case the header already names: randomise a run
 * of CONSTANT data, so the transmitted stream IS the sequence. A period-P
 * sequence then repeats every P bits and its power sits on the harmonics of
 * 1/P; the question is how far above the surrounding floor those lines stand,
 * for each of the two generators, at the same analysis length.
 *
 * `psd_core` is the meter rather than a private periodogram, so the number is
 * one a caller reproduces with `doppler.spectral.PSD`.
 */
typedef struct
{
  double peak_db;   /**< strongest non-DC line, dB above the median bin */
  double at_1_255;  /**< the bin nearest 1/255, same reference          */
  double median_db; /**< the floor the two are measured against         */
} spec_t;

static spec_t
rand_spectrum (const ccsds_tm_rand_t *r, size_t nfft, size_t frames)
{
  spec_t       out = { 0 };
  const size_t n   = nfft * frames;

  uint8_t *bits = malloc (n);
  float   *x    = malloc (n * sizeof *x);
  if (!bits || !x)
    exit (1);

  /* Constant data, so the randomiser's own sequence is what is transmitted;
     a PN payload would be flat whatever the generator did, which is the
     measurement hazard `ccsds_tm.h` warns about for the same reason. */
  memset (bits, 0, n);
  ccsds_tm_randomise_with (r, bits, n);
  for (size_t i = 0; i < n; i++)
    x[i] = bits[i] ? -1.0f : 1.0f; /* NRZ, as a symbol mapper would */

  /* Rectangular analysis is wrong here and Blackman-Harris is why: the lines
     are the signal, so a window whose sidelobes are above the line spacing
     would smear one harmonic into its neighbours and report a floor that is
     really the window. */
  psd_state_t *p = psd_create (nfft, 1.0, 2 /* Blackman-Harris */, 0.0f, 1,
                               1.0, 0, 0 /* mean */, 0.0);
  if (!p)
    exit (1);
  psd_accumulate_real (p, x, n);

  const size_t nb = psd_power_onesided_max_out (p);
  float       *pw = malloc (nb * sizeof *pw);
  if (!pw)
    exit (1);
  psd_power_onesided (p, nb, pw, nb);

  /* The floor is the MEDIAN bin, not the mean: a mean over a spectrum whose
     lines carry most of the power is dragged up by the thing being measured.
     Bin 0 is DC — constant data has a DC term that belongs to the mapping,
     not to the generator — so the search starts at 1. */
  float *sorted = malloc (nb * sizeof *sorted);
  if (!sorted)
    exit (1);
  memcpy (sorted, pw, nb * sizeof *sorted);
  for (size_t i = 1; i < nb; i++)
    {
      const float v = sorted[i];
      size_t      j = i;
      while (j > 0 && sorted[j - 1] > v)
        {
          sorted[j] = sorted[j - 1];
          j--;
        }
      sorted[j] = v;
    }
  const double med = sorted[nb / 2] > 0.0f ? sorted[nb / 2] : 1e-30;

  double peak = 0.0;
  for (size_t i = 1; i < nb; i++)
    if (pw[i] > peak)
      peak = pw[i];

  /* One-sided bin i is frequency i/(2*(nb-1)) in cycles per bit. */
  const size_t bin_1_255
      = (size_t)((1.0 / 255.0) * (double)(2u * ((size_t)nb - 1u)) + 0.5);

  out.median_db = 10.0 * log10 (med);
  out.peak_db   = 10.0 * log10 (peak / med);
  out.at_1_255
      = 10.0 * log10 ((bin_1_255 < nb ? pw[bin_1_255] : (float)med) / med);

  free (sorted);
  free (pw);
  free (x);
  free (bits);
  psd_destroy (p);
  return out;
}

int
main (int argc, char **argv)
{
  const int emit = (argc > 1 && strcmp (argv[1], "--emit") == 0);

  if (!emit)
    printf ("=== ccsds_tm certification sweeps ===\n\n"
            "The interleaver, the marker as a detector, and the spectrum\n"
            "B-6 demoted the legacy randomiser over.\n");

  /* ── the interleaver ──────────────────────────────────────────────────*/
  if (emit)
    printf ("\n# burst\ndepth,burst,blocks,all_ok,uncorrectable,symbols,"
            "frame_exact\n");
  else
    printf ("\n  depth  burst  blocks all decoded  exact\n"
            "  -----  -----  ------------------  -----\n");

  for (size_t d = 0; d < N_DEPTHS; d++)
    {
      const unsigned depth = DEPTHS[d];
      const unsigned edge  = depth * CCSDS_TM_RS_E;
      /* Straddle the predicted edge, and take one point well inside it so a
         sweep that failed everywhere is distinguishable from one that found
         the boundary. */
      const unsigned WHERE[] = { edge / 2u, edge - 1u, edge, edge + 1u,
                                 edge + (depth > 1u ? depth : 2u) };
      for (size_t w = 0; w < sizeof WHERE / sizeof WHERE[0]; w++)
        {
          const burst_t b = burst_point (depth, WHERE[w],
                                         20260820u + (uint32_t)(d * 16u + w));
          if (emit)
            printf ("%u,%u,%u,%u,%u,%u,%u\n", depth, WHERE[w], b.blocks,
                    b.all_ok, b.uncorrectable, b.symbols, b.frame_exact);
          else
            printf ("  %5u  %5u  %8u/%-9u  %5u\n", w == 0 ? depth : 0u,
                    WHERE[w], b.all_ok, b.blocks, b.frame_exact);
        }
    }

  /* ── the marker: H0 ───────────────────────────────────────────────────*/
  if (emit)
    printf ("\n# asm_fa\nmax_errors,bits,hits\n");
  else
    printf ("\n  max_errors   false hits per %u random bits\n"
            "  ----------   ---------------------------\n",
            FA_BITS);

  for (unsigned t = 0; t <= 8u; t++)
    {
      const unsigned h = fa_point (t, 424242u + t);
      if (emit)
        printf ("%u,%u,%u\n", t, FA_BITS, h);
      else
        printf ("  %10u   %u\n", t, h);
    }

  /* ── the marker: H1 ───────────────────────────────────────────────────*/
  static const double BER[] = { 0.0, 0.01, 0.02, 0.05, 0.10, 0.15, 0.20 };
  if (emit)
    printf ("\n# asm_pd\nber,max_errors,inverted,lead_bits,trials,found,"
            "correct,polarity_ok\n");
  else
    printf ("\n  BER    t=2     t=4     t=6     t=8   (detected at the right "
            "offset)\n"
            "  ----   -----   -----   -----   -----\n");

  for (size_t i = 0; i < sizeof BER / sizeof BER[0]; i++)
    {
      if (!emit)
        printf ("  %.2f ", BER[i]);
      for (unsigned t = 2u; t <= 8u; t += 2u)
        {
          for (int inv = 0; inv <= 1; inv++)
            {
              const pd_t p = pd_point (
                  BER[i], t, inv,
                  9001u + (uint32_t)(i * 32u + t * 2u + (unsigned)inv));
              if (emit)
                printf ("%.2f,%u,%d,%u,%u,%u,%u,%u\n", BER[i], t, inv,
                        (unsigned)ASM_LEAD_BITS, p.trials, p.found, p.correct,
                        p.polarity_ok);
              else if (inv == 0)
                printf ("  %5.3f", (double)p.correct / p.trials);
            }
        }
      if (!emit)
        printf ("\n");
    }

  /* ── the randomiser's spectrum ────────────────────────────────────────*/
  {
    /* 4096-bin frames over 64 of them: long enough that the legacy period
       lands on a bin (255 is not a divisor of 4096, so the line is spread
       across neighbours — which is why the peak, not one nominated bin, is
       the statistic the validator reads). */
    const size_t NFFT = 4096u, FRAMES = 64u;
    const spec_t def = rand_spectrum (&CCSDS_TM_RAND, NFFT, FRAMES);
    const spec_t leg = rand_spectrum (&CCSDS_TM_RAND_LEGACY, NFFT, FRAMES);

    if (emit)
      {
        printf (
            "\n# spectrum\nwhich,period,nfft,frames,peak_db,at_1_255_db\n");
        printf ("default,%zu,%zu,%zu,%.3f,%.3f\n",
                (size_t)CCSDS_TM_RAND.period, NFFT, FRAMES, def.peak_db,
                def.at_1_255);
        printf ("legacy,%zu,%zu,%zu,%.3f,%.3f\n",
                (size_t)CCSDS_TM_RAND_LEGACY.period, NFFT, FRAMES, leg.peak_db,
                leg.at_1_255);
      }
    else
      printf ("\n  randomiser  period   strongest line   at 1/255\n"
              "  ----------  ------   --------------   --------\n"
              "  default     %6zu   %10.1f dB   %6.1f dB\n"
              "  legacy      %6zu   %10.1f dB   %6.1f dB\n",
              (size_t)CCSDS_TM_RAND.period, def.peak_db, def.at_1_255,
              (size_t)CCSDS_TM_RAND_LEGACY.period, leg.peak_db, leg.at_1_255);
  }

  if (!emit)
    printf ("\nRun with --emit for the CSV the validator parses.\n");
  return 0;
}
