#include "doppler/burst_demod/burst_demod_core.h"
#include "doppler/dp_complex.h"
#include "doppler/dp_crc16.h"
#include "doppler/wfm/wfm_frame.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACQ_SF 500
#define ACQ_REPS 5
#define DATA_SF 50
#define SPC 4
#define SYNC_LEN 13
#define PAYLOAD 64
#define CRC_BITS 16
#define CHIP_RATE 1.0e6

/* Scratch capacity for a frame's symbols and for the burst built from it. A
 * generous constant, not a derived length: the frame's own length comes from
 * its description's layout (frame_syms() below), and every buffer here is
 * merely big enough to hold any frame this file builds. */
#define MAX_FRAME 256u
#define BURST_CAP ((ACQ_SF * ACQ_REPS + MAX_FRAME * DATA_SF) * SPC + 64)

/* Barker-13 as 0/1 (0 -> +1, 1 -> -1). */
static const uint8_t SYNC[SYNC_LEN]
    = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0 };

/* MUST returns from the CALLER, which is wrong inside a void helper;
 * a helper that cannot continue records the failure and leaves. */
#define MUST(cond)                                                            \
  do                                                                          \
    {                                                                         \
      const int must_ok_ = (cond) ? 1 : 0;                                    \
      DP_CHECK (must_ok_);                                                    \
      if (!must_ok_)                                                          \
        exit (1);                                                             \
    }                                                                         \
  while (0)

static float
csign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

/* ── One description, both ends ───────────────────────────────────────────
 *
 * The frame is `sync | payload | CRC-16`, described ONCE as a
 * wfm_frame_desc_t. The receiver is built from that description
 * (create_desc derives the sync word from field 0 and the length from the
 * layout), and the burst it is fed is spread from a description too
 * (dp_wfm_dsss_desc_chips). Nothing here counts symbols by hand. */

static void
frame_desc (wfm_frame_desc_t *f, const uint8_t *payload, int crc)
{
  const wfm_seq_t sync
      = { .kind = WFM_SEQ_LITERAL, .bits = SYNC, .len = SYNC_LEN };
  const wfm_seq_t pay
      = { .kind = WFM_SEQ_LITERAL, .bits = payload, .len = PAYLOAD };
  MUST (dp_wfm_frame_fixed (f, NULL, 0, &sync, &pay, crc) == 0);
}

static void
layout_of (const wfm_frame_desc_t *f, wfm_frame_desc_layout_t *l)
{
  MUST (dp_wfm_frame_desc_layout (f, l) == 0);
}

/* The receiver's frame length in symbols, sync included: the layout's. */
static size_t
desc_syms (const wfm_frame_desc_t *f)
{
  wfm_frame_desc_layout_t l;
  layout_of (f, &l);
  return l.frame_bits;
}

/* Length of the standard sync | payload | CRC-16 frame. */
static size_t
frame_syms (void)
{
  static const uint8_t zero[PAYLOAD] = { 0 };
  wfm_frame_desc_t     f;
  frame_desc (&f, zero, 1);
  return desc_syms (&f);
}

/* The caller's half of the split: does the frame's own trailer match its
 * own payload? This object stops at decisions (doppler#1022), so every
 * assertion that used to read `frame_valid` reads this instead — the same
 * arithmetic, at the layer that owns it. `wfm.Frame.deframe()` is the
 * shipped form; a few lines here keep this test linked against burst
 * objects only. Where the payload and the trailer sit comes from the
 * description's layout, not from adding lengths up. */
static int
frame_ok (const uint8_t *frame, size_t n)
{
  static const uint8_t    zero[PAYLOAD] = { 0 };
  wfm_frame_desc_t        f;
  wfm_frame_desc_layout_t l;
  frame_desc (&f, zero, 1);
  layout_of (&f, &l);
  if (n < l.frame_bits)
    return 0;
  uint16_t rx = 0;
  for (size_t j = 0; j < l.field_bits[2]; j++)
    rx = (uint16_t)((rx << 1) | (frame[l.field_off[2] + j] & 1u));
  return rx == dp_crc16_ccitt (frame + l.field_off[1], l.field_bits[1]);
}

/* A demodulator built from the description the transmitter spread. */
static dp_burst_demod_state_t *
make_demod_for (const uint8_t *dcode, const wfm_frame_desc_t *rx,
                double max_rate, size_t segs)
{
  const char             *why = NULL;
  dp_burst_demod_state_t *d   = dp_burst_demod_create_desc (
      dcode, DATA_SF, rx, SPC, CHIP_RATE, 0.0, max_rate, segs, &why);
  if (!d)
    printf ("create_desc refused: %s\n", why ? why : "(no reason)");
  MUST (d != NULL);
  return d;
}

