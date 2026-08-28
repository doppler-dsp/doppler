#include "burst_demod/burst_demod_core.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ACQ_SF 500
#define ACQ_REPS 5
#define DATA_SF 50
#define SPC 4
#define SYNC_LEN 13
#define PAYLOAD 64
#define CRC_BITS 16
/* What demod() hands back per burst: the frame as received. */
#define FRAME_SYMS (SYNC_LEN + PAYLOAD + CRC_BITS)
#define CHIP_RATE 1.0e6

/* Barker-13 as 0/1 (0 -> +1, 1 -> -1). */
static const uint8_t SYNC[SYNC_LEN]
    = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0 };

static float
csign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

static uint16_t
crc16 (const uint8_t *bits, size_t n)
{
  uint16_t c = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      c ^= (uint16_t)((bits[i] & 1u) << 15);
      c = (c & 0x8000u) ? (uint16_t)((c << 1) ^ 0x1021u) : (uint16_t)(c << 1);
    }
  return c;
}

/* The caller's half of the split: does the frame's own trailer match its
 * own payload? This object stops at decisions (doppler#1022), so every
 * assertion that used to read `frame_valid` reads this instead — the same
 * arithmetic, at the layer that owns it. `wfm.Frame.deframe()` is the
 * shipped form; six lines here keep this test linked against burst objects
 * only. */
static int
frame_ok (const uint8_t *frame, size_t n)
{
  if (n < SYNC_LEN + PAYLOAD + CRC_BITS)
    return 0;
  uint16_t rx = 0;
  for (size_t j = 0; j < CRC_BITS; j++)
    rx = (uint16_t)((rx << 1) | (frame[SYNC_LEN + PAYLOAD + j] & 1u));
  return rx == crc16 (frame + SYNC_LEN, PAYLOAD);
}

/* Append one BPSK data symbol (bit -> +/-1) spread by data_code. */
static size_t
put_symbol (float complex *y, size_t n, const uint8_t *dcode, uint8_t bit)
{
  float a = csign (bit);
  for (size_t c = 0; c < DATA_SF; c++)
    for (size_t k = 0; k < SPC; k++)
      y[n++] = a * csign (dcode[c]);
  return n;
}

/* Build preamble (5x500 unmod) + frame (sync|payload|crc), then apply the
 * carrier exp(j2π(f0·n + ½μ·n²)). Returns total sample count. */
static size_t
build_burst (float complex *y, const uint8_t *acode, const uint8_t *dcode,
             const uint8_t *payload, double f0, double mu)
{
  size_t n = 0;
  for (size_t r = 0; r < ACQ_REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++)
        y[n++] = csign (acode[c]); /* unmodulated preamble */
  for (size_t j = 0; j < SYNC_LEN; j++)
    n = put_symbol (y, n, dcode, SYNC[j]);
  for (size_t j = 0; j < PAYLOAD; j++)
    n = put_symbol (y, n, dcode, payload[j]);
  uint16_t crc = crc16 (payload, PAYLOAD);
  for (size_t j = 0; j < CRC_BITS; j++)
    n = put_symbol (y, n, dcode, (crc >> (CRC_BITS - 1 - j)) & 1u);

  for (size_t i = 0; i < n; i++)
    {
      double ph
          = 2.0 * M_PI * (f0 * (double)i + 0.5 * mu * (double)i * (double)i);
      y[i] *= (float)cos (ph) + (float)sin (ph) * I;
    }
  return n;
}

/* As build_burst(), but with `filler` random-ish data symbols BEFORE the
 * sync word, and an optional payload bit flipped after the CRC is computed
 * (so the trailer no longer matches). Both are what the read-back claims
 * below need: frame_offset is only meaningful when the sync is NOT at 0,
 * and frame_valid's negative case needs a frame that ARRIVES and fails. */