/* The standard receiver: sync-first, sync | payload | CRC-16. The payload
 * bits are irrelevant to a receiver (only its length is), so a zero payload
 * stands in for whatever the transmitter sends. */
static dp_burst_demod_state_t *
make_demod (const uint8_t *dcode, double max_rate, size_t segs)
{
  static const uint8_t zero[PAYLOAD] = { 0 };
  wfm_frame_desc_t     rx;
  frame_desc (&rx, zero, 1);
  return make_demod_for (dcode, &rx, max_rate, segs);
}

/* Spread the whole burst a TRANSMIT description names (the unspread
 * preamble, then the spread frame), hold each chip SPC samples, and apply
 * the carrier exp(j2π(f0·n + ½μ·n²)). Returns the sample count. */
static size_t
burst_from_desc (float _Complex *y, const uint8_t *acode, const uint8_t *dcode,
                 const wfm_frame_desc_t *tx, double f0, double mu)
{
  static uint8_t chips[BURST_CAP / SPC];
  const size_t   nc = dp_wfm_dsss_desc_chips (
      tx, NULL, acode, ACQ_SF, ACQ_REPS, dcode, DATA_SF, chips, sizeof chips);
  MUST (nc > 0 && nc * SPC <= BURST_CAP);
  size_t n = 0;
  for (size_t c = 0; c < nc; c++)
    for (size_t k = 0; k < SPC; k++)
      y[n++] = csign (chips[c]);

  for (size_t i = 0; i < n; i++)
    {
      double ph
          = 2.0 * M_PI * (f0 * (double)i + 0.5 * mu * (double)i * (double)i);
      y[i] *= (float)cos (ph) + (float)sin (ph) * I;
    }
  return n;
}

/* The standard burst: preamble + sync|payload|crc, carrier on top. */
static size_t
build_burst (float _Complex *y, const uint8_t *acode, const uint8_t *dcode,
             const uint8_t *payload, double f0, double mu)
{
  wfm_frame_desc_t f;
  frame_desc (&f, payload, 1);
  return burst_from_desc (y, acode, dcode, &f, f0, mu);
}

/* As build_burst(), but with `filler` data symbols BEFORE the sync word, and
 * an optional payload bit transmitted wrong AFTER the CRC was computed (so
 * the trailer no longer matches). Both are what the read-back claims below
 * need: frame_offset is only meaningful when the sync is NOT at 0, and
 * frame_valid's negative case needs a frame that ARRIVES and fails.
 *
 * Each is a different TRANSMIT description; the receiver's stays sync-first.
 *   filler:     [filler | sync | payload | crc]
 *   corrupt_at: [sync | payload' | crc], all literal -- payload' has the bit
 *               flipped and crc is the CLEAN frame's trailer, so the wrong
 *               bit is on the wire and the CRC is not recomputed over it. */
static size_t
build_burst_ex (float _Complex *y, const uint8_t *acode, const uint8_t *dcode,
                const uint8_t *payload, double f0, size_t filler,
                int corrupt_at)
{
  uint8_t fill[MAX_FRAME];
  for (size_t j = 0; j < filler; j++)
    fill[j] = (uint8_t)((j * 5u + 1u) & 1u);
  const wfm_seq_t fseq
      = { .kind = WFM_SEQ_LITERAL, .bits = fill, .len = filler };
  const wfm_seq_t sync
      = { .kind = WFM_SEQ_LITERAL, .bits = SYNC, .len = SYNC_LEN };

  wfm_frame_desc_t tx = { 0 };
  if (corrupt_at < 0)
    {
      const wfm_seq_t pay
          = { .kind = WFM_SEQ_LITERAL, .bits = payload, .len = PAYLOAD };
      if (filler)
        MUST (dp_wfm_frame_add_field (&tx, "filler", &fseq, 1) >= 0);
      MUST (dp_wfm_frame_add_field (&tx, "sync", &sync, 1) >= 0);
      MUST (dp_wfm_frame_add_field (&tx, "payload", &pay, 1) >= 0);
      MUST (dp_wfm_frame_add_derived (&tx, "crc", CRC_BITS) >= 0);
      MUST (dp_wfm_frame_add_stage (&tx, WFM_STAGE_CRC16, "payload", "crc")
            >= 0);
    }
  else
    {
      /* The clean frame's bits give the trailer; the wrong bit is then
         transmitted in front of it. */
      wfm_frame_desc_t        clean;
      wfm_frame_desc_layout_t l;
      uint8_t                 bits[MAX_FRAME];
      frame_desc (&clean, payload, 1);
      layout_of (&clean, &l);
      MUST (dp_wfm_frame_assemble (&clean, NULL, bits, sizeof bits)
            == l.frame_bits);
      uint8_t wrong[PAYLOAD];
      memcpy (wrong, payload, PAYLOAD);
      wrong[corrupt_at] = (uint8_t)(wrong[corrupt_at] ^ 1u);

      const wfm_seq_t pay
          = { .kind = WFM_SEQ_LITERAL, .bits = wrong, .len = PAYLOAD };
      const wfm_seq_t crc = { .kind = WFM_SEQ_LITERAL,
                              .bits = bits + l.field_off[2],
                              .len  = l.field_bits[2] };
      if (filler)
        MUST (dp_wfm_frame_add_field (&tx, "filler", &fseq, 1) >= 0);
      MUST (dp_wfm_frame_add_field (&tx, "sync", &sync, 1) >= 0);
      MUST (dp_wfm_frame_add_field (&tx, "payload", &pay, 1) >= 0);
      MUST (dp_wfm_frame_add_field (&tx, "crc", &crc, 1) >= 0);
      return burst_from_desc (y, acode, dcode, &tx, f0, 0.0);
    }
  return burst_from_desc (y, acode, dcode, &tx, f0, 0.0);
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

  const size_t    cap = BURST_CAP;
  float _Complex *y   = malloc (cap * sizeof *y);
  DP_REQUIRE (y != NULL);

  /* ONE description: the transmitter spreads it, the receiver is built from
     it. The receiver derives its sync word (field 0) and its length from the
     layout; nothing here states either by hand. */
  wfm_frame_desc_t f;
  frame_desc (&f, payload, 1);
  const size_t n = burst_from_desc (y, acode, dcode, &f, f0, mu);

  dp_burst_demod_state_t *d = make_demod_for (dcode, &f, max_rate, 10);
  DP_CHECK (d != NULL);
  DP_CHECK_MSG (d->frame_syms == desc_syms (&f) && d->sync_len == SYNC_LEN,
                "the receiver took its length and its sync word from the "
                "description");
  memset (&f, 0xA5, sizeof f); /* the demodulator kept none of it */
  dp_burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
  dp_burst_demod_set_prior (d, f0_prior, 0);

  uint8_t bits[MAX_FRAME];
  size_t  nb = dp_burst_demod_demod (d, y, n, bits, frame_syms ());
  DP_CHECK (nb == frame_syms ());
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
  /* ...and the trailer it received is the one dp_crc16_ccitt() computes over
     the payload it received -- the whole point of the trailer, verified where
     a caller would verify it. */
  DP_CHECK_MSG (
      frame_ok (bits, nb),
      "the trailer it received is the one dp_crc16_ccitt() computes over "
      "the payload it received");

  printf ("  %-10s f0=%.4f(prior %.4f) mu=%.2e | est f=%.1fHz r=%.2eHz/s "
          "cn0=%.1fdBHz tau=%+.3fchip off=%zu errs=%zu\n",
          name, f0, f0_prior, mu, d->est_freq_hz, d->est_rate_hz,
          d->est_cn0_dbhz, d->est_timing_chips, d->frame_offset, errs);
  dp_burst_demod_destroy (d);
  free (y);
  return 0;
}

/* A refused description is a NULL demodulator whose `why` names the fix.
 *
 * What used to be "the caller got frame_syms or the sync word wrong" is a
 * description refusal now: there is no longer a number to get wrong, only a
 * description that does not say where the sync word is. Each is built the way
 * test_wfm_frame.c's test_desc_rx builds it, and checked through `why`. */