static size_t
build_burst_ex (float complex *y, const uint8_t *acode, const uint8_t *dcode,
                const uint8_t *payload, double f0, size_t filler,
                int corrupt_at)
{
  size_t n = 0;
  for (size_t r = 0; r < ACQ_REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++)
        y[n++] = csign (acode[c]);
  for (size_t j = 0; j < filler; j++)
    n = put_symbol (y, n, dcode, (uint8_t)((j * 5u + 1u) & 1u));
  for (size_t j = 0; j < SYNC_LEN; j++)
    n = put_symbol (y, n, dcode, SYNC[j]);

  uint16_t crc = crc16 (payload, PAYLOAD); /* over the CLEAN payload */
  for (size_t j = 0; j < PAYLOAD; j++)
    {
      uint8_t b = payload[j];
      if (corrupt_at >= 0 && (size_t)corrupt_at == j)
        b = (uint8_t)(b ^ 1u); /* transmitted wrong; CRC unchanged */
      n = put_symbol (y, n, dcode, b);
    }
  for (size_t j = 0; j < CRC_BITS; j++)
    n = put_symbol (y, n, dcode, (crc >> (CRC_BITS - 1 - j)) & 1u);

  for (size_t i = 0; i < n; i++)
    {
      double ph = 2.0 * M_PI * f0 * (double)i;
      y[i] *= (float)cos (ph) + (float)sin (ph) * I;
    }
  return n;
}

static int
run_case (const char *name, double f0, double f0_prior, double mu,
          double max_rate)
{
  /* Codes + payload (deterministic). */
  uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
  for (size_t i = 0; i < ACQ_SF; i++)
    acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
  for (size_t i = 0; i < DATA_SF; i++)
    dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
  for (size_t i = 0; i < PAYLOAD; i++)
    payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

  size_t cap
      = (ACQ_SF * ACQ_REPS + (SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF) * SPC
        + 16;
  float complex *y = malloc (cap * sizeof *y);
  size_t         n = build_burst (y, acode, dcode, payload, f0, mu);

  burst_demod_state_t *d = burst_demod_create (dcode, DATA_SF, SPC, CHIP_RATE,
                                               0.0, max_rate, FRAME_SYMS, 10);
  DP_CHECK (d != NULL);
  burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
  burst_demod_set_sync (d, SYNC, SYNC_LEN);
  burst_demod_set_prior (d, f0_prior, 0);

  uint8_t bits[FRAME_SYMS];
  size_t  nb = burst_demod_demod (d, y, n, bits, FRAME_SYMS);
  DP_CHECK (nb == FRAME_SYMS);
  /* The FRAME comes back, sync word first, exactly as transmitted. Which
     bits are payload is the caller's arithmetic now, and so is the check:
     this object stops at decisions (doppler#1022). */
  size_t errs = 0;
  for (size_t i = 0; i < SYNC_LEN; i++)
    if (bits[i] != SYNC[i])
      errs++;
  for (size_t i = 0; i < PAYLOAD; i++)
    if (bits[SYNC_LEN + i] != payload[i])
      errs++;
  DP_CHECK (errs == 0);
  /* ...and the trailer it received is the one crc16() computes over the
     payload it received -- the whole point of the trailer, verified where
     a caller would verify it. */
  DP_CHECK_MSG (frame_ok (bits, nb),
                "the trailer it received is the one crc16() computes over "
                "the payload it received");

  printf ("  %-10s f0=%.4f(prior %.4f) mu=%.2e | est f=%.1fHz r=%.2eHz/s "
          "snr=%.0f off=%zu errs=%zu\n",
          name, f0, f0_prior, mu, d->est_freq_hz, d->est_rate_hz,
          d->est_snr_db, d->frame_offset, errs);
  burst_demod_destroy (d);
  free (y);
  return 0;
}

/* Guard / error / clamp paths the happy-path cases never reach. */
static int
run_edge_cases (void)
{
  uint8_t dc[DATA_SF], ac[ACQ_SF];
  for (size_t i = 0; i < DATA_SF; i++)
    dc[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < ACQ_SF; i++)
    ac[i] = (uint8_t)(i & 1u);

  /* Argument validation → NULL (each clause of the create guard). */
  DP_CHECK (
      burst_demod_create (NULL, DATA_SF, SPC, CHIP_RATE, 0, 0, FRAME_SYMS, 10)
      == NULL);
  DP_CHECK (burst_demod_create (dc, 0, SPC, CHIP_RATE, 0, 0, FRAME_SYMS, 10)
            == NULL);
  DP_CHECK (
      burst_demod_create (dc, DATA_SF, 0, CHIP_RATE, 0, 0, FRAME_SYMS, 10)
      == NULL);
  DP_CHECK (burst_demod_create (dc, DATA_SF, SPC, 0.0, 0, 0, FRAME_SYMS, 10)
            == NULL);
  DP_CHECK (
      burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0, -1.0, FRAME_SYMS, 10)
      == NULL);
  DP_CHECK (burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0, 0, PAYLOAD, 0)
            == NULL);

  burst_demod_destroy (NULL); /* no-op on NULL */

  burst_demod_state_t *d
      = burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0, 0, FRAME_SYMS, 10);
  DP_CHECK (d != NULL);
  burst_demod_set_preamble (d, NULL, 0, 0); /* guard: ignored */
  burst_demod_set_sync (d, NULL, 0);        /* guard: ignored */
  burst_demod_set_preamble (d, ac, ACQ_SF, ACQ_REPS);
  burst_demod_set_preamble (d, ac, ACQ_SF,
                            ACQ_REPS); /* re-arm: frees old ppe */
  burst_demod_set_sync (d, SYNC, SYNC_LEN);

  /* Too-short input → clean failure: no frame, so no bits and no LLRs. */
  float complex tiny[8] = { 0 };
  uint8_t       eb[FRAME_SYMS];
  burst_demod_set_prior (d, 0.0, 0);
  DP_CHECK (burst_demod_demod (d, tiny, 8, eb, FRAME_SYMS) == 0);
  float el[FRAME_SYMS];
  DP_CHECK (burst_demod_llrs (d, 1, el, FRAME_SYMS) == 0);
  /* The capacity accessor answers from the CONFIGURATION, so it reports a
     frame's worth even when the last call produced none — that is what a
     caller sizes a buffer with, before there is anything to size for. */
  DP_CHECK_MSG (burst_demod_llrs_max_out (d, 1) == FRAME_SYMS,
                "llrs_max_out is the frame's length, not the last call's");
  DP_CHECK_MSG (burst_demod_demod_max_out (d) == FRAME_SYMS,
                "and demod_max_out agrees with it");
  burst_demod_destroy (d);

  /* est_segments > acq_sf forces the per-segment chip clamp (Lseg >= 1). */
  burst_demod_state_t *d2 = burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0,
                                                0, FRAME_SYMS, ACQ_SF + 100);
  DP_CHECK (d2 != NULL);
  burst_demod_set_preamble (d2, ac, ACQ_SF, 1);
  burst_demod_destroy (d2);
  return 0;
}