static int
run_desc_refusals (void)
{
  uint8_t dc[DATA_SF];
  for (size_t i = 0; i < DATA_SF; i++)
    dc[i] = (uint8_t)(i & 1u);
  const wfm_seq_t sync
      = { .kind = WFM_SEQ_LITERAL, .bits = SYNC, .len = SYNC_LEN };
  const wfm_seq_t data = { .kind = WFM_SEQ_DATA, .len = 8 };
  const wfm_seq_t pre
      = { .kind = WFM_SEQ_LITERAL, .bits = SYNC, .len = SYNC_LEN };

  struct
  {
    const char      *what;
    const char      *needle;
    wfm_frame_desc_t d;
  } bad[6];
  size_t nb = 0;

  memset (&bad[nb], 0, sizeof bad[nb]);
  bad[nb].what     = "an empty description";
  bad[nb++].needle = "does not lay out";

  bad[nb].what   = "a data field as field 0: nothing to correlate against";
  bad[nb].needle = "field 0 is the";
  DP_REQUIRE (dp_wfm_frame_fixed (&bad[nb++].d, NULL, 0, NULL, &data, 1) == 0);

  memset (&bad[nb], 0, sizeof bad[nb]);
  bad[nb].what   = "a derived field as field 0";
  bad[nb].needle = "field 0 is the";
  DP_REQUIRE (dp_wfm_frame_add_derived (&bad[nb].d, "crc", 16) == 0);
  DP_REQUIRE (dp_wfm_frame_add_field (&bad[nb].d, "payload", &sync, 1) == 1);
  DP_REQUIRE (
      dp_wfm_frame_add_stage (&bad[nb].d, WFM_STAGE_CRC16, "crc", "crc") >= 0);
  nb++;

  bad[nb].what   = "a field 0 named preamble";
  bad[nb].needle = "preamble";
  DP_REQUIRE (dp_wfm_frame_fixed (&bad[nb++].d, &pre, 2, &sync, &data, 1)
              == 0);

  memset (&bad[nb], 0, sizeof bad[nb]);
  bad[nb].what   = "a stage covering the sync word";
  bad[nb].needle = "covers";
  DP_REQUIRE (dp_wfm_frame_add_field (&bad[nb].d, "sync", &sync, 1) == 0);
  DP_REQUIRE (dp_wfm_frame_add_field (&bad[nb].d, "payload", &sync, 1) == 1);
  DP_REQUIRE (dp_wfm_frame_add_derived (&bad[nb].d, "crc", 16) == 2);
  DP_REQUIRE (
      dp_wfm_frame_add_stage (&bad[nb].d, WFM_STAGE_CRC16, "sync", "crc")
      >= 0);
  nb++;

  bad[nb].what   = "an emitting stage";
  bad[nb].needle = "emits";
  DP_REQUIRE (dp_wfm_frame_fixed (&bad[nb].d, NULL, 0, &sync, &data, 1) == 0);
  const int cst
      = dp_wfm_frame_add_stage (&bad[nb].d, WFM_STAGE_CONV, "sync", "crc");
  DP_REQUIRE (cst >= 0);
  bad[nb].d.stage[cst].emit_num = 2u;
  bad[nb].d.stage[cst].emit_den = 1u;
  nb++;

  for (size_t i = 0; i < nb; i++)
    {
      const char *why = NULL;
      DP_CHECK_MSG (dp_burst_demod_create_desc (dc, DATA_SF, &bad[i].d, SPC,
                                                CHIP_RATE, 0.0, 0.0, 10, &why)
                            == NULL
                        && why && strstr (why, bad[i].needle),
                    bad[i].what);
      if (why && !strstr (why, bad[i].needle))
        printf ("    (%s: why = \"%s\")\n", bad[i].what, why);
    }

  /* a good description, a bad parameter: the demodulator's own refusal */
  wfm_frame_desc_t f;
  frame_desc (&f, SYNC, 1);
  const char *why = NULL;
  DP_CHECK (dp_burst_demod_create_desc (dc, DATA_SF, &f, 0 /* spc */,
                                        CHIP_RATE, 0.0, 0.0, 10, &why)
                == NULL
            && why && strstr (why, "invalid parameter"));
  /* ...and with `why` NULL, still a NULL and not a crash */
  DP_CHECK (dp_burst_demod_create_desc (dc, DATA_SF, &f, 0, CHIP_RATE, 0.0,
                                        0.0, 10, NULL)
            == NULL);
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

  /* Argument validation → NULL (each clause of the create guard), now
     against a GOOD description so it is the parameter that is refused. */
  wfm_frame_desc_t f;
  frame_desc (&f, SYNC, 1);
#define REFUSED(call)                                                         \
  do                                                                          \
    {                                                                         \
      const char             *why_ = NULL;                                    \
      dp_burst_demod_state_t *r_   = (call);                                  \
      DP_CHECK (r_ == NULL && why_ && strstr (why_, "invalid parameter"));    \
      dp_burst_demod_destroy (r_);                                            \
    }                                                                         \
  while (0)
#define WHY &why_
  /* A NULL description is refused, not dereferenced. */
  {
    const char *why_ = NULL;
    DP_CHECK (dp_burst_demod_create_desc (dc, DATA_SF, NULL, SPC, CHIP_RATE, 0,
                                          0, 10, WHY)
              == NULL);
    DP_CHECK (why_ != NULL);
  }
  REFUSED (dp_burst_demod_create_desc (NULL, DATA_SF, &f, SPC, CHIP_RATE, 0, 0,
                                       10, WHY));
  REFUSED (
      dp_burst_demod_create_desc (dc, 0, &f, SPC, CHIP_RATE, 0, 0, 10, WHY));
  REFUSED (dp_burst_demod_create_desc (dc, DATA_SF, &f, 0, CHIP_RATE, 0, 0, 10,
                                       WHY));
  REFUSED (
      dp_burst_demod_create_desc (dc, DATA_SF, &f, SPC, 0.0, 0, 0, 10, WHY));
  REFUSED (dp_burst_demod_create_desc (dc, DATA_SF, &f, SPC, CHIP_RATE, 0,
                                       -1.0, 10, WHY));
  REFUSED (dp_burst_demod_create_desc (dc, DATA_SF, &f, SPC, CHIP_RATE, 0, 0,
                                       0, WHY));
#undef WHY
#undef REFUSED

  dp_burst_demod_destroy (NULL); /* no-op on NULL */

  dp_burst_demod_state_t *d = make_demod (dc, 0.0, 10);
  DP_CHECK (d != NULL);
  dp_burst_demod_set_preamble (d, NULL, 0, 0); /* guard: ignored */
  dp_burst_demod_set_preamble (d, ac, ACQ_SF, ACQ_REPS);
  dp_burst_demod_set_preamble (d, ac, ACQ_SF,
                               ACQ_REPS); /* re-arm: frees old ppe */

  /* Too-short input → clean failure: no frame, so no bits and no LLRs. */
  float _Complex tiny[8] = { 0 };
  uint8_t eb[MAX_FRAME];
  dp_burst_demod_set_prior (d, 0.0, 0);
  DP_CHECK (dp_burst_demod_demod (d, tiny, 8, eb, frame_syms ()) == 0);
  float el[MAX_FRAME];
  DP_CHECK (dp_burst_demod_llrs (d, 1, el, frame_syms ()) == 0);
  float _Complex esym[MAX_FRAME];
  DP_CHECK_MSG (dp_burst_demod_symbols (d, 1, esym, frame_syms ()) == 0,
                "no frame yet: the constellation read-back is empty, not "
                "stale");
  /* The capacity accessor answers from the CONFIGURATION, so it reports a
     frame's worth even when the last call produced none — that is what a
     caller sizes a buffer with, before there is anything to size for. */
  DP_CHECK_MSG (dp_burst_demod_llrs_max_out (d, 1) == frame_syms (),
                "llrs_max_out is the frame's length, not the last call's");
  DP_CHECK_MSG (dp_burst_demod_symbols_max_out (d, 1) == frame_syms (),
                "symbols_max_out matches llrs_max_out -- the two read-backs "
                "describe ONE frame at one length");
  DP_CHECK_MSG (dp_burst_demod_demod_max_out (d) == frame_syms (),
                "and demod_max_out agrees with it");
  dp_burst_demod_destroy (d);

  /* est_segments > acq_sf forces the per-segment chip clamp (Lseg >= 1). */
  dp_burst_demod_state_t *d2 = make_demod (dc, 0.0, ACQ_SF + 100);
  DP_CHECK (d2 != NULL);
  dp_burst_demod_set_preamble (d2, ac, ACQ_SF, 1);
  dp_burst_demod_destroy (d2);
  return 0;
}

int
main (void)
{
  (void)run_edge_cases ();
  if (run_desc_refusals ())
    return 1;

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

    const size_t    cap = BURST_CAP;
    float _Complex *y   = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    if (y)
      {
        const double f0 = 0.012;
        uint8_t      bits[MAX_FRAME];

        /* Baseline: clean, so the negative results below are not simply a
           demodulator that never works. */
        size_t n = build_burst_ex (y, acode, dcode, payload, f0, 0, -1);
        dp_burst_demod_state_t *d = make_demod (dcode, 0.0, 10);
        DP_CHECK (d != NULL);
        if (d)
          {
            dp_burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
            dp_burst_demod_set_prior (d, f0, 0);
            DP_CHECK (dp_burst_demod_demod (d, y, n, bits, frame_syms ())
                      == frame_syms ());
            DP_CHECK (frame_ok (bits, frame_syms ()));
            dp_burst_demod_destroy (d);
          }

        /* Three flipped positions, including both ends of the payload. */
        const int spots[3] = { 0, 17, PAYLOAD - 1 };
        for (int si = 0; si < 3; si++)
          {
            n = build_burst_ex (y, acode, dcode, payload, f0, 0, spots[si]);
            dp_burst_demod_state_t *b = make_demod (dcode, 0.0, 10);
            DP_CHECK (b != NULL);
            if (b)
              {
                dp_burst_demod_set_preamble (b, acode, ACQ_SF, ACQ_REPS);
                dp_burst_demod_set_prior (b, f0, 0);
                size_t nb
                    = dp_burst_demod_demod (b, y, n, bits, frame_syms ());
                /* The frame is still demodulated -- this is not the
                   too-short path. */
                DP_CHECK (nb == frame_syms ());
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
                DP_CHECK (!frame_ok (bits, nb));
                dp_burst_demod_destroy (b);
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

    const size_t    cap = BURST_CAP;
    float _Complex *y   = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    if (y)
      {
        const double f0       = 0.012;
        const size_t fills[3] = { 0, 3, 9 };
        uint8_t      bits[MAX_FRAME];
        size_t       prev_syms = 0;
        for (int fi = 0; fi < 3; fi++)
          {
            size_t n
                = build_burst_ex (y, acode, dcode, payload, f0, fills[fi], -1);
            dp_burst_demod_state_t *d = make_demod (dcode, 0.0, 10);
            DP_CHECK (d != NULL);
            if (d)
              {
                dp_burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
                dp_burst_demod_set_prior (d, f0, 0);
                DP_CHECK (dp_burst_demod_demod (d, y, n, bits, frame_syms ())
                          == frame_syms ());
                DP_CHECK_MSG (frame_ok (bits, frame_syms ()),
                              "a frame behind filler symbols still checks "
                              "out — the offset is found, not guessed");
                /* the sync sits exactly `filler` symbols in */
                DP_CHECK (d->frame_offset == fills[fi]);
                /* and the whole data section was despread */
                DP_CHECK (d->n_symbols == fills[fi] + frame_syms ());
                DP_CHECK (d->n_symbols > prev_syms || fi == 0);
                prev_syms = d->n_symbols;
                dp_burst_demod_destroy (d);
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

    const size_t    cap = BURST_CAP;
    float _Complex *y   = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    if (y)
      {
        const double f0 = 0.012;
        uint8_t      bits[MAX_FRAME];
        size_t       n = build_burst_ex (y, acode, dcode, payload, f0, 0, -1);
        dp_burst_demod_state_t *d = make_demod (dcode, 0.0, 10);
        DP_CHECK (d != NULL);
        if (d)
          {
            dp_burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
            dp_burst_demod_set_prior (d, f0, 0);
            DP_CHECK (dp_burst_demod_demod (d, y, n, bits, frame_syms ())
                      == frame_syms ());
            DP_CHECK_MSG (frame_ok (bits, frame_syms ()),
                          "the burst this reset is tested against really "
                          "decoded, or the preconditions below are vacuous");
            /* the precondition: they are NON-zero before the reset, or the
               assertions below pass on state that was already clear */
            DP_CHECK (d->n_symbols > 0);
            DP_CHECK (d->est_freq_hz != 0.0);

            dp_burst_demod_reset (d);
            DP_CHECK (d->n_symbols == 0);
            DP_CHECK (d->frame_offset == 0);
            DP_CHECK (d->est_freq_hz == 0.0);
            DP_CHECK (d->est_rate_hz == 0.0);
            DP_CHECK (d->est_cn0_dbhz == 0.0);
            DP_CHECK (d->est_timing_chips == 0.0);
            dp_burst_demod_destroy (d);
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
    wfm_frame_desc_t f;
    frame_desc (&f, payload, 0); /* sync | payload, no CRC-16 */
    const size_t    no_crc = desc_syms (&f);
    const size_t    cap    = BURST_CAP;
    float _Complex *y      = malloc (cap * sizeof *y);
    DP_REQUIRE (y != NULL);
    const size_t n = burst_from_desc (y, acode, dcode, &f, 0.0, 0.0);

    dp_burst_demod_state_t *d = make_demod_for (dcode, &f, 0.0, 10);
    DP_REQUIRE (d != NULL);
    dp_burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
    dp_burst_demod_set_prior (d, 0.0, 0);

    uint8_t bits[MAX_FRAME];
    size_t  nb = dp_burst_demod_demod (d, y, n, bits, frame_syms ());
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
    DP_CHECK_MSG (dp_burst_demod_llrs_max_out (d, 1) == no_crc,
                  "and the soft twin is the same length");
    float sl[MAX_FRAME];
    DP_CHECK (dp_burst_demod_llrs (d, 1, sl, frame_syms ()) == no_crc);
    size_t soft_bad = 0;
    for (size_t i = 0; i < no_crc; i++)
      if (((sl[i] < 0.0f) ? 1u : 0u) != bits[i])
        soft_bad++;
    DP_CHECK_MSG (soft_bad == 0,
                  "...and carries the same decisions, one per symbol");
    dp_burst_demod_destroy (d);
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

    const size_t    cap = BURST_CAP;
    float _Complex *y   = malloc (cap * sizeof *y);
    DP_REQUIRE (y != NULL);
    size_t n = build_burst (y, acode, dcode, payload, 0.0, 0.0);

    dp_burst_demod_state_t *d = make_demod (dcode, 0.0, 10);
    DP_REQUIRE (d != NULL);
    dp_burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
    dp_burst_demod_set_prior (d, 0.0, 0);

    uint8_t bits[MAX_FRAME];
    DP_CHECK (dp_burst_demod_demod (d, y, n, bits, frame_syms ())
              == frame_syms ());

    const size_t nl = dp_burst_demod_llrs_max_out (d, 1);
    DP_CHECK_MSG (nl == frame_syms (),
                  "one LLR per FRAME symbol, not per payload bit");
    float *llr = malloc (nl * sizeof *llr);
    DP_REQUIRE (llr != NULL);
    DP_CHECK (dp_burst_demod_llrs (d, 1, llr, nl) == nl);

    size_t disagree = 0;
    for (size_t i = 0; i < frame_syms (); i++)
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

    /* ── the CONSTELLATION the LLRs are the real part of ────────────────
     *
     * It was built either way -- the projection and the noise estimate are
     * both made from it -- and then freed unread, which cost a caller the
     * quadrature (doppler#1087). After derotation the real axis carries the
     * signal and the imaginary axis carries noise alone, so Q is the only
     * place a phase-coherence problem shows: measured through this object, a
     * Doppler rate of 1e5 Hz/s raises Q/I 41x while est_cn0_dbhz, est_rate_hz
     * and the bits are all unchanged. */
    float _Complex *sy = malloc (nl * sizeof *sy);
    DP_REQUIRE (sy != NULL);
    DP_CHECK_MSG (dp_burst_demod_symbols (d, 1, sy, nl) == nl,
                  "the constellation spans the frame, as the LLRs do");

    size_t sign_bad = 0, scale_bad = 0;
    for (size_t i = 0; i < nl; i++)
      {
        if (((crealf (sy[i]) < 0.0f) ? 1u : 0u) != bits[i])
          sign_bad++;
        /* llr = 4*Re/est_n0 -- one projection, reported twice. Checking the
           RELATION rather than either value keeps this a statement about the
           two read-backs agreeing, not about the demapper's constant. */
        const float want = 4.0f * crealf (sy[i]) / (float)d->est_n0;
        const float tol  = 1e-3f * (fabsf (want) + 1.0f);
        if (fabsf (llr[i] - want) > tol)
          scale_bad++;
      }
    DP_CHECK_MSG (sign_bad == 0,
                  "Re(symbol) carries the same decision the bits do");
    DP_CHECK_MSG (scale_bad == 0,
                  "llrs() is Re(symbols) scaled by est_n0 -- the same "
                  "projection reported twice, not two computations");

    /* This fixture is effectively noiseless (snr ~71 dB), so the honest
       statement here is that the constellation is TIGHT: derotated onto the
       real axis with almost nothing in quadrature. A constellation that was
       not derotated, or was the raw symbols, would fail this. How Q behaves
       when there IS an impairment needs a noisy scene and is asserted on the
       Python side, where one can be composed. */
    double qe = 0.0, ie = 0.0;
    for (size_t i = 0; i < nl; i++)
      {
        qe += (double)cimagf (sy[i]) * (double)cimagf (sy[i]);
        ie += (double)crealf (sy[i]) * (double)crealf (sy[i]);
      }
    DP_CHECK_MSG (ie > 0.0 && qe < 0.01 * ie,
                  "a clean burst derotates onto the real axis -- the "
                  "constellation is the derotated one, not the raw symbols");

    free (sy);
    free (llr);
    dp_burst_demod_destroy (d);
    free (y);
  }

  /* ── est_cn0_dbhz is a CHANNEL quantity: right, and flat ─────────────
   *
   * Its predecessor est_snr_db was the preamble estimator's spectral
   * prominence and could be compared with nothing: it carried the coherent
   * processing gain, read 59 dB at a 25 dB input, and moved 33 dB with the
   * burst start alone at a fixed channel (doppler#1304).
   *
   * So all three of its failures are pinned here, against a channel whose
   * C/N0 this test sets: the VALUE, the flatness across a sub-chip start
   * error, and independence of a segmentation the estimate has no business
   * depending on. Each seeds noise itself rather than reusing run_case, so
   * the true C/N0 is known rather than inferred.
   *
   * C/N0 = Es/N0 * Rs, with Rs = chip_rate/DATA_SF: a density, so it is the
   * same number whatever `spc` the front end runs at. */
  {
    uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
    for (size_t i = 0; i < ACQ_SF; i++)
      acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
    for (size_t i = 0; i < DATA_SF; i++)
      dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
    for (size_t i = 0; i < PAYLOAD; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    const double    sym_rate = CHIP_RATE / (double)DATA_SF;
    const size_t    cap      = BURST_CAP;
    float _Complex *y        = malloc (cap * sizeof *y);
    float _Complex *z        = malloc (cap * sizeof *z);
    uint8_t        *bits     = malloc (MAX_FRAME);
    DP_REQUIRE (y != NULL && z != NULL && bits != NULL);

    /* One burst at a stated Es/N0, started `lead` samples late, demodulated
       with `segs` estimator segments. Returns the reported C/N0, or -1e9
       when no frame came back. */
    double   got_cn0 = 0.0, got_tau = 0.0;
    uint32_t seed_state = 0;
#define CN0_RUN(es_n0_db, lead, segs)                                         \
  do                                                                          \
    {                                                                         \
      const double f0_ = 0.012;                                               \
      size_t       n_  = build_burst (y, acode, dcode, payload, f0_, 0.0);    \
      /* per-sample sigma from Es/N0: Es is DATA_SF*SPC samples of unit       \
         power, and the noise is complex with sigma^2 total. */               \
      double sig_                                                             \
          = pow (10.0, -((es_n0_db) - 10.0 * log10 ((double)(DATA_SF * SPC))) \
                           / 20.0);                                           \
      seed_state = 12345u;                                                    \
      for (size_t k_ = 0; k_ < (lead); k_++)                                  \
        z[k_] = 0.0f;                                                         \
      for (size_t k_ = 0; k_ < n_; k_++)                                      \
        z[k_ + (lead)] = y[k_];                                               \
      /* dp_cgauss is E|z|^2 = 1, so sigma scales straight to total noise     \
         power -- the shared harness owns the convention. */                  \
      for (size_t k_ = 0; k_ < n_ + (lead); k_++)                             \
        z[k_] += (float)sig_ * dp_cgauss (&seed_state);                       \
      dp_burst_demod_state_t *dd_ = make_demod (dcode, 0.0, (segs));          \
      DP_REQUIRE (dd_ != NULL);                                               \
      dp_burst_demod_set_preamble (dd_, acode, ACQ_SF, ACQ_REPS);             \
      dp_burst_demod_set_prior (dd_, f0_, 0);                                 \
      size_t nb_                                                              \
          = dp_burst_demod_demod (dd_, z, n_ + (lead), bits, frame_syms ());  \
      got_cn0 = (nb_ == frame_syms ()) ? dd_->est_cn0_dbhz : -1e9;            \
      got_tau = dd_->est_timing_chips;                                        \
      dp_burst_demod_destroy (dd_);                                           \
    }                                                                         \
  while (0)

    /* 1. The VALUE, over a 20 dB span. A prominence cannot do this: it has
          the processing gain in it and saturates at the top of the range. */
    for (int es = 10; es <= 30; es += 10)
      {
        const double want = (double)es + 10.0 * log10 (sym_rate);
        CN0_RUN (es, 0, 10);
        if (fabs (got_cn0 - want) >= 2.0)
          printf ("    Es/N0 %d: reported %.1f dB-Hz, true %.1f\n", es,
                  got_cn0, want);
        DP_CHECK_MSG (fabs (got_cn0 - want) < 2.0,
                      "C/N0 must be the CHANNEL's, not the estimator's");
      }

    /* 2. FLAT across a sub-chip start error, which is what a realized SNR
          is not: the same channel read 4.8 dB lower at half a chip before
          the timing term existed. The offset is REPORTED, and reporting it
          is half the point -- a reader can see what was corrected. */
    {
      const double want = 25.0 + 10.0 * log10 (sym_rate);
      for (size_t lead = 0; lead <= SPC / 2; lead++)
        {
          CN0_RUN (25, lead, 10);
          if (fabs (got_cn0 - want) >= 2.0
              || fabs (got_tau - (double)lead / (double)SPC) >= 0.1)
            printf ("    lead %zu: C/N0 %.1f (true %.1f), tau %+.3f chip\n",
                    lead, got_cn0, want, got_tau);
          DP_CHECK_MSG (fabs (got_cn0 - want) < 2.0,
                        "a sub-chip start error must not move a CHANNEL "
                        "C/N0 -- a realized one falls 4.8 dB at half a chip");
          DP_CHECK_MSG (fabs (got_tau - (double)lead / (double)SPC) < 0.1,
                        "the start error the object removed is REPORTED");
        }
    }

    /* 3. INDEPENDENT of est_segments, which only shapes the preamble
          estimator's partials. It used to dominate the answer: est_snr_db
          was read off those partials, and at an acq_sf est_segments does
          not divide the last one ran long and cost 23.8 dB. C/N0 is
          measured on the DATA symbols, so the knob cannot reach it -- that
          SEPARATION is what this pins, not the partial-length defect,
          which remains and is doppler#1305. ACQ_SF here is 500, so 10
          divides it and 32 does not; both must land on the same channel. */
    {
      const double want   = 25.0 + 10.0 * log10 (sym_rate);
      const size_t segs[] = { 5, 10, 32 };
      for (size_t i = 0; i < sizeof segs / sizeof *segs; i++)
        {
          CN0_RUN (25, 0, segs[i]);
          if (fabs (got_cn0 - want) >= 2.0)
            printf ("    est_segments %zu: C/N0 %.1f, true %.1f\n", segs[i],
                    got_cn0, want);
          DP_CHECK_MSG (fabs (got_cn0 - want) < 2.0,
                        "est_segments shapes the preamble estimator's "
                        "partials and must not move the channel's C/N0");
        }
    }
#undef CN0_RUN
    free (y);
    free (z);
    free (bits);
  }

  DP_TEST_END ("test_burst_demod_core");
}