int
main (void)
{
  (void)run_edge_cases ();

  /* Near-static Doppler (negligible rate): max_rate = 0, single-FFT estimate.
   */
  (void)run_case ("static", 0.012, 0.012, 0.0, 0.0);

  /* LEO: a real chirp, coarse prior slightly off; the 2-D estimate recovers
   * the residual Doppler + rate and dechirps before despreading. */
  (void)run_case ("leo", 0.012, 0.0115, 6.0e-7, 1.0e-6);

  /* ── a flipped bit REACHES the output, and the trailer catches it ─────
   *
   * The demodulator's contract is that its bits are what was transmitted,
   * so the interesting case is a frame that arrives intact, aligns on the
   * sync, produces its symbols -- and carries one payload bit transmitted
   * flipped after the trailer was computed. Two things must then be true,
   * and they are the two halves of the layering: this object hands the
   * error THROUGH rather than hiding or fixing it, and the check that
   * notices belongs to whoever holds the frame.
   *
   * It used to assert `frame_valid == 0` here, which tested the same
   * arithmetic one layer too low. */
  {
    uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
    for (size_t i = 0; i < ACQ_SF; i++)
      acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
    for (size_t i = 0; i < DATA_SF; i++)
      dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
    for (size_t i = 0; i < PAYLOAD; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    const size_t cap
        = (ACQ_SF * ACQ_REPS + (16 + SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF)
              * SPC
          + 16;
    float complex *y = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    if (y)
      {
        const double f0 = 0.012;
        uint8_t      bits[FRAME_SYMS];

        /* Baseline: clean, so the negative results below are not simply a
           demodulator that never works. */
        size_t n = build_burst_ex (y, acode, dcode, payload, f0, 0, -1);
        burst_demod_state_t *d = burst_demod_create (
            dcode, DATA_SF, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10);
        DP_CHECK (d != NULL);
        if (d)
          {
            burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
            burst_demod_set_sync (d, SYNC, SYNC_LEN);
            burst_demod_set_prior (d, f0, 0);
            DP_CHECK (burst_demod_demod (d, y, n, bits, FRAME_SYMS)
                      == FRAME_SYMS);
            uint16_t rx = 0;
            for (size_t j = 0; j < CRC_BITS; j++)
              rx = (uint16_t)((rx << 1) | (bits[SYNC_LEN + PAYLOAD + j] & 1u));
            DP_CHECK (rx == crc16 (bits + SYNC_LEN, PAYLOAD));
            burst_demod_destroy (d);
          }

        /* Three flipped positions, including both ends of the payload. */
        const int spots[3] = { 0, 17, PAYLOAD - 1 };
        for (int si = 0; si < 3; si++)
          {
            n = build_burst_ex (y, acode, dcode, payload, f0, 0, spots[si]);
            burst_demod_state_t *b = burst_demod_create (
                dcode, DATA_SF, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10);
            DP_CHECK (b != NULL);
            if (b)
              {
                burst_demod_set_preamble (b, acode, ACQ_SF, ACQ_REPS);
                burst_demod_set_sync (b, SYNC, SYNC_LEN);
                burst_demod_set_prior (b, f0, 0);
                size_t nb = burst_demod_demod (b, y, n, bits, FRAME_SYMS);
                /* The frame is still demodulated -- this is not the
                   too-short path. */
                DP_CHECK (nb == FRAME_SYMS);
                /* The flip is IN the output, at the bit it was applied to:
                   the demodulator reports what arrived. */
                DP_CHECK_MSG (bits[SYNC_LEN + (size_t)spots[si]]
                                  != payload[spots[si]],
                              "a transmitted bit error must reach the caller");
                /* ...and ONLY that bit: a demodulator that "fixed" one
                   error by mangling its neighbours would pass the check
                   above and be useless. */
                size_t other = 0;
                for (size_t i = 0; i < PAYLOAD; i++)
                  if (i != (size_t)spots[si]
                      && bits[SYNC_LEN + i] != payload[i])
                    other++;
                DP_CHECK_MSG (other == 0, "and no other payload bit moved");
                /* The sync word is untouched too — it is what the frame was
                   found by. */
                size_t sync_moved = 0;
                for (size_t i = 0; i < SYNC_LEN; i++)
                  if (bits[i] != SYNC[i])
                    sync_moved++;
                DP_CHECK_MSG (sync_moved == 0, "and the sync word is intact");
                /* ...and the trailer no longer matches, which is the check
                   doing its job one layer up. */
                uint16_t rx = 0;
                for (size_t j = 0; j < CRC_BITS; j++)
                  rx = (uint16_t)((rx << 1)
                                  | (bits[SYNC_LEN + PAYLOAD + j] & 1u));
                DP_CHECK (rx != crc16 (bits + SYNC_LEN, PAYLOAD));
                burst_demod_destroy (b);
              }
          }
        free (y);
      }
  }

  /* ── frame_offset and n_symbols, away from their degenerate values ────
   *
   * frame_offset is documented as "symbol offset of the sync word" and was
   * only ever observed as 0 -- which is what a read-back hardwired to zero
   * also reports. n_symbols ("despread data symbols produced") had no
   * mention at all in either language.
   *
   * Both are checked against a burst carrying filler symbols BEFORE the
   * sync: the offset must equal the filler count, and n_symbols must grow
   * with it, because the demodulator despreads the whole data section and
   * then aligns within it. */
  {
    uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
    for (size_t i = 0; i < ACQ_SF; i++)
      acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
    for (size_t i = 0; i < DATA_SF; i++)
      dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
    for (size_t i = 0; i < PAYLOAD; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    const size_t cap
        = (ACQ_SF * ACQ_REPS + (16 + SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF)
              * SPC
          + 16;
    float complex *y = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    if (y)
      {
        const double f0       = 0.012;
        const size_t fills[3] = { 0, 3, 9 };
        uint8_t      bits[FRAME_SYMS];
        size_t       prev_syms = 0;
        for (int fi = 0; fi < 3; fi++)
          {
            size_t n
                = build_burst_ex (y, acode, dcode, payload, f0, fills[fi], -1);
            burst_demod_state_t *d = burst_demod_create (
                dcode, DATA_SF, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10);
            DP_CHECK (d != NULL);
            if (d)
              {
                burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
                burst_demod_set_sync (d, SYNC, SYNC_LEN);
                burst_demod_set_prior (d, f0, 0);
                DP_CHECK (burst_demod_demod (d, y, n, bits, FRAME_SYMS)
                          == FRAME_SYMS);
                DP_CHECK_MSG (frame_ok (bits, FRAME_SYMS),
                              "a frame behind filler symbols still checks "
                              "out — the offset is found, not guessed");
                /* the sync sits exactly `filler` symbols in */
                DP_CHECK (d->frame_offset == fills[fi]);
                /* and the whole data section was despread */
                DP_CHECK (d->n_symbols
                          == fills[fi] + SYNC_LEN + PAYLOAD + CRC_BITS);
                DP_CHECK (d->n_symbols > prev_syms || fi == 0);
                prev_syms = d->n_symbols;
                burst_demod_destroy (d);
              }
          }
        free (y);
      }
  }

  /* ── reset() clears the read-backs ────────────────────────────────────
   *
   * Documented, and called by nothing in either language. The read-backs
   * are the object's whole output surface, so a reset that left them
   * standing would report the PREVIOUS burst's verdict for a burst that
   * had not been demodulated yet -- the worst possible failure for a
   * per-burst object, and completely silent. */
  {
    uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
    for (size_t i = 0; i < ACQ_SF; i++)
      acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
    for (size_t i = 0; i < DATA_SF; i++)
      dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
    for (size_t i = 0; i < PAYLOAD; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    const size_t cap
        = (ACQ_SF * ACQ_REPS + (SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF) * SPC
          + 16;
    float complex *y = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    if (y)
      {
        const double f0 = 0.012;
        uint8_t      bits[FRAME_SYMS];
        size_t       n = build_burst_ex (y, acode, dcode, payload, f0, 0, -1);
        burst_demod_state_t *d = burst_demod_create (
            dcode, DATA_SF, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10);
        DP_CHECK (d != NULL);
        if (d)
          {
            burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
            burst_demod_set_sync (d, SYNC, SYNC_LEN);
            burst_demod_set_prior (d, f0, 0);
            DP_CHECK (burst_demod_demod (d, y, n, bits, FRAME_SYMS)
                      == FRAME_SYMS);
            DP_CHECK_MSG (frame_ok (bits, FRAME_SYMS),
                          "the burst this reset is tested against really "
                          "decoded, or the preconditions below are vacuous");
            /* the precondition: they are NON-zero before the reset, or the
               assertions below pass on state that was already clear */
            DP_CHECK (d->n_symbols > 0);
            DP_CHECK (d->est_freq_hz != 0.0);

            burst_demod_reset (d);
            DP_CHECK (d->n_symbols == 0);
            DP_CHECK (d->frame_offset == 0);
            DP_CHECK (d->est_freq_hz == 0.0);
            DP_CHECK (d->est_rate_hz == 0.0);
            DP_CHECK (d->est_snr_db == 0.0);
            burst_demod_destroy (d);
          }
        free (y);
      }
  }

  /* ── a frame is however many symbols the caller says ─────────────────
   *
   * The frame's length used to be `sync + payload + CRC-16`, computed
   * inside this object -- so a burst sent WITHOUT a trailer was measured
   * against sixteen bits nobody transmitted, and reported invalid despite
   * decoding perfectly. `frame_syms` is now a number the caller states,
   * and a shorter frame is simply a smaller number. There is nothing here
   * to disagree with a transmitter about.
   */
  {
    uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
    for (size_t i = 0; i < ACQ_SF; i++)
      acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
    for (size_t i = 0; i < DATA_SF; i++)
      dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
    for (size_t i = 0; i < PAYLOAD; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    /* The same burst as everywhere else in this file, with the trailer
       simply not transmitted. */
    const size_t   no_crc = SYNC_LEN + PAYLOAD;
    size_t         cap    = (ACQ_SF * ACQ_REPS + no_crc * DATA_SF) * SPC + 16;
    float complex *y      = malloc (cap * sizeof *y);
    DP_REQUIRE (y != NULL);
    size_t n = 0;
    for (size_t r = 0; r < ACQ_REPS; r++)
      for (size_t c = 0; c < ACQ_SF; c++)
        for (size_t k = 0; k < SPC; k++)
          y[n++] = csign (acode[c]);
    for (size_t j = 0; j < SYNC_LEN; j++)
      n = put_symbol (y, n, dcode, SYNC[j]);
    for (size_t j = 0; j < PAYLOAD; j++)
      n = put_symbol (y, n, dcode, payload[j]);

    burst_demod_state_t *d = burst_demod_create (
        dcode, DATA_SF, SPC, CHIP_RATE, 0.0, 0.0, no_crc, 10);
    DP_REQUIRE (d != NULL);
    burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
    burst_demod_set_sync (d, SYNC, SYNC_LEN);
    burst_demod_set_prior (d, 0.0, 0);

    uint8_t bits[FRAME_SYMS];
    size_t  nb = burst_demod_demod (d, y, n, bits, FRAME_SYMS);
    DP_CHECK_MSG (nb == no_crc,
                  "the caller asked for a shorter frame and got one");
    size_t errs = 0;
    for (size_t i = 0; i < SYNC_LEN; i++)
      if (bits[i] != SYNC[i])
        errs++;
    for (size_t i = 0; i < PAYLOAD; i++)
      if (bits[SYNC_LEN + i] != payload[i])
        errs++;
    DP_CHECK_MSG (errs == 0, "...bit-exactly, sync word included");
    DP_CHECK_MSG (burst_demod_llrs_max_out (d, 1) == no_crc,
                  "and the soft twin is the same length");
    float sl[FRAME_SYMS];
    DP_CHECK (burst_demod_llrs (d, 1, sl, FRAME_SYMS) == no_crc);
    size_t soft_bad = 0;
    for (size_t i = 0; i < no_crc; i++)
      if (((sl[i] < 0.0f) ? 1u : 0u) != bits[i])
        soft_bad++;
    DP_CHECK_MSG (soft_bad == 0,
                  "...and carries the same decisions, one per symbol");
    burst_demod_destroy (d);
    free (y);
  }

  /* ── the soft bits, and the one decision rule ─────────────────────────
   *
   * `crealf(sym * derot)` is the LLR up to a scale, and it was sliced to a
   * bit and freed. What has to be true of the kept version is that it is
   * the SAME decision: `L < 0` reproduces demod()'s own bits, over the
   * whole frame rather than the payload alone (doppler#1018).
   */
  {
    uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
    for (size_t i = 0; i < ACQ_SF; i++)
      acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
    for (size_t i = 0; i < DATA_SF; i++)
      dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
    for (size_t i = 0; i < PAYLOAD; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    size_t cap
        = (ACQ_SF * ACQ_REPS + (SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF) * SPC
          + 16;
    float complex *y = malloc (cap * sizeof *y);
    DP_REQUIRE (y != NULL);
    size_t n = build_burst (y, acode, dcode, payload, 0.0, 0.0);

    burst_demod_state_t *d = burst_demod_create (
        dcode, DATA_SF, SPC, CHIP_RATE, 0.0, 0.0, FRAME_SYMS, 10);
    DP_REQUIRE (d != NULL);
    burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
    burst_demod_set_sync (d, SYNC, SYNC_LEN);
    burst_demod_set_prior (d, 0.0, 0);

    uint8_t bits[FRAME_SYMS];
    DP_CHECK (burst_demod_demod (d, y, n, bits, FRAME_SYMS) == FRAME_SYMS);

    const size_t nl = burst_demod_llrs_max_out (d, 1);
    DP_CHECK_MSG (nl == FRAME_SYMS,
                  "one LLR per FRAME symbol, not per payload bit");
    float *llr = malloc (nl * sizeof *llr);
    DP_REQUIRE (llr != NULL);
    DP_CHECK (burst_demod_llrs (d, 1, llr, nl) == nl);

    size_t disagree = 0;
    for (size_t i = 0; i < FRAME_SYMS; i++)
      if (((llr[i] < 0.0f) ? 1u : 0u) != bits[i])
        disagree++;
    DP_CHECK_MSG (disagree == 0,
                  "the soft bits and the hard bits are one decision rule "
                  "seen twice, not two rules that happen to agree");
    /* Every symbol the frame occupies, sync word included -- which is what
       makes the LLRs usable by a code whose cover reached that far. */
    size_t sync_bad = 0;
    for (size_t i = 0; i < SYNC_LEN; i++)
      if (((llr[i] < 0.0f) ? 1u : 0u) != SYNC[i])
        sync_bad++;
    DP_CHECK_MSG (sync_bad == 0, "the frame's leading symbols are soft too");
    /* And they are SCALED: a noise estimate the caller can read back. */
    DP_CHECK_MSG (d->est_n0 > 0.0, "the LLR scale is published, not hidden");

    free (llr);
    burst_demod_destroy (d);
    free (y);
  }

  DP_TEST_END ("test_burst_demod_core");
}
