/*
 * test_wfm_compose.c — multi-segment composer (Phase B).
 *
 * Verifies segment sequencing, gaps (noise-floor by default, zeros when
 * clean/off), delays, once-through completion, and repeat looping — all
 * over the reused Phase-A synth engine.
 */
#include "doppler/ccsds_tm/ccsds_tm.h"
#include "doppler/ccsds_tm/ccsds_tm_frame.h"
#include "doppler/dp_crc16.h"
#include "doppler/dp_interleave.h"
#include "doppler/gold/gold_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h" /* WFM_SOURCE_DEFAULTS */
#include "doppler/wfm/wfm_dsp.h"
#include "doppler/wfm/wfm_frame.h" /* the descriptor the unspread frame section reads */
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_test.h"

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h> /* getpid, under its POSIX name; the UCRT has no <unistd.h> */
#else
#include <unistd.h>
#endif

/* The standalone-Synth half of the shared bridge. It has no header of its own
   — the Python binding declares it the same way (wfm_compose_ext.c) — and the
   unspread-frame section below asserts it refuses exactly what the composer's
   path refuses, which is the only way "they share the attach" is checkable. */
extern dp_wfm_synth_state_t *dp_wfm_source_to_synth (const wfm_source_t *,
                                                     double);

/* ── the SEAM functions, whose whole job is that two faces agree ─────────
 *
 * This is the composition, so its subject is the seam rather than a re-run
 * of the parts (Synth, Writer, Reader and Frame are certified separately
 * and this cites them rather than re-deriving them).
 *
 * Four functions in wfm_compose.h exist for no reason except to stop the
 * faces drifting, and each says so in its own doc. Two of them had ZERO
 * mentions in any C test in the tree:
 *
 *   dp_wfm_source_create_snr    "the one create-time entry point shared by the
 *                            composer and the standalone-Synth bridge, so
 *                            every face agrees to the bit"      -- 0 tests
 *   dp_wfm_source_attach_frame  "called from the same two places for the same
 *                            reason"                            -- 0 tests
 *
 * A function whose entire purpose is agreement, with nothing asserting the
 * agreement, is the shape this campaign exists to find.
 */
static int
test_the_create_snr_seam (void)
{
  /* ── every non-dsss type passes through UNCHANGED, mode included ─────
     The pre-referral exists for dsss, and for a FRAMED PN-sourced type
     (built as BITS -- test_a_framed_pn_type_sends_its_frame). These
     sources are unframed. For the rest, touching either
     value here would silently move the noise of every composed source,
     and the generator's own conversion would then be applied twice. */
  {
    const int types[] = { WFM_SYNTH_TONE, WFM_SYNTH_NOISE,  WFM_SYNTH_PN,
                          WFM_SYNTH_BPSK, WFM_SYNTH_QPSK,   WFM_SYNTH_CHIRP,
                          WFM_SYNTH_BITS, WFM_SYNTH_SYMBOLS };
    const int modes[] = { 0, 1, 2, 3 };
    for (size_t t = 0; t < sizeof types / sizeof types[0]; t++)
      for (size_t m = 0; m < sizeof modes / sizeof modes[0]; m++)
        {
          wfm_source_t s = { 0 };
          s.type         = types[t];
          s.sps          = 8;
          s.snr_mode     = modes[m];
          int    mode    = -1;
          double got     = dp_wfm_source_create_snr (&s, 1e6, 7.5, &mode);
          DP_REQUIRE_MSG (got == 7.5,
                          "a non-dsss source's SNR passes through");
          DP_REQUIRE_MSG (mode == modes[m],
                          "and so does its mode -- the conversion is for "
                          "dsss alone");
        }
  }

  /* ── a dsss source is pre-referred to fs, because create() cannot ────
     dp_wfm_synth_create() runs before the codes attach, so it cannot know the
     spreading factor its own esno would need. The composer does, and hands
     over an already-fs figure. Scored against the arithmetic the header
     states -- snr - 10log10(span) - not against dp_wfm_snr_over_fs(), which
     is the same conversion and would move with it. */
  {
    const size_t sf  = 8; /* chips per data symbol */
    const int    sps = 2; /* samples per chip      */
    /* burst: the span is sf*sps = 16 samples, so 10log10(16) = 12.0412 dB */
    wfm_source_t b  = { 0 };
    b.type          = WFM_SYNTH_DSSS;
    b.sps           = sps;
    b.snr_mode      = 3; /* esno */
    b.data_code.len = sf;
    int    mode     = -1;
    double got      = dp_wfm_source_create_snr (&b, 1e6, 9.0, &mode);
    DP_REQUIRE_MSG (mode == 1, "a dsss source is handed over as fs");
    DP_REQUIRE_MSG (dp_near (got, 9.0 - 12.041199826559248, 1e-9),
                    "burst span is sf*sps -- 9 dB Es/N0 over 16 samples is "
                    "-3.0412 dB over fs");

    /* continuous: the symbol clock is INDEPENDENT of the code, so the span
       is fs/symbol_rate, NOT sf*sps. The two coincide only in the
       synchronous case the continuous mode exists to avoid, so a fixture
       where they agree would prove nothing -- these deliberately differ. */
    wfm_source_t c = b;
    c.symbol_rate  = 12500.0; /* fs/symbol_rate = 80 samples */
    mode           = -1;
    got            = dp_wfm_source_create_snr (&c, 1e6, 9.0, &mode);
    DP_REQUIRE_MSG (mode == 1, "continuous dsss too");
    DP_REQUIRE_MSG (dp_near (got, 9.0 - 19.030899869919434, 1e-9),
                    "continuous span is fs/symbol_rate (80), NOT sf*sps");
    DP_REQUIRE_MSG (
        !dp_near (9.0 - 19.030899869919434, 9.0 - 12.041199826559248, 1e-6),
        "precondition: the two spans give different answers here");
  }

  /* ── a CLEAN dsss source passes through, or the no-AWGN shortcut dies ─
     dp_wfm_synth_create() skips AWGN entirely at snr >= WFM_SYNTH_SNR_CLEAN.
     Pre-referring a clean figure would push it below the threshold and
     make every clean dsss source pay for noise it did not ask for. */
  {
    wfm_source_t s  = { 0 };
    s.type          = WFM_SYNTH_DSSS;
    s.sps           = 2;
    s.snr_mode      = 3;
    s.data_code.len = 8;
    int    mode     = -1;
    double got
        = dp_wfm_source_create_snr (&s, 1e6, WFM_SYNTH_SNR_CLEAN, &mode);
    DP_REQUIRE_MSG (got >= WFM_SYNTH_SNR_CLEAN,
                    "a clean dsss source stays clean -- the no-AWGN "
                    "shortcut still applies");
  }
  return 0;
}

/* ── the two synth-construction faces agree, in C as well as Python ─────
 *
 * The claim is "every face agrees to the bit", and until now C deferred it
 * ("covered from Python, where that face actually lives") while Python's
 * own three-faces test compares kwargs-Composer, from_json and the CLI --
 * three spellings of the COMPOSER path. The standalone bridge was compared
 * with none of them. It is one memcmp; it belongs here.
 */
static int
test_the_two_faces_agree (void)
{
  const size_t    n = 512;
  float _Complex *a = malloc (n * sizeof *a);
  float _Complex *b = malloc (n * sizeof *b);
  DP_REQUIRE_MSG (a && b, "faces: alloc");

  const int types[] = { WFM_SYNTH_TONE, WFM_SYNTH_BPSK, WFM_SYNTH_QPSK,
                        WFM_SYNTH_PN, WFM_SYNTH_NOISE };
  for (size_t t = 0; t < sizeof types / sizeof types[0]; t++)
    for (int noisy = 0; noisy <= 1; noisy++)
      {
        wfm_source_t s = { 0 };
        s.type         = types[t];
        s.sps          = 8;
        s.seed         = 5;
        s.pn_length    = 9;
        s.snr          = noisy ? 9.0 : 100.0;
        s.snr_mode     = noisy ? 3 : 0;

        dp_wfm_synth_state_t *comp = dp_wfm_compose_build_synth (
            &s, 1e6, n, s.freq, s.snr, s.f_end, 0, 0, 0);
        dp_wfm_synth_state_t *bridge = dp_wfm_source_to_synth (&s, 1e6);
        DP_REQUIRE_MSG (comp && bridge, "both faces build");
        dp_wfm_synth_steps (comp, a, n);
        dp_wfm_synth_steps (bridge, b, n);
        DP_REQUIRE_MSG (memcmp (a, b, n * sizeof *a) == 0,
                        "the composer's synth and the standalone bridge's "
                        "are byte-identical");
        /* and not vacuously: a noisy source must not be all zeros, and a
           signal source must not be silent */
        double p = 0.0;
        for (size_t i = 0; i < n; i++)
          p += (double)(crealf (a[i]) * crealf (a[i]));
        DP_REQUIRE_MSG (p > 0.0, "precondition: the face produced signal");
        dp_wfm_synth_destroy (comp);
        dp_wfm_synth_destroy (bridge);
      }
  /* ── a chirp: the span is declared, never read off the first block ────
   *
   * #1115. The bridge used to leave the span to dp_wfm_synth_steps()'s first
   * block, so the standalone face swept over whatever the caller read first.
   * A declared span must reach BOTH faces, and must beat the on-time. */
  {
    wfm_source_t s             = { 0 };
    s.type                     = WFM_SYNTH_CHIRP;
    s.freq                     = 1e5;
    s.f_end                    = 3e5;
    s.snr                      = 100.0;
    s.span                     = n / 4; /* deliberately not the on-time */
    dp_wfm_synth_state_t *comp = dp_wfm_compose_build_synth (
        &s, 1e6, n, s.freq, s.snr, s.f_end, 0, 0, 0);
    dp_wfm_synth_state_t *bridge = dp_wfm_source_to_synth (&s, 1e6);
    DP_REQUIRE_MSG (comp && bridge, "both faces build a declared chirp");
    DP_REQUIRE_MSG (comp->chirp_span == n / 4 && bridge->chirp_span == n / 4,
                    "the declared span beats the on-time on both faces");
    dp_wfm_synth_steps (comp, a, n);
    for (size_t off = 0; off < n; off += 16) /* chunked: must not matter */
      dp_wfm_synth_steps (bridge, b + off, 16);
    DP_REQUIRE_MSG (memcmp (a, b, n * sizeof *a) == 0,
                    "a declared chirp is byte-identical across faces and "
                    "read chunkings");
    dp_wfm_synth_destroy (comp);
    dp_wfm_synth_destroy (bridge);

    /* undeclared: the composer lends the on-time, the bridge has none */
    s.span = 0;
    comp   = dp_wfm_compose_build_synth (&s, 1e6, n, s.freq, s.snr, s.f_end, 0,
                                         0, 0);
    DP_REQUIRE_MSG (comp && comp->chirp_span == n,
                    "an undeclared span falls back to the on-time");
    dp_wfm_synth_destroy (comp);
    DP_REQUIRE_MSG (!dp_wfm_source_to_synth (&s, 1e6),
                    "a standalone sweep with no span is refused, not guessed");
    s.f_end = s.freq; /* flat: no slope to lose */
    bridge  = dp_wfm_source_to_synth (&s, 1e6);
    DP_REQUIRE_MSG (bridge != NULL, "a flat chirp needs no span");
    dp_wfm_synth_destroy (bridge);
  }
  /* ── and the case where the shared helper actually DOES work ─────────
   *
   * The loop above uses types for which dp_wfm_source_create_snr is a
   * pass-through, so it cannot see the two faces drift: sabotaging the
   * bridge to skip the helper entirely left it green. A dsss source at a
   * data-symbol Es/N0 is where the pre-referral happens, and it is the only
   * shape that can catch that. Found by sabotage, which is what it is for.
   */
  {
    static const uint8_t code[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };
    static const uint8_t pay[5]  = { 1, 0, 1, 1, 0 };
    wfm_source_t         s       = { 0 };
    s.type                       = WFM_SYNTH_DSSS;
    s.sps                        = 2;
    s.seed                       = 3;
    s.pn_length                  = 9;
    s.snr                        = 9.0;
    s.snr_mode                   = 3; /* esno: the pre-referral runs */
    s.acq_code.kind              = WFM_SEQ_LITERAL;
    s.acq_code.bits              = code;
    s.acq_code.len               = sizeof code;
    s.acq_reps                   = 3;
    s.data_code.kind             = WFM_SEQ_LITERAL;
    s.data_code.bits             = code;
    s.data_code.len              = sizeof code;
    s.payload.kind               = WFM_SEQ_LITERAL;
    s.payload.bits               = pay;
    s.payload.len                = sizeof pay;
    s.crc                        = 1;

    float _Complex *da = malloc (n * sizeof *da);
    float _Complex *db = malloc (n * sizeof *db);
    DP_REQUIRE_MSG (da && db, "dsss faces: alloc");
    dp_wfm_synth_state_t *comp = dp_wfm_compose_build_synth (
        &s, 1e6, n, s.freq, s.snr, s.f_end, 0, 0, 0);
    dp_wfm_synth_state_t *bridge = dp_wfm_source_to_synth (&s, 1e6);
    DP_REQUIRE_MSG (comp && bridge, "both faces build a dsss burst");
    dp_wfm_synth_steps (comp, da, n);
    dp_wfm_synth_steps (bridge, db, n);
    DP_REQUIRE_MSG (memcmp (da, db, n * sizeof *da) == 0,
                    "the two faces agree on a dsss source, where the "
                    "shared SNR referral actually runs");
    /* precondition: the noise is really there, or the comparison is of two
       clean waveforms and the referral could not have shown up either way */
    double p = 0.0;
    for (size_t i = 0; i < n; i++)
      p += (double)(crealf (da[i]) * crealf (da[i])
                    + cimagf (da[i]) * cimagf (da[i]));
    DP_REQUIRE_MSG (p / (double)n > 1.5,
                    "precondition: the source is genuinely noisy, so the "
                    "referral is in the answer");
    dp_wfm_synth_destroy (comp);
    dp_wfm_synth_destroy (bridge);
    free (da);
    free (db);
  }

  free (a);
  free (b);
  return 0;
}

/* The bits an unspread source puts on the WIRE, read back off its samples:
 * the source's own synth (dp_wfm_compose_build_synth -- the one
 * construction path), clean, BPSK, the sign of every sps-th sample. A test
 * asserting on these observes the whole source path, whatever frame the
 * bridge built, rather than a private step inside it. The modulation is
 * forced to BPSK because the claim is about the bits, and BPSK is where the
 * sign IS the bit (0 -> +1, 1 -> -1). Returns @p n, or 0 if the source did
 * not build. */
static size_t
wire_bits (const wfm_source_t *src, uint8_t *out, size_t n)
{
  wfm_source_t s            = *src;
  s.modulation              = 1; /* bpsk */
  const size_t          sps = (s.sps < 1) ? 1u : (size_t)s.sps;
  dp_wfm_synth_state_t *sy = dp_wfm_compose_build_synth (&s, 1.0, n * sps, 0.0,
                                                         100.0, 0.0, 0, 0, 0);
  if (!sy)
    return 0;
  float _Complex *x = malloc (n * sps * sizeof *x);
  if (!x)
    {
      dp_wfm_synth_destroy (sy);
      return 0;
    }
  dp_wfm_synth_steps (sy, x, n * sps);
  for (size_t i = 0; i < n; i++)
    out[i] = crealf (x[i * sps]) < 0.0f ? 1u : 0u;
  free (x);
  dp_wfm_synth_destroy (sy);
  return n;
}

/* A CODED frame, described by name -- the shapes a `--frame FILE` carries,
 * written the way a caller writes one. The covers are 131.0-B-6's rule,
 * generalised past the CADU: a marker, a preamble and a sync word are
 * FOUND, so only the inner code covers them; the payload, its CRC and the
 * outer code's parity are the data group the outer code, the randomiser and
 * the interleaver reach over; stages are listed in APPLICATION order, the
 * interleaver last of the data-group stages so it is what the channel sees.
 * The sequences are borrowed; the marker's bits are static. Returns 0. */
typedef struct
{
  int              asm_;      /* the 0x1ACFFC1D marker, first            */
  const wfm_seq_t *pre;       /* preamble, or NULL                        */
  size_t           reps;      /* its repetitions                          */
  const wfm_seq_t *sync;      /* sync word, or NULL                       */
  const wfm_seq_t *payload;   /* the payload                              */
  int              crc;       /* CRC-16 over the payload                  */
  unsigned         rs;        /* outer code depth; 0 = none               */
  unsigned         rand;      /* randomiser: 1 = 10.4.1, 2 = 10.4.2       */
  unsigned         ilv, unit; /* interleaver depth and unit bits          */
  int              conv;      /* inner code over every field              */
} coded_t;

static int
coded_frame (const coded_t *c, wfm_frame_desc_t *d)
{
  static uint8_t marker[CCSDS_TM_ASM_BITS];
  const char    *first = NULL;
  memset (d, 0, sizeof *d);
  if (c->asm_)
    {
      dp_ccsds_tm_asm_bits (marker);
      const wfm_seq_t m = { .kind = WFM_SEQ_LITERAL,
                            .bits = marker,
                            .len  = CCSDS_TM_ASM_BITS };
      if (dp_wfm_frame_add_field (d, "asm", &m, 0u) < 0)
        return -1;
      first = "asm";
    }
  if (c->pre && c->reps)
    {
      if (dp_wfm_frame_add_field (d, "preamble", c->pre, c->reps) < 0)
        return -1;
      first = first ? first : "preamble";
    }
  if (c->sync)
    {
      if (dp_wfm_frame_add_field (d, "sync", c->sync, 0u) < 0)
        return -1;
      first = first ? first : "sync";
    }
  if (dp_wfm_frame_add_field (d, "payload", c->payload, 0u) < 0)
    return -1;
  first = first ? first : "payload";
  if (c->crc && dp_wfm_frame_add_derived (d, "crc", WFM_FRAME_CRC_BITS) < 0)
    return -1;
  if (c->rs
      && dp_wfm_frame_add_derived (d, "rs_parity",
                                   (size_t)CCSDS_TM_RS_2E * c->rs * 8u)
             < 0)
    return -1;
  const char *last = c->rs ? "rs_parity" : (c->crc ? "crc" : "payload");
  int         st;
  if (c->crc
      && dp_wfm_frame_add_stage (d, WFM_STAGE_CRC16, "payload", "crc") < 0)
    return -1;
  if (c->rs)
    {
      if ((st = dp_wfm_frame_add_stage (d, WFM_STAGE_RS, "payload", last)) < 0)
        return -1;
      d->stage[st].depth = c->rs;
    }
  if (c->rand)
    {
      if ((st
           = dp_wfm_frame_add_stage (d, WFM_STAGE_RANDOMISE, "payload", last))
          < 0)
        return -1;
      d->stage[st].depth = c->rand;
    }
  if (c->ilv)
    {
      if ((st
           = dp_wfm_frame_add_stage (d, WFM_STAGE_INTERLEAVE, "payload", last))
          < 0)
        return -1;
      d->stage[st].depth     = c->ilv;
      d->stage[st].unit_bits = c->unit;
    }
  if (c->conv)
    {
      if ((st = dp_wfm_frame_add_stage (d, WFM_STAGE_CONV, first, last)) < 0)
        return -1;
      d->stage[st].emit_num = 2u; /* rate 1/2 */
      d->stage[st].emit_den = 1u;
    }
  return 0;
}

/* ── a source EATS a frame the caller built ──────────────────────────
 * The frame builder already did everything asked of it -- named fields,
 * a caller's own bits at a position they choose, stages covering spans
 * they name, kernels looked up by kind. What it could not do was reach
 * wfmgen: wfm_source_t restated 13 flat framing/coding fields, derived a
 * description from them, and offered no way to hand one in. So every new
 * coding type had to become another wfmgen flag rather than a stage.
 *
 * These pin the seam that fixes it, and the rule that came with it: a
 * carried description is the WHOLE frame, so a second spelling of part of
 * it beside it is refused rather than silently dropped. */
static int
test_a_source_carries_the_frame_a_caller_built (void)
{
  /* A field of the caller's own bits -- 0x5C5C, spelled bit by bit -- is
     the case the flat fields cannot express at all: they offer a preamble,
     a sync word and a payload, at fixed positions, and nothing else. */
  static uint8_t marker_bits[16];
  for (int i = 0; i < 16; i++)
    marker_bits[i] = (uint8_t)((0x5C5Cu >> (15 - i)) & 1u);
  static uint8_t payload_bits[24];
  for (int i = 0; i < 24; i++)
    payload_bits[i] = (uint8_t)(i & 1u);

  wfm_seq_t marker  = { 0 };
  marker.kind       = WFM_SEQ_LITERAL;
  marker.bits       = marker_bits;
  marker.len        = 16;
  wfm_seq_t payload = { 0 };
  payload.kind      = WFM_SEQ_LITERAL;
  payload.bits      = payload_bits;
  payload.len       = 24;

  wfm_frame_desc_t d;
  memset (&d, 0, sizeof d);
  DP_REQUIRE_MSG (
      dp_wfm_frame_add_field (&d, "mark", &marker, 0u) == 0,
      "frame: dp_wfm_frame_add_field (&d, 'mark', &marker, 0u) == 0");
  DP_REQUIRE_MSG (
      dp_wfm_frame_add_field (&d, "payload", &payload, 0u) == 1,
      "frame: dp_wfm_frame_add_field (&d, 'payload', &payload, 0u) == 1");
  DP_REQUIRE_MSG (
      dp_wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "payload", "payload") == 0,
      "frame: dp_wfm_frame_add_stage (&d, WFM_STAGE_CRC16, 'payload', 'payl");

  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_BITS;
  src.pn_length    = 7;
  src.modulation   = 1; /* bpsk */
  src.sps          = 1;
  src.payload      = payload;

  /* Without a carried frame, none of the flat fields is set, so this
     source is unframed -- the precondition that makes the next line mean
     something rather than restating a default. */
  DP_REQUIRE_MSG (!dp_wfm_source_has_frame (&src),
                  "frame: !dp_wfm_source_has_frame (&src)");

  src.frame = &d;
  DP_REQUIRE_MSG (
      dp_wfm_source_has_frame (&src),
      "frame: dp_wfm_source_has_frame (&src)"); /* carried == framed */

  /* And the frame on the wire is the one handed in: the source's own synth
     emits exactly what the description assembles to. */
  wfm_frame_desc_layout_t l;
  DP_REQUIRE (dp_wfm_frame_desc_layout (&d, &l) == 0);
  uint8_t want[64], got[64];
  DP_REQUIRE_MSG (l.out_bits <= sizeof want
                      && dp_wfm_frame_assemble (&d, NULL, want, sizeof want)
                             == l.out_bits,
                  "the description assembles");
  DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) == NULL,
                  "a carried frame is honoured as given");
  DP_REQUIRE_MSG (wire_bits (&src, got, l.out_bits) == l.out_bits,
                  "the source builds");
  DP_REQUIRE_MSG (memcmp (got, want, l.out_bits) == 0,
                  "frame: the wire carries the caller's description, bit for "
                  "bit");

  /* ONE frame, said one way: a sync word beside a carried description is a
     second spelling of part of it, and the description would silently win.
     Refused -- and for the same reason an unspread preamble is. */
  src.sync = marker;
  DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) != NULL,
                  "frame: a sync word beside a carried frame is refused");
  memset (&src.sync, 0, sizeof src.sync);
  src.acq_code = marker;
  src.acq_reps = 2;
  DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) != NULL,
                  "frame: so is an unspread preamble");

  /* Drop the description and the same fields describe the COMMON frame --
     the flat path is untouched, which is what keeps every existing scene
     working. */
  src.frame = NULL;
  DP_REQUIRE_MSG (dp_wfm_source_has_frame (&src),
                  "frame: a preamble alone frames the common frame");
  DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) == NULL,
                  "frame: and it builds");

  printf ("  a source carries the frame a caller built\n");
  return 0;
}

/* The scene JSON carries a description across, in BOTH directions.
 *
 * A carried frame that the spec cannot write is a frame Python and
 * `--from-file` cannot reach, and one the spec writes but cannot read back is
 * worse: `--record` then `--from-file` rebuilds the DERIVED frame, silently,
 * and the capture nobody can reproduce looks exactly like one that works.
 *
 * Two kinds of assertion, because they catch different failures and neither
 * subsumes the other.
 *
 * Field-by-field catches a value mangled in transit. The byte-identical
 * emit -> parse -> emit catches a key the READER has no line for: the value
 * regresses to its default, the second emit writes something different, and
 * the strings diverge. That is why one stage below carries a non-zero
 * `depth`/`unit_bits` that no field assert reads back -- with every
 * uncompared key sitting at 0 the re-emit could not fail and would be
 * decoration. Sabotage-checked: a parser that drops "depth" is caught HERE
 * and by nothing else in this test.
 *
 * What the re-emit does NOT catch, measured rather than assumed: a key the
 * WRITER alone adds. Both emissions write it, so the strings still match and
 * every assert here passes. `additionalProperties: false` in
 * docs/schema/wfmgen.schema.json is what refuses an undeclared key, checked
 * by test_schema.py against real --record output. */
static int
test_a_carried_frame_survives_the_scene_json (void)
{
  static const uint8_t marker_bits[8] = { 1, 0, 1, 0, 1, 1, 0, 0 };

  static const uint8_t payload_bits[16]
      = { 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1 };

  wfm_seq_t marker = { 0 };
  marker.kind      = WFM_SEQ_LITERAL;
  marker.bits      = marker_bits;
  marker.len       = 8;

  wfm_seq_t payload = { 0 };
  payload.kind      = WFM_SEQ_LITERAL;
  payload.bits      = payload_bits;
  payload.len       = 16;

  /* A GENERATED field, so the seq marshaling this reuses is exercised too:
     the parameters cross, not a million-symbol run. */
  wfm_seq_t code = { 0 };
  code.kind      = WFM_SEQ_PN;
  code.len       = 31;
  code.reg_bits  = 5;
  code.poly      = 0x12u;
  code.seed      = 1u;
  code.lfsr      = 0;

  wfm_frame_desc_t d;
  memset (&d, 0, sizeof d);
  DP_REQUIRE_MSG (dp_wfm_frame_add_field (&d, "mark", &marker, 0u) == 0,
                  "json-frame: add_field mark == 0");
  DP_REQUIRE_MSG (dp_wfm_frame_add_field (&d, "code", &code, 0u) == 1,
                  "json-frame: add_field code == 1");
  DP_REQUIRE_MSG (dp_wfm_frame_add_derived (&d, "check", 16u) == 2,
                  "json-frame: add_derived check == 2");
  DP_REQUIRE_MSG (dp_wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "code", "check")
                      == 0,
                  "json-frame: add_stage crc16 == 0");
  /* A second named kind, renderable, so this scene is one the composer
     accepts; the caller's-own-kind case is its own block after this one. */
  DP_REQUIRE_MSG (
      dp_wfm_frame_add_stage (&d, WFM_STAGE_RANDOMISE, "mark", "check") == 1,
      "json-frame: add_stage randomise == 1");
  /* A stage carrying NON-ZERO geometry, and deliberately geometry no assert
     below reads back field-by-field. That is what gives the byte-identical
     re-emit something to catch that nothing else here would: a parser that
     drops "depth" leaves it at its 0 default, the second emit then OMITS the
     key, and only the string comparison notices. With every uncompared key
     sitting at 0 the re-emit could not fail, and would be decoration. */
  DP_REQUIRE_MSG (
      dp_wfm_frame_add_stage (&d, WFM_STAGE_INTERLEAVE, "mark", "check") == 2,
      "json-frame: add_stage interleave == 2");
  /* depth x unit_bits must divide the 55 bits it covers (8 + 31 + 16), or
     the interleaver refuses and -- since the composer builds each source at
     create -- so does the scene. 5 x 11 keeps both keys non-zero. */
  d.stage[2].depth     = 5u;
  d.stage[2].unit_bits = 11u;

  /* BITS rather than BPSK: a bits source's payload is the literal it was
     given, and a payload that did not survive the round trip would make the
     reject below fire for the wrong reason. */
  wfm_source_t src = { .type      = WFM_SYNTH_BITS,
                       .freq      = 0.0,
                       .snr       = 40.0,
                       .snr_mode  = 0,
                       .seed      = 1,
                       .sps       = 4,
                       .pn_length = 7 };
  /* A framed source needs a payload -- dp_wfm_source_frame_error() refuses one
     without, and a CARRIED description is framing by the same predicate, so
     the rule reaches this source exactly as it reaches a flat-framed one. */
  src.payload = payload;
  src.frame   = &d;
  wfm_segment_t seg
      = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 256 };

  char *js1 = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (js1, "json-frame: to_json");
  DP_REQUIRE_MSG (strstr (js1, "\"frame\""),
                  "json-frame: the spec carries a \"frame\" key");
  /* A named kind reads as its name, a caller's own as its number. */
  DP_REQUIRE_MSG (strstr (js1, "\"crc16\""),
                  "json-frame: a named kind is written by NAME");
  /* (the caller's own kind: see the block after this one) */

  dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (js1);
  DP_REQUIRE_MSG (c, "json-frame: from_json");

  size_t               n  = 0;
  int                  rp = 0, ct = 0;
  const wfm_segment_t *gs = dp_wfm_compose_segments (c, &n, &rp, &ct);
  DP_REQUIRE_MSG (gs && n == 1 && gs[0].n_sources == 1,
                  "json-frame: one segment, one source came back");
  const wfm_frame_desc_t *g = gs[0].sources[0].frame;
  DP_REQUIRE_MSG (g, "json-frame: the parsed source CARRIES a description");

  DP_REQUIRE_MSG (g->n_fields == d.n_fields && g->n_stages == d.n_stages,
                  "json-frame: field and stage counts survive");
  DP_REQUIRE_MSG (strcmp (g->field[0].name, "mark") == 0
                      && strcmp (g->field[1].name, "code") == 0
                      && strcmp (g->field[2].name, "check") == 0,
                  "json-frame: field names survive");
  DP_REQUIRE_MSG (g->field[0].seq.kind == WFM_SEQ_LITERAL
                      && g->field[0].seq.len == 8
                      && memcmp (g->field[0].seq.bits, marker_bits, 8) == 0,
                  "json-frame: a literal field's BITS survive");
  DP_REQUIRE_MSG (
      g->field[1].seq.kind == WFM_SEQ_PN && g->field[1].seq.len == 31
          && g->field[1].seq.reg_bits == 5 && g->field[1].seq.poly == 0x12u
          && g->field[1].seq.seed == 1u,
      "json-frame: a generated field's PARAMETERS survive");
  DP_REQUIRE_MSG (g->field[2].bits == 16u
                      && g->field[2].derived_by == d.field[2].derived_by,
                  "json-frame: a derived field's length and producer survive");
  DP_REQUIRE_MSG (g->stage[0].kind == WFM_STAGE_CRC16
                      && g->stage[1].kind == WFM_STAGE_RANDOMISE,
                  "json-frame: every named kind survives");
  DP_REQUIRE_MSG (g->stage[0].first_field == d.stage[0].first_field
                      && g->stage[0].n_fields == d.stage[0].n_fields
                      && g->stage[1].first_field == d.stage[1].first_field
                      && g->stage[1].n_fields == d.stage[1].n_fields,
                  "json-frame: every stage's COVER survives");

  /* The one that catches a key with only one direction wired. */
  char *js2 = dp_wfm_spec_to_json (gs, n, rp, ct, 0, 0.0);
  DP_REQUIRE_MSG (js2, "json-frame: re-emit");
  DP_REQUIRE_MSG (strcmp (js1, js2) == 0,
                  "json-frame: emit -> parse -> emit is byte-identical");

  free (js2);
  dp_wfm_compose_destroy (c);
  free (js1);

  /* A kind doppler has NEVER heard of. The wire form carries it -- as an
     integer, the case a name-only encoding could not carry at all -- but a
     composer runs only the built-in and ccsds_tm kernels, so a scene naming
     it can never render. Since doppler#1590 that is refused at create rather
     than accepted and rendered as a silent gap: both halves are pinned. */
  {
    wfm_frame_desc_t du = d;
    du.stage[1].kind    = WFM_STAGE_USER + 1u;
    wfm_source_t su     = src;
    su.frame            = &du;
    wfm_segment_t sgu   = seg;
    sgu.sources         = &su;
    char *ju            = dp_wfm_spec_to_json (&sgu, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (
        ju && strstr (ju, "4097"),
        "json-frame: a caller's own kind is written as its integer");
    DP_REQUIRE_MSG (dp_wfm_compose_from_json (ju) == NULL,
                    "json-frame: a kind with no kernel is refused at create");
    free (ju);
  }

  /* Each reject below differs from THIS scene by exactly one thing: the
     frame defect under test. Asserting the control PARSES is what stops
     them passing for an unrelated reason -- a framed source with no payload
     is refused outright, so a frame reject written without one proves
     nothing about the frame. */
#define FRAME_SCENE(FR)                                                       \
  "{\"segments\":[{\"type\":\"bits\",\"fs\":1e6,\"num_samples\":16,"          \
  "\"payload\":\"1100101001110001\",\"frame\":" FR "}]}"

  dp_wfm_compose_state_t *ctl = dp_wfm_compose_from_json (
      FRAME_SCENE ("{\"fields\":[{\"name\":\"a\",\"spec\":\"1010\"}]}"));
  DP_REQUIRE_MSG (ctl, "json-frame: the control scene parses");
  dp_wfm_compose_destroy (ctl);

  /* A kind this build does not know is REFUSED, not defaulted -- kind 0 is
     crc16, so a reader that fell back would turn a typo into a CRC stage. */
  DP_REQUIRE_MSG (!dp_wfm_compose_from_json (FRAME_SCENE (
                      "{\"fields\":[{\"name\":\"a\",\"spec\":\"1010\"}],"
                      "\"stages\":[{\"kind\":\"crc32\",\"n_fields\":1}]}")),
                  "json-frame: an unknown stage NAME is refused");
  /* A field's bits are ONE Field, "spec"; the keys it replaced are
     refused by name rather than read beside it. */
  DP_REQUIRE_MSG (!dp_wfm_compose_from_json (FRAME_SCENE (
                      "{\"fields\":[{\"name\":\"a\",\"lit\":\"1010\"}]}")),
                  "json-frame: a field's retired \"lit\" key is refused");
#undef FRAME_SCENE

  printf ("  a carried frame survives the scene json\n");
  return 0;
}

/* ── a framed bpsk/qpsk/pn puts its FRAME on the wire (doppler#1616) ────
 *
 * gh-762 made the PN-sourced types frameable: `frame_modulation()` names
 * their mapping and `dp_wfm_source_frame_error()` passes them. But the
 * synth was created with the source's own type, and `set_bits` is a no-op
 * for anything but a BITS synth -- so the frame was assembled, handed over
 * and dropped, and the waveform was the LFSR stream. No test read one back
 * off the wire.
 *
 * The truth for bpsk is the issue's own reproduction, read off the CLI's
 * `--type bits --modulation bpsk` twin: Barker-13 | 10110011 | CRC-16. For
 * every type the twin is `bits` with the modulation the type names, sample
 * for sample, on BOTH construction faces -- clean, and noisy in the type's
 * own SNR reference, so the noise a framed bpsk gets is still Es/N0.
 */
static int
test_a_framed_pn_type_sends_its_frame (void)
{
  static const uint8_t barker[13] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };
  static const uint8_t pay[8]     = { 1, 0, 1, 1, 0, 0, 1, 1 };
  static const char    want[]     = "1111100110101101100110111011001001000";
  const size_t         nw         = sizeof want - 1u; /* 13 + 8 + 16 */

  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_BPSK;
  src.sps          = 1;
  src.seed         = 5;
  src.pn_length    = 7;
  src.snr          = 100.0;
  src.crc          = 1; /* crc16 */
  src.sync.kind    = WFM_SEQ_LITERAL;
  src.sync.bits    = barker;
  src.sync.len     = sizeof barker;
  src.payload.kind = WFM_SEQ_LITERAL;
  src.payload.bits = pay;
  src.payload.len  = sizeof pay;
  DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) == NULL,
                  "precondition: a framed bpsk is a shape the rule accepts");

  uint8_t got[64];
  DP_REQUIRE_MSG (wire_bits (&src, got, nw) == nw, "framed bpsk builds");
  int same = 1;
  for (size_t i = 0; i < nw; i++)
    same &= (got[i] == (uint8_t)(want[i] - '0'));
  DP_CHECK_MSG (same, "a framed --type bpsk sends Barker-13 | payload | "
                      "CRC-16 -- the frame, not its own PN stream (#1616)");

  const size_t    n = 4u * nw * 4u; /* four frames at sps 4 */
  float _Complex *a = malloc (n * sizeof *a);
  float _Complex *b = malloc (n * sizeof *b);
  DP_REQUIRE_MSG (a && b, "framed pn: alloc");
  const struct
  {
    int type, modulation;
  } T[]
      = { { WFM_SYNTH_BPSK, 1 }, { WFM_SYNTH_QPSK, 2 }, { WFM_SYNTH_PN, 1 } };
  for (size_t t = 0; t < sizeof T / sizeof T[0]; t++)
    for (int noisy = 0; noisy <= 2; noisy++)
      {
        /* clean; noisy in auto (Es/N0 for bpsk/qpsk, fs for pn -- a BITS
           synth's own auto is fs); noisy in Eb/No, where qpsk's two bits
           per symbol count and a BITS synth's one would not. */
        wfm_source_t s = src;
        s.type         = T[t].type;
        s.sps          = 4;
        s.snr          = noisy ? 9.0 : 100.0;
        s.snr_mode     = noisy == 2 ? 2 : 0;

        /* The twin: the same frame as a `bits` source with the mapping the
           type names, its noise stated over fs -- what the TYPE's own
           reference resolves to, through the shared conversion. */
        wfm_source_t tw = s;
        tw.type         = WFM_SYNTH_BITS;
        tw.modulation   = T[t].modulation;
        tw.snr_mode     = noisy ? 1 : 0;
        tw.snr = noisy ? dp_wfm_snr_over_fs (s.snr_mode, s.type, s.sps, 0, 0.0,
                                             s.snr)
                       : 100.0;

        dp_wfm_synth_state_t *twin = dp_wfm_compose_build_synth (
            &tw, 1e6, n, 0.0, tw.snr, 0.0, 0, 0, 0);
        dp_wfm_synth_state_t *comp = dp_wfm_compose_build_synth (
            &s, 1e6, n, 0.0, s.snr, 0.0, 0, 0, 0);
        dp_wfm_synth_state_t *bridge = dp_wfm_source_to_synth (&s, 1e6);
        DP_REQUIRE_MSG (twin && comp && bridge, "framed pn: all build");

        dp_wfm_synth_steps (twin, a, n);
        dp_wfm_synth_steps (comp, b, n);
        DP_CHECK_MSG (memcmp (a, b, n * sizeof *a) == 0,
                      "composer: a framed bpsk/qpsk/pn is its `bits` twin, "
                      "sample for sample, noise included");
        dp_wfm_synth_steps (bridge, b, n);
        DP_CHECK_MSG (memcmp (a, b, n * sizeof *a) == 0,
                      "standalone: and so is the bridge's synth");
        dp_wfm_synth_destroy (twin);
        dp_wfm_synth_destroy (comp);
        dp_wfm_synth_destroy (bridge);
      }

  /* An UNFRAMED bpsk keeps its PN stream: the switch is the frame's, not
     the type's. A bridge that made every bpsk a BITS synth would pass
     everything above and silence every unframed PN source. */
  {
    wfm_source_t u = src;
    memset (&u.sync, 0, sizeof u.sync);
    DP_REQUIRE (!dp_wfm_source_has_frame (&u));
    wfm_source_t plain = u;
    memset (&plain.payload, 0, sizeof plain.payload);
    DP_REQUIRE (wire_bits (&u, got, 32) == 32);
    uint8_t ref[32];
    DP_REQUIRE (wire_bits (&plain, ref, 32) == 32);
    DP_CHECK_MSG (memcmp (got, ref, 32) == 0,
                  "an unframed bpsk still emits its own PN stream");
  }
  free (a);
  free (b);
  printf ("  a framed bpsk/qpsk/pn sends its frame\n");
  return 0;
}

/* dp_wfm_frame_from_json / dp_wfm_frame_free, called directly: the entry
 * point `wfmgen --frame FILE` uses, which the scene tests above reach only
 * through the reader they share. Each check is a header claim (D13). */
static int
test_frame_from_json_directly (void)
{
  /* Every stage kind by name, each key it may carry, and a derived field. */
  static const char full[]
      = "{\"fields\":[{\"name\":\"asm\",\"spec\":\"0x1ACFFC1D\"},"
        "{\"name\":\"data\",\"spec\":\"pn:31:5*4\"},"
        "{\"name\":\"crc\",\"bits\":16,\"derived_by\":1}],"
        "\"stages\":[{\"kind\":\"crc16\",\"first_field\":1,\"n_fields\":2},"
        "{\"kind\":\"rs\",\"first_field\":1,\"n_fields\":1,\"depth\":1},"
        "{\"kind\":\"randomise\",\"first_field\":1,\"n_fields\":2},"
        "{\"kind\":\"interleave\",\"first_field\":1,\"n_fields\":1,"
        "\"depth\":4,\"unit_bits\":31},"
        "{\"kind\":\"conv\",\"first_field\":0,\"n_fields\":3,"
        "\"emit_num\":2,\"emit_den\":1}]}";
  const char       *why = "unset";
  wfm_frame_desc_t *d   = dp_wfm_frame_from_json (full, &why);
  DP_REQUIRE_MSG (d != NULL, "frame_from_json: a full description reads");
  DP_CHECK_MSG (why == NULL, "frame_from_json: success leaves why NULL");
  DP_CHECK_MSG (d->n_fields == 3 && d->n_stages == 5,
                "frame_from_json: three fields, five stages");
  DP_CHECK_MSG (d->field[0].seq.kind == WFM_SEQ_LITERAL
                    && d->field[0].seq.len == 32 && d->field[0].seq.bits,
                "frame_from_json: a literal spec owns its bits");
  DP_CHECK_MSG (d->field[1].seq.kind == WFM_SEQ_PN && d->field[1].reps == 4,
                "frame_from_json: a generated spec keeps its *REPS");
  DP_CHECK_MSG (d->field[2].bits == 16 && d->field[2].derived_by == 1,
                "frame_from_json: a derived field is bits + derived_by");
  DP_CHECK_MSG (d->stage[0].kind == WFM_STAGE_CRC16
                    && d->stage[1].kind == WFM_STAGE_RS
                    && d->stage[2].kind == WFM_STAGE_RANDOMISE
                    && d->stage[3].kind == WFM_STAGE_INTERLEAVE
                    && d->stage[4].kind == WFM_STAGE_CONV,
                "frame_from_json: all five kinds read by name");
  DP_CHECK_MSG (d->stage[3].depth == 4 && d->stage[3].unit_bits == 31,
                "frame_from_json: depth and unit_bits");
  DP_CHECK_MSG (d->stage[4].emit_num == 2 && d->stage[4].emit_den == 1,
                "frame_from_json: emit_num and emit_den");
  dp_wfm_frame_free (d); /* bits and all: LSan holds it under test-asan */

  /* Reading is not laying out: a derived field that is not the last of its
     producer's cover is an invariant of the LAYOUT (wfm_frame.h), so this
     description reads, and does not lay out. */
  d = dp_wfm_frame_from_json (
      "{\"fields\":[{\"name\":\"crc\",\"bits\":16,\"derived_by\":1},"
      "{\"name\":\"data\",\"spec\":\"pn:31:5\"}],"
      "\"stages\":[{\"kind\":\"crc16\",\"first_field\":0,"
      "\"n_fields\":2}]}",
      NULL);
  wfm_frame_desc_layout_t lay;
  DP_CHECK_MSG (d != NULL && dp_wfm_frame_desc_layout (d, &lay) != 0,
                "frame_from_json: a description that will not lay out "
                "still reads");
  dp_wfm_frame_free (d); /* bits and all: LSan holds it under test-asan */

  /* A caller's own kind, by number. */
  d = dp_wfm_frame_from_json ("{\"fields\":[{\"spec\":\"1010\"}],"
                              "\"stages\":[{\"kind\":4097,\"n_fields\":1}]}",
                              NULL);
  DP_CHECK_MSG (d && d->stage[0].kind == WFM_STAGE_USER + 1u,
                "frame_from_json: a numeric kind reads as that kind");
  dp_wfm_frame_free (d);

  /* Not a frame object: NULL, each with a static reason. */
  static const char *const not_frame[]
      = { "[]", "garbage", "{\"fields\":{}}",
          "{\"fields\":[{\"spec\":\"2\"}]}", NULL };
  for (size_t i = 0; i < sizeof not_frame / sizeof *not_frame; i++)
    {
      const char *w1 = NULL, *w2 = NULL;
      DP_CHECK_MSG (dp_wfm_frame_from_json (not_frame[i], &w1) == NULL,
                    not_frame[i] ? not_frame[i] : "(NULL text)");
      (void)dp_wfm_frame_from_json (not_frame[i], &w2);
      DP_CHECK_MSG (w1 != NULL && w1 == w2,
                    not_frame[i] ? not_frame[i] : "(NULL text) has a reason");
      DP_CHECK_MSG (dp_wfm_frame_from_json (not_frame[i], NULL) == NULL,
                    "frame_from_json: why may be NULL");
    }
  dp_wfm_frame_free (NULL); /* NULL is a no-op */

  /* The header's @code example, as written. */
  {
    const char       *why;
    wfm_frame_desc_t *ex = dp_wfm_frame_from_json (
        "{\"fields\": [{\"name\": \"sync\", \"spec\": \"0x1ACFFC1D\"},"
        "              {\"name\": \"data\", \"spec\": \"pn:31:5*4\"}]}",
        &why);
    DP_CHECK_MSG (ex && ex->n_fields == 2 && ex->field[1].reps == 4,
                  "frame_from_json @code: two fields, the second *4");
    dp_wfm_frame_free (ex);
    dp_wfm_frame_free (dp_wfm_frame_from_json ("{\"fields\": []}", NULL));
  }
  printf ("  dp_wfm_frame_from_json reads, refuses and frees\n");
  return 0;
}

int
main (void)
{
  /* tone @100kHz (1000 on, 500 off), then qpsk (4096 on, 0 off). */
  wfm_source_t  src0    = { .type      = 0,
                            .freq      = 1e5,
                            .snr       = 100.0,
                            .snr_mode  = 0,
                            .seed      = 1,
                            .sps       = 8,
                            .pn_length = 7,
                            .pn_poly   = 0 };
  wfm_source_t  src1    = { .type      = 4,
                            .freq      = 0,
                            .snr       = 100.0,
                            .snr_mode  = 0,
                            .seed      = 5,
                            .sps       = 8,
                            .pn_length = 7,
                            .pn_poly   = 0 };
  wfm_segment_t segs[2] = {
    { .sources     = &src0,
      .n_sources   = 1,
      .fs          = 1e6,
      .num_samples = 1000,
      .off_samples = 500 },
    { .sources     = &src1,
      .n_sources   = 1,
      .fs          = 1e6,
      .num_samples = 4096,
      .off_samples = 0 },
  };

  /* ── once-through: collect the whole stream in odd-sized chunks ── */
  dp_wfm_compose_state_t *c = dp_wfm_compose_create (segs, 2, 0, 0);
  DP_REQUIRE_MSG (c, "create");
  static float _Complex all[8192];
  size_t total = 0, n;
  float _Complex buf[777];
  while ((n = dp_wfm_compose_execute (c, buf, 777)) > 0)
    {
      DP_REQUIRE_MSG (total + n <= 8192, "overflow");
      for (size_t i = 0; i < n; i++)
        all[total + i] = buf[i];
      total += n;
    }
  DP_REQUIRE_MSG (total == 1000 + 500 + 4096, "total sample count");

  /* tone region non-zero; off region exactly zero; qpsk region non-zero */
  for (size_t i = 0; i < 1000; i++)
    DP_REQUIRE_MSG (all[i] != 0.0f, "tone region should be non-zero");
  for (size_t i = 1000; i < 1500; i++)
    DP_REQUIRE_MSG (all[i] == 0.0f, "off-time gap should be zero");
  for (size_t i = 1500; i < 1500 + 4096; i++)
    DP_REQUIRE_MSG (all[i] != 0.0f, "qpsk region should be non-zero");

  /* tone sits at +0.1 cyc/sample (100kHz / 1MHz): correlation ≈ 1 */
  double re = 0, im = 0;
  for (int k = 0; k < 1000; k++)
    {
      double ph = -2.0 * M_PI * 0.1 * k;
      re += creal (all[k]) * cos (ph) - cimag (all[k]) * sin (ph);
      im += creal (all[k]) * sin (ph) + cimag (all[k]) * cos (ph);
    }
  DP_REQUIRE_MSG (sqrt (re * re + im * im) / 1000.0 > 0.95,
                  "tone freq/correlation");
  dp_wfm_compose_destroy (c);

  /* ── repeat: the sequence loops, execute never returns short ── */
  dp_wfm_compose_state_t *r = dp_wfm_compose_create (segs, 2, 1, 0);
  DP_REQUIRE_MSG (r, "create repeat");
  for (int it = 0; it < 8; it++)
    DP_REQUIRE_MSG (dp_wfm_compose_execute (r, all, 8192) == 8192,
                    "repeat loops full");
  dp_wfm_compose_destroy (r);

  /* ── JSON round-trip: spec → JSON → spec produces identical output ── */
  char *json = dp_wfm_spec_to_json (segs, 2, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (json, "to_json");
  DP_REQUIRE_MSG (strstr (json, "\"tone\"") && strstr (json, "\"qpsk\""),
                  "type names");
  DP_REQUIRE_MSG (strstr (json, "\"version\""), "version tag");
  dp_wfm_compose_state_t *jc = dp_wfm_compose_from_json (json);
  DP_REQUIRE_MSG (jc, "from_json");
  static float _Complex jall[8192];
  size_t jtotal = 0;
  while ((n = dp_wfm_compose_execute (jc, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        jall[jtotal + i] = buf[i];
      jtotal += n;
    }
  DP_REQUIRE_MSG (jtotal == total, "round-trip sample count");
  /* re-collect the direct stream for a byte comparison */
  dp_wfm_compose_state_t *d      = dp_wfm_compose_create (segs, 2, 0, 0);
  size_t                  dtotal = 0;
  while ((n = dp_wfm_compose_execute (d, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        all[dtotal + i] = buf[i];
      dtotal += n;
    }
  for (size_t i = 0; i < total; i++)
    DP_REQUIRE_MSG (jall[i] == all[i],
                    "JSON round-trip must be sample-identical");
  dp_wfm_compose_destroy (jc);
  dp_wfm_compose_destroy (d);

  /* bad spec → NULL (unknown type, empty segments) */
  DP_REQUIRE_MSG (
      !dp_wfm_compose_from_json ("{\"segments\":[{\"type\":\"bogus\"}]}"),
      "unknown type rejected");
  DP_REQUIRE_MSG (!dp_wfm_compose_from_json ("{\"segments\":[]}"),
                  "empty rejected");

  free (json);

  /* ── json-template: the dumped example must parse and compose ── */
  {
    char *tpl = dp_wfm_spec_template_json ();
    DP_REQUIRE_MSG (tpl, "template built");
    DP_REQUIRE_MSG (strstr (tpl, "\"version\""), "template version tag");
    DP_REQUIRE_MSG (strstr (tpl, "\"sum\""),
                    "template shows a multi-source segment");
    dp_wfm_compose_state_t *tc = dp_wfm_compose_from_json (tpl);
    DP_REQUIRE_MSG (tc, "template round-trips through from_json");
    size_t tt = 0;
    while ((n = dp_wfm_compose_execute (tc, buf, 777)) > 0)
      tt += n;
    /* 10000 tone + (8000 on + 2000 off gap) bits + 10000 mix */
    DP_REQUIRE_MSG (tt == 30000, "template sample count");
    dp_wfm_compose_destroy (tc);
    free (tpl);
  }

  /* ── symbols JSON round-trip (gh #331): a type="symbols" source carries an
   *    explicit complex constellation array that must survive to_json →
   *    from_json. Both the 1-source inline serializer and the multi-source
   *    "sum" serializer are exercised; each must compose sample-identically.
   * ──
   */
  {
    float _Complex c0[8] = { 1 + 1 * I, -1 + 1 * I, 1 - 1 * I,  -1 - 1 * I,
                             1 + 1 * I, 1 - 1 * I,  -1 + 1 * I, -1 - 1 * I };
    float _Complex c1[8] = { 1 - 1 * I,  1 + 1 * I, -1 - 1 * I, -1 + 1 * I,
                             -1 - 1 * I, 1 + 1 * I, 1 - 1 * I,  -1 + 1 * I };
    wfm_source_t a0      = { .type      = WFM_SYNTH_SYMBOLS,
                             .snr       = 100.0,
                             .seed      = 1,
                             .sps       = 4,
                             .symbols   = c0,
                             .n_symbols = 8 };
    wfm_source_t a1      = { .type      = WFM_SYNTH_SYMBOLS,
                             .snr       = 100.0,
                             .seed      = 2,
                             .sps       = 4,
                             .level     = -3.0,
                             .symbols   = c1,
                             .n_symbols = 8 };
    /* both the inline (1-source) and "sum" (2-source) serializer paths */
    wfm_source_t  one[1]   = { a0 };
    wfm_source_t  both[2]  = { a0, a1 };
    wfm_segment_t segs2[2] = {
      { .sources = one, .n_sources = 1, .fs = 1e6, .num_samples = 256 },
      { .sources = both, .n_sources = 2, .fs = 1e6, .num_samples = 256 }
    };
    char *js = dp_wfm_spec_to_json (segs2, 2, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (js, "symbols to_json");
    DP_REQUIRE_MSG (strstr (js, "\"symbols\""),
                    "symbols type + array serialized");
    dp_wfm_compose_state_t *jc = dp_wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jc, "symbols from_json");
    dp_wfm_compose_state_t *dc = dp_wfm_compose_create (segs2, 2, 0, 0);
    DP_REQUIRE_MSG (jc && dc, "symbols states built");
    float _Complex ja[600], da[600];
    size_t jt = 0, dt = 0;
    while ((n = dp_wfm_compose_execute (jc, buf, 333)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          ja[jt + i] = buf[i];
        jt += n;
      }
    while ((n = dp_wfm_compose_execute (dc, buf, 333)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          da[dt + i] = buf[i];
        dt += n;
      }
    DP_REQUIRE_MSG (jt == dt && jt == 512, "symbols round-trip sample count");
    int ok = 1;
    for (size_t i = 0; i < jt; i++)
      if (ja[i] != da[i])
        ok = 0;
    DP_REQUIRE_MSG (
        ok, "symbols JSON round-trip must be sample-identical (gh #331)");
    dp_wfm_compose_destroy (jc);
    dp_wfm_compose_destroy (dc);
    free (js);
  }

  /* ── level: a segment at -6.0206 dBFS is the level-0 stream × 0.5 ── */
  {
    wfm_source_t src0
        = { .type = 0, .snr = 100.0, .seed = 1, .sps = 8, .pn_length = 7 };
    wfm_source_t src6 = src0;
    src6.level        = -6.020599913; /* gain 0.5 */
    wfm_segment_t s0
        = { .sources = &src0, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
    wfm_segment_t s6
        = { .sources = &src6, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
    float _Complex a[64], b[64];
    dp_wfm_compose_state_t *ca = dp_wfm_compose_create (&s0, 1, 0, 0);
    dp_wfm_compose_state_t *cb = dp_wfm_compose_create (&s6, 1, 0, 0);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (ca, a, 64) == 64, "level a");
    DP_REQUIRE_MSG (dp_wfm_compose_execute (cb, b, 64) == 64, "level b");
    int ok = 1;
    for (int i = 0; i < 64; i++)
      if (cabsf (b[i] - a[i] * 0.5f) > 1e-6f)
        ok = 0;
    DP_REQUIRE_MSG (ok, "level -6 dBFS == 0.5 * level-0 stream");
    dp_wfm_compose_destroy (ca);
    dp_wfm_compose_destroy (cb);
  }

  /* ── 1 source ≡ bundled: a noisy single source == direct dp_wfm_synth_steps
   * ──
   */
  {
    wfm_source_t  src = { .type      = 4, /* qpsk */
                          .snr       = 9.0,
                          .snr_mode  = 3,
                          .seed      = 7,
                          .sps       = 4,
                          .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 200 };
    float _Complex viac[200], direct[200];
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c, viac, 200) == 200,
                    "1src execute");
    dp_wfm_compose_destroy (c);
    dp_wfm_synth_state_t *s
        = dp_wfm_synth_create (4, 1e6, 0.0, 9.0, 3, 7, 4, 7, 0, 0, 0.0);
    dp_wfm_synth_steps (s, direct, 200);
    dp_wfm_synth_destroy (s);
    int ok = 1;
    for (int i = 0; i < 200; i++)
      if (viac[i]
          != direct[i]) /* same dp_wfm_synth_steps call → bit-identical */
        ok = 0;
    DP_REQUIRE_MSG (
        ok, "1-source segment == bundled dp_wfm_synth_steps (bit-exact)");
  }

  /* ── 2-source accumulate: segment sum == g0*synth0 + g1*synth1 ── */
  {
    wfm_source_t srcs[2] = {
      { .type = 0, .freq = 0.0, .snr = 100.0, .seed = 1 }, /* tone */
      { .type  = 0,
        .freq  = 2e5,
        .snr   = 100.0,
        .seed  = 2,
        .level = -6.020599913 }, /* tone -6 dB */
    };
    wfm_segment_t seg
        = { .sources = srcs, .n_sources = 2, .fs = 1e6, .num_samples = 100 };
    float _Complex sum[100];
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c, sum, 100) == 100,
                    "2src execute");
    dp_wfm_compose_destroy (c);
    /* reference: render each source and add with the same gains + order. */
    dp_wfm_synth_state_t *sa
        = dp_wfm_synth_create (0, 1e6, 0.0, 100.0, 0, 1, 1, 7, 0, 0, 0.0);
    dp_wfm_synth_state_t *sb
        = dp_wfm_synth_create (0, 1e6, 2e5, 100.0, 0, 2, 1, 7, 0, 0, 0.0);
    float _Complex ba[100], bb[100];
    dp_wfm_synth_steps (sa, ba, 100);
    dp_wfm_synth_steps (sb, bb, 100);
    dp_wfm_synth_destroy (sa);
    dp_wfm_synth_destroy (sb);
    float gb = (float)pow (10.0, -6.020599913 / 20.0);
    int   ok = 1;
    for (int i = 0; i < 100; i++)
      {
        float _Complex ref = ba[i] + gb * bb[i];
        if (cabsf (sum[i] - ref) > 1e-5f)
          ok = 0;
      }
    DP_REQUIRE_MSG (ok, "2-source sum == s0 + 0.5*s1");
  }

  /* ── noise resolve: snr on a source → a WFM_SYNTH_NOISE source at the floor
   * ──
   */
  {
    wfm_source_t srcs[2] = {
      { .type = 0, .snr = 10.0, .snr_mode = 1, .level = 0.0 }, /* anchor, fs */
      { .type  = 0,
        .freq  = 2e5,
        .snr   = 100.0,
        .level = -20.0 }, /* interferer */
    };
    wfm_segment_t seg
        = { .sources = srcs, .n_sources = 2, .fs = 1e6, .num_samples = 16 };
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (c, "resolve create");
    size_t               nseg;
    const wfm_segment_t *rs = dp_wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (rs[0].n_sources == 3, "noise source appended (2 → 3)");
    DP_REQUIRE_MSG (rs[0].sources[0].snr >= WFM_SYNTH_SNR_CLEAN,
                    "anchor cleaned");
    DP_REQUIRE_MSG (rs[0].sources[2].type == WFM_SYNTH_NOISE,
                    "appended is WFM_SYNTH_NOISE");
    DP_REQUIRE_MSG (fabs (rs[0].sources[2].level - (-10.0)) < 1e-9,
                    "floor = level - snr_fs = -10 dBFS");
    dp_wfm_compose_destroy (c);
  }

  /* ── resolve is idempotent: a clean+explicit-noise spec is a fixed point ──
   */
  {
    wfm_source_t resolved[3] = {
      { .type = 0, .snr = 100.0, .level = 0.0 },
      { .type = 0, .freq = 2e5, .snr = 100.0, .level = -20.0 },
      { .type = WFM_SYNTH_NOISE, .level = -10.0 }, /* explicit floor */
    };
    wfm_segment_t seg = {
      .sources = resolved, .n_sources = 3, .fs = 1e6, .num_samples = 16
    };
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (c, "idempotent create");
    size_t               nseg;
    const wfm_segment_t *rs = dp_wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (rs[0].n_sources == 3,
                    "idempotent: no second noise source");
    DP_REQUIRE_MSG (fabs (rs[0].sources[2].level - (-10.0)) < 1e-9,
                    "floor preserved");
    dp_wfm_compose_destroy (c);
  }

  /* ── reject: a non-anchor source over-specifying snr AND level ── */
  {
    wfm_source_t bad[2] = {
      { .type = 0, .snr = 10.0, .level = 0.0 }, /* anchor */
      { .type = 0, .snr = 5.0, .level = -3.0 }, /* non-anchor: snr + level */
    };
    wfm_segment_t seg
        = { .sources = bad, .n_sources = 2, .fs = 1e6, .num_samples = 16 };
    DP_REQUIRE_MSG (!dp_wfm_compose_create (&seg, 1, 0, 0),
                    "reject non-anchor snr + level");
  }

  /* ── JSON "sum" round-trip: a multi-source segment serialises + reparses ──
   */
  {
    wfm_source_t srcs[2] = {
      { .type      = 4, /* qpsk */
        .snr       = 12.0,
        .snr_mode  = 3,
        .seed      = 3,
        .sps       = 4,
        .pn_length = 7 },
      { .type = 0, .freq = 1.5e5, .snr = 100.0, .level = -10.0 }, /* tone */
    };
    wfm_segment_t seg
        = { .sources = srcs, .n_sources = 2, .fs = 1e6, .num_samples = 4096 };
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (c, "sum-json create");
    size_t               ns;
    int                  rp, ct;
    const wfm_segment_t *rs   = dp_wfm_compose_segments (c, &ns, &rp, &ct);
    char                *json = dp_wfm_spec_to_json (rs, ns, rp, ct, 0, 0.0);
    DP_REQUIRE_MSG (json && strstr (json, "\"sum\""), "sum array emitted");
    /* reparse and compare sample-for-sample. */
    dp_wfm_compose_state_t *jc = dp_wfm_compose_from_json (json);
    DP_REQUIRE_MSG (jc, "sum from_json");
    float _Complex a[4096], b[4096];
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c, a, 4096) == 4096, "sum direct");
    DP_REQUIRE_MSG (dp_wfm_compose_execute (jc, b, 4096) == 4096,
                    "sum reparsed");
    int ok = 1;
    for (int i = 0; i < 4096; i++)
      if (a[i] != b[i])
        ok = 0;
    DP_REQUIRE_MSG (ok, "JSON sum round-trip sample-identical");
    free (json);
    dp_wfm_compose_destroy (c);
    dp_wfm_compose_destroy (jc);
  }

  /* ── headroom rides in the record: emitted when set, extracted back ── */
  {
    wfm_source_t  src = { .type = 0, .snr = 100.0, .sps = 8, .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 16 };
    char *j6 = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 6.0);
    DP_REQUIRE_MSG (j6 && strstr (j6, "\"headroom\""),
                    "headroom emitted when set");
    DP_REQUIRE_MSG (fabs (dp_wfm_spec_headroom (j6) - 6.0) < 1e-9,
                    "headroom extracted");
    free (j6);
    char *j0 = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"headroom\""),
                    "headroom omitted at 0 dB");
    DP_REQUIRE_MSG (dp_wfm_spec_headroom (j0) == 0.0, "absent headroom → 0");
    free (j0);
  }

  /* ── seed_advance rides in the record too (doppler#978) ──
   *
   * Its twin above is not decoration: the key was PARSED and never emitted,
   * so a recorded run replayed with the mode reset to NONE and every loop
   * after the first came out identical. The composer is the SSOT here --
   * `--from-file` sets the mode from the spec and the flag path sets it from
   * `--seed-advance`, so a serialiser reads it back with
   * dp_wfm_compose_seed_advance() rather than from whichever half supplied it.
   */
  {
    wfm_source_t  src = { .type = 0, .snr = 100.0, .sps = 8, .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 16 };
    dp_wfm_compose_state_t *c
        = dp_wfm_compose_create (&seg, 1, /*repeat=*/1, 0);
    DP_REQUIRE_MSG (c, "seed_advance create");
    DP_REQUIRE_MSG (dp_wfm_compose_seed_advance (c) == WFM_SEED_ADVANCE_NONE,
                    "seed_advance defaults to NONE");
    dp_wfm_compose_set_seed_advance (c, WFM_SEED_ADVANCE_NOISE);
    DP_REQUIRE_MSG (dp_wfm_compose_seed_advance (c) == WFM_SEED_ADVANCE_NOISE,
                    "the getter reads back what the setter wrote");
    /* Out of range is ignored by the setter — so the getter must still show
     * the last good value, not whatever was passed. */
    dp_wfm_compose_set_seed_advance (c, 99);
    DP_REQUIRE_MSG (dp_wfm_compose_seed_advance (c) == WFM_SEED_ADVANCE_NOISE,
                    "an out-of-range mode leaves the getter untouched");

    size_t               ns;
    int                  rp, ct;
    const wfm_segment_t *rs = dp_wfm_compose_segments (c, &ns, &rp, &ct);
    char *jn = dp_wfm_spec_to_json (rs, ns, rp, ct,
                                    dp_wfm_compose_seed_advance (c), 0.0);
    /* Presence only — the VALUE is checked by the round trip below, so this
     * assertion must not encode cJSON's whitespace. */
    DP_REQUIRE_MSG (jn && strstr (jn, "\"seed_advance\""),
                    "seed_advance emitted when set");
    /* The round trip that matters: parse it back and the mode survives. */
    dp_wfm_compose_state_t *jc = dp_wfm_compose_from_json (jn);
    DP_REQUIRE_MSG (jc, "seed_advance from_json");
    DP_REQUIRE_MSG (dp_wfm_compose_seed_advance (jc) == WFM_SEED_ADVANCE_NOISE,
                    "seed_advance survives emit → parse");
    free (jn);
    dp_wfm_compose_destroy (jc);
    dp_wfm_compose_destroy (c);

    /* Omitted at the default, exactly as headroom is at 0 dB: the inline
     * form's field order is frozen for byte-identity, so an always-present
     * key would churn every capture ever recorded to say "none". */
    char *j0 = dp_wfm_spec_to_json (&seg, 1, 0, 0, WFM_SEED_ADVANCE_NONE, 0.0);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"seed_advance\""),
                    "seed_advance omitted at NONE");
    dp_wfm_compose_state_t *j0c = dp_wfm_compose_from_json (j0);
    DP_REQUIRE_MSG (
        j0c && dp_wfm_compose_seed_advance (j0c) == WFM_SEED_ADVANCE_NONE,
        "absent seed_advance → NONE");
    free (j0);
    dp_wfm_compose_destroy (j0c);
  }

  /* ── seed_advance: none = byte-identical repeat; all = whole seed advances
   *    (PN code changes) with the first pass unchanged ── */
  {
    wfm_source_t  pn = { .type      = 2, /* pn, no noise (snr 100) */
                         .snr       = 100.0,
                         .snr_mode  = 0,
                         .seed      = 1,
                         .sps       = 1,
                         .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &pn, .n_sources = 1, .fs = 1e6, .num_samples = 127 };
    float _Complex none[254], all[254];

    dp_wfm_compose_state_t *cn = dp_wfm_compose_create (&seg, 1, 1, 0);
    dp_wfm_compose_set_seed_advance (cn, WFM_SEED_ADVANCE_NONE);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (cn, none, 254) == 254,
                    "seedadv none exec");

    dp_wfm_compose_state_t *ca = dp_wfm_compose_create (&seg, 1, 1, 0);
    dp_wfm_compose_set_seed_advance (ca, WFM_SEED_ADVANCE_ALL);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (ca, all, 254) == 254,
                    "seedadv all exec");

    int none_same = 1, all_diff = 0, first_same = 1;
    for (int i = 0; i < 127; i++)
      {
        if (none[i] != none[127 + i])
          none_same = 0; /* none: repeat byte-identical */
        if (all[i] != all[127 + i])
          all_diff = 1; /* all: code changes on repeat */
        if (none[i] != all[i])
          first_same = 0; /* first pass is the unmodified seed */
      }
    DP_REQUIRE_MSG (none_same, "seed_advance none → byte-identical repeat");
    DP_REQUIRE_MSG (all_diff,
                    "seed_advance all → signal/code changes on repeat");
    DP_REQUIRE_MSG (first_same, "seed_advance all → first pass unchanged");
    dp_wfm_compose_destroy (cn);
    dp_wfm_compose_destroy (ca);
  }

  /* ── seed_advance noise: a noisy source's repeat is a fresh realization ──
   */
  {
    wfm_source_t  noisy = { .type     = 0, /* tone @ DC + AWGN */
                            .freq     = 0,
                            .snr      = 3.0,
                            .snr_mode = 1,
                            .seed     = 1,
                            .sps      = 1 };
    wfm_segment_t seg
        = { .sources = &noisy, .n_sources = 1, .fs = 1e6, .num_samples = 128 };
    float _Complex z[256];
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 1, 0);
    dp_wfm_compose_set_seed_advance (c, WFM_SEED_ADVANCE_NOISE);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c, z, 256) == 256,
                    "seedadv noise exec");
    int diff = 0;
    for (int i = 0; i < 128; i++)
      if (z[i] != z[128 + i])
        diff = 1;
    DP_REQUIRE_MSG (diff, "seed_advance noise → fresh noise on repeat");
    dp_wfm_compose_destroy (c);
  }

  /* ── ranged field: freq drawn uniformly per repeat, reproducibly ── */
  {
    /* A near-noiseless tone whose freq is a [lo, hi] draw. The noise stream is
     * deterministic per epoch, so any epoch-to-epoch sample difference is the
     * freq draw alone; a second composer must reproduce it bit-for-bit (the
     * draw hashes seed+epoch, it carries no RNG state). */
    wfm_source_t  rsrc = { .type     = 0,
                           .freq     = 0.05,
                           .freq_hi  = 0.45,
                           .ranged   = WFM_RANGE_FREQ,
                           .snr      = 100.0,
                           .snr_mode = 1,
                           .seed     = 7,
                           .sps      = 1 };
    wfm_segment_t rseg
        = { .sources = &rsrc, .n_sources = 1, .fs = 1e6, .num_samples = 128 };
    float _Complex a[256], b[256];
    dp_wfm_compose_state_t *c1 = dp_wfm_compose_create (&rseg, 1, 1, 0);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c1, a, 256) == 256,
                    "ranged freq exec");
    int diff = 0;
    for (int i = 0; i < 128; i++)
      if (a[i] != a[128 + i])
        diff = 1;
    DP_REQUIRE_MSG (diff, "ranged freq → fresh draw each repeat");
    dp_wfm_compose_state_t *c2 = dp_wfm_compose_create (&rseg, 1, 1, 0);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c2, b, 256) == 256,
                    "ranged freq exec 2");
    for (int i = 0; i < 256; i++)
      DP_REQUIRE_MSG (a[i] == b[i],
                      "ranged draw reproducible across composers");
    dp_wfm_compose_destroy (c1);
    dp_wfm_compose_destroy (c2);
  }

  /* ── ranged fields round-trip through JSON as [lo, hi] arrays ── */
  {
    wfm_source_t  s  = { .type     = 0,
                         .freq     = 100.0,
                         .freq_hi  = 200.0,
                         .ranged   = WFM_RANGE_FREQ,
                         .snr      = 10.0,
                         .snr_mode = 1,
                         .seed     = 1,
                         .sps      = 1 };
    wfm_segment_t g  = { .sources        = &s,
                         .n_sources      = 1,
                         .fs             = 1e6,
                         .num_samples    = 64,
                         .off_samples    = 10,
                         .off_samples_hi = 30,
                         .ranged         = WFM_RANGE_OFF_SAMPLES };
    char         *js = dp_wfm_spec_to_json (&g, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (js, "ranged spec to json");
    DP_REQUIRE_MSG (strstr (js, "200") && strstr (js, "30"),
                    "ranges emitted as arrays");
    dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (js);
    DP_REQUIRE_MSG (c, "ranged spec parse");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = dp_wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (nseg == 1, "ranged round-trip seg count");
    DP_REQUIRE_MSG (gg[0].ranged & WFM_RANGE_OFF_SAMPLES,
                    "off range bit survives");
    DP_REQUIRE_MSG (gg[0].off_samples == 10 && gg[0].off_samples_hi == 30,
                    "off range bounds survive");
    DP_REQUIRE_MSG (gg[0].sources[0].ranged & WFM_RANGE_FREQ,
                    "freq range bit survives");
    DP_REQUIRE_MSG (gg[0].sources[0].freq == 100.0
                        && gg[0].sources[0].freq_hi == 200.0,
                    "freq range bounds survive");
    free (js);
    dp_wfm_compose_destroy (c);
  }

  /* ── ranged snr/level/f_end + ranged num/off all drawn on execute ── */
  {
    /* A chirp with every per-source ranged field set, in a segment whose on-
     * and off-times are themselves ranged. Executing forces start_segment to
     * draw each one — the freq path is covered above; this exercises the snr,
     * level, f_end and sample-count draws. Two repeats so the draws refire. */
    wfm_source_t  rsrc = { .type     = WFM_SYNTH_CHIRP,
                           .freq     = 0.01,
                           .freq_hi  = 0.02,
                           .f_end    = 0.03,
                           .f_end_hi = 0.04,
                           .snr      = 20.0,
                           .snr_hi   = 40.0,
                           .level    = -6.0,
                           .level_hi = -1.0,
                           .ranged   = WFM_RANGE_FREQ | WFM_RANGE_FEND
                                       | WFM_RANGE_SNR | WFM_RANGE_LEVEL,
                           .snr_mode = 1,
                           .seed     = 3,
                           .sps      = 1 };
    wfm_segment_t rseg
        = { .sources        = &rsrc,
            .n_sources      = 1,
            .fs             = 1e6,
            .num_samples    = 32,
            .num_samples_hi = 64,
            .off_samples    = 8,
            .off_samples_hi = 16,
            .ranged         = WFM_RANGE_NUM_SAMPLES | WFM_RANGE_OFF_SAMPLES };
    float _Complex buf[512];
    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&rseg, 1, 1, 0);
    DP_REQUIRE_MSG (dp_wfm_compose_execute (c, buf, 512) == 512,
                    "ranged snr/level/f_end/num/off exec");
    dp_wfm_compose_destroy (c);
  }

  /* ── empty on-time with a trailing gap: the off-only segment branch ── */
  {
    /* num_samples 0 → no synth started → straight to the PHASE_OFF gap. */
    wfm_source_t  src = { .type = 0, .snr = 100.0, .seed = 1, .sps = 1 };
    wfm_segment_t g
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .off_samples = 8 };
    float _Complex buf[8];
    dp_wfm_compose_state_t *c   = dp_wfm_compose_create (&g, 1, 0, 0);
    size_t                  got = dp_wfm_compose_execute (c, buf, 8);
    DP_REQUIRE_MSG (got == 8, "off-only segment emits the gap");
    for (size_t i = 0; i < got; i++)
      DP_REQUIRE_MSG (buf[i] == 0.0f, "off-only gap is zeros");
    dp_wfm_compose_destroy (c);
  }

  /* ── chirp f_end range emits + round-trips through JSON ── */
  {
    wfm_source_t  s = { .type     = WFM_SYNTH_CHIRP,
                        .freq     = 1e5,
                        .f_end    = 2e5,
                        .f_end_hi = 3e5,
                        .ranged   = WFM_RANGE_FEND,
                        .snr      = 100.0,
                        .seed     = 1,
                        .sps      = 1 };
    wfm_segment_t g
        = { .sources = &s, .n_sources = 1, .fs = 1e6, .num_samples = 32 };
    char *js = dp_wfm_spec_to_json (&g, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (js && strstr (js, "f_end"), "chirp f_end present");
    DP_REQUIRE_MSG (strstr (js, "300000") != NULL, "f_end hi bound emitted");
    dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (js);
    DP_REQUIRE_MSG (c, "chirp f_end json parse");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = dp_wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (gg[0].sources[0].ranged & WFM_RANGE_FEND,
                    "f_end range bit survives");
    DP_REQUIRE_MSG (gg[0].sources[0].f_end == 2e5
                        && gg[0].sources[0].f_end_hi == 3e5,
                    "f_end range bounds survive");
    free (js);
    dp_wfm_compose_destroy (c);
  }

  /* ── dsss: a two-code burst source, byte-identical to the pre-spread
   * bits path it replaces, with intrinsic on-time and a JSON round-trip ── */
  {
    /* small geometry: 8-chip acq ×3, 4-chip data code, 5 payload bits,
     * 2-bit sync, crc16 — sps 2, data-symbol Es/N0 6 dB. */
    uint8_t acq[8]   = { 1, 0, 1, 1, 0, 0, 1, 0 };
    uint8_t dcode[4] = { 0, 1, 1, 0 };
    uint8_t sync[2]  = { 1, 0 };
    uint8_t pay[5]   = { 1, 0, 0, 1, 1 };

    wfm_source_t dsss = { .type         = WFM_SYNTH_DSSS,
                          .snr          = 6.0,
                          .snr_mode     = 3, /* esno: outer data symbol */
                          .seed         = 7,
                          .sps          = 2,
                          .pn_length    = 7,
                          .acq_code     = { .bits = acq, .len = 8 },
                          .acq_reps     = 3,
                          .data_code    = { .bits = dcode, .len = 4 },
                          .sync         = { .bits = sync, .len = 2 },
                          .payload.bits = pay, /* payload */
                          .payload.len  = 5,
                          .crc          = 1 };
    /* deliberately wrong num_samples: the intrinsic on-time must win */
    wfm_segment_t g = { .sources     = &dsss,
                        .n_sources   = 1,
                        .fs          = 1e6,
                        .num_samples = 17,
                        .off_samples = 10 };

    /* The burst the source should produce, through the description pair:
       the common frame over the same sync/payload/CRC, spread. */
    const wfm_seq_t hsyn = { .kind = WFM_SEQ_LITERAL, .bits = sync, .len = 2 };
    const wfm_seq_t hpay = { .kind = WFM_SEQ_LITERAL, .bits = pay, .len = 5 };
    wfm_frame_desc_t hd;
    DP_REQUIRE (dp_wfm_frame_fixed (&hd, NULL, 0, &hsyn, &hpay, 1) == 0);
    size_t nchips = dp_wfm_dsss_desc_nchips (&hd, 8, 3, 4);
    DP_REQUIRE_MSG (nchips == 8 * 3 + (2 + 5 + 16) * 4, "burst chip count");
    size_t on = nchips * 2;

    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&g, 1, 0, 0);
    DP_REQUIRE_MSG (c, "dsss create");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = dp_wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (gg[0].num_samples == on, "dsss on-time is intrinsic");
    static float _Complex dall[1024];
    size_t dt = 0;
    while ((n = dp_wfm_compose_execute (c, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          dall[dt + i] = buf[i];
        dt += n;
      }
    DP_REQUIRE_MSG (dt == on + 10, "dsss burst + gap length");
    /* (c stays alive: gg borrows its segments for the JSON emit below.) */

    /* equivalent hand-spread bits segment at the hand-converted fs SNR (the
     * exact conversion the demo used: esno − 10log10(sf·sps), mode fs). */
    static uint8_t chips[512];
    DP_REQUIRE_MSG (dp_wfm_dsss_desc_chips (&hd, NULL, acq, 8, 3, dcode, 4,
                                            chips, sizeof chips)
                        == nchips,
                    "hand chips");
    wfm_source_t            bits = { .type = WFM_SYNTH_BITS,
                                     .snr  = 6.0 - 10.0 * log10 (4.0 * 2.0),
                                     .snr_mode     = 1, /* fs */
                                     .seed         = 7,
                                     .sps          = 2,
                                     .pn_length    = 7,
                                     .payload.bits = chips,
                                     .payload.len  = nchips,
                                     .modulation   = 1 };
    wfm_segment_t           gb   = { .sources     = &bits,
                                     .n_sources   = 1,
                                     .fs          = 1e6,
                                     .num_samples = on,
                                     .off_samples = 10 };
    dp_wfm_compose_state_t *cb   = dp_wfm_compose_create (&gb, 1, 0, 0);
    DP_REQUIRE_MSG (cb, "bits create");
    static float _Complex ball[1024];
    size_t bt = 0;
    while ((n = dp_wfm_compose_execute (cb, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          ball[bt + i] = buf[i];
        bt += n;
      }
    dp_wfm_compose_destroy (cb);
    DP_REQUIRE_MSG (bt == dt, "dsss vs bits length");
    DP_REQUIRE_MSG (
        memcmp (dall, ball, dt * sizeof (float _Complex)) == 0,
        "dsss segment byte-identical to pre-spread bits at converted snr");

    /* JSON round-trip: geometry keys emitted, parse back, same bytes, and
     * the recorded num_samples is the resolved intrinsic on-time. */
    char *js = dp_wfm_spec_to_json (gg, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (c); /* json built; the borrow ends here */
    DP_REQUIRE_MSG (js && strstr (js, "\"dsss\""), "dsss type name");
    DP_REQUIRE_MSG (strstr (js, "acq_code") && strstr (js, "data_code")
                        && strstr (js, "\"payload\"")
                        && strstr (js, "\"crc\""),
                    "dsss geometry keys");
    dp_wfm_compose_state_t *jc2 = dp_wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jc2, "dsss from_json");
    static float _Complex j2[1024];
    size_t jt = 0;
    while ((n = dp_wfm_compose_execute (jc2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          j2[jt + i] = buf[i];
        jt += n;
      }
    DP_REQUIRE_MSG (
        jt == dt && memcmp (dall, j2, dt * sizeof (float _Complex)) == 0,
        "dsss json round-trip byte-identical");
    dp_wfm_compose_destroy (jc2);

    /* "pattern" USED to be an alias for "payload". A Field replaced every
     * spelling of its field, and the old ones are refused by name -- never
     * read as aliases (frame-description.md F.3). Rename the emitted key in
     * place (same length) and the scene must be refused, naming the key. */
    char *pat = strstr (js, "\"payload\"");
    DP_REQUIRE_MSG (pat, "payload key present");
    memcpy (pat, "\"pattern\"", 9);
    const char             *why = NULL;
    dp_wfm_compose_state_t *jp  = dp_wfm_compose_from_json_why (js, &why);
    DP_CHECK_MSG (jp == NULL, "a retired \"pattern\" key is refused");
    DP_CHECK_MSG (why && strstr (why, "\"pattern\""),
                  "and the refusal names the key it refused");
    dp_wfm_compose_destroy (jp);
    free (js);

    /* ebno on a dsss burst is esno (BPSK payload, 1 bit/symbol): same
     * bytes as the esno render above. */
    wfm_source_t  eb           = dsss;
    wfm_segment_t ge           = g;
    ge.sources                 = &eb;
    eb.snr_mode                = 2; /* ebno */
    dp_wfm_compose_state_t *ce = dp_wfm_compose_create (&ge, 1, 0, 0);
    DP_REQUIRE_MSG (ce, "ebno dsss create");
    static float _Complex eall[1024];
    size_t et = 0;
    while ((n = dp_wfm_compose_execute (ce, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          eall[et + i] = buf[i];
        et += n;
      }
    dp_wfm_compose_destroy (ce);
    DP_REQUIRE_MSG (
        et == dt && memcmp (dall, eall, dt * sizeof (float _Complex)) == 0,
        "dsss ebno == esno (BPSK payload)");

    /* an absent optional code is simply not emitted (no empty "" keys) */
    wfm_source_t  nos          = dsss;
    wfm_segment_t gnos         = g;
    gnos.sources               = &nos;
    nos.sync.bits              = NULL;
    nos.sync.len               = 0;
    dp_wfm_compose_state_t *cn = dp_wfm_compose_create (&gnos, 1, 0, 0);
    DP_REQUIRE_MSG (cn, "no-sync dsss create");
    size_t               nn  = 0;
    const wfm_segment_t *ggn = dp_wfm_compose_segments (cn, &nn, NULL, NULL);
    char                *jn  = dp_wfm_spec_to_json (ggn, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (cn);
    DP_REQUIRE_MSG (jn && !strstr (jn, "\"sync\""),
                    "absent sync key not emitted");
    free (jn);

    /* ── a coding stage reaches a DSSS burst ──────────────────────────
     *
     * A coding stage once reached a DSSS burst in name only: the retired
     * `--conv` flag on a dsss source produced a byte-identical waveform to
     * no `--conv` at all, because the burst was assembled by a private
     * four-field builder that had never heard of a stage (doppler#1017).
     * The burst is now the same
     * description every other source's frame is, so a rate-1/2 inner code
     * doubles the frame -- and leaves the preamble alone, because the
     * preamble is not IN the description.
     *
     * The coded burst CARRIES its description: the common frame over the
     * same sync, payload and CRC, plus an inner code over all of it. The
     * source's own sync goes into the description, since a carried frame
     * is the whole frame; the unspread preamble stays on the source.
     */
    {
      wfm_frame_desc_t cvd;
      DP_REQUIRE (dp_wfm_frame_fixed (&cvd, NULL, 0, &dsss.sync, &dsss.payload,
                                      dsss.crc)
                  == 0);
      const int cst
          = dp_wfm_frame_add_stage (&cvd, WFM_STAGE_CONV, "sync", "crc");
      DP_REQUIRE (cst >= 0);
      cvd.stage[cst].emit_num = 2u; /* rate 1/2 */
      cvd.stage[cst].emit_den = 1u;
      wfm_source_t cv         = dsss;
      memset (&cv.sync, 0, sizeof cv.sync);
      cv.frame = &cvd;
      wfm_segment_t gcv
          = { .sources = &cv, .n_sources = 1, .fs = 1e6, .off_samples = 0 };
      wfm_segment_t gpl
          = { .sources = &dsss, .n_sources = 1, .fs = 1e6, .off_samples = 0 };

      const size_t pre   = 8u * 3u; /* acq_code x acq_reps */
      const size_t frame = (2u + 5u + WFM_FRAME_CRC_BITS) * 4u;
      DP_REQUIRE_MSG (dp_wfm_source_dsss_nchips (&dsss) == pre + frame,
                      "plain burst: preamble + spread frame");
      DP_REQUIRE_MSG (dp_wfm_source_dsss_nchips (&cv) == pre + 2u * frame,
                      "a rate-1/2 inner code doubles the SPREAD part only");

      dp_wfm_compose_state_t *cp = dp_wfm_compose_create (&gpl, 1, 0, 0);
      dp_wfm_compose_state_t *cc = dp_wfm_compose_create (&gcv, 1, 0, 0);
      DP_REQUIRE_MSG (cp && cc, "both bursts compose");
      static float _Complex pbuf[4096], cbuf[4096];
      size_t pn2 = dp_wfm_compose_execute (cp, pbuf, 4096);
      size_t cn2 = dp_wfm_compose_execute (cc, cbuf, 4096);
      dp_wfm_compose_destroy (cp);
      dp_wfm_compose_destroy (cc);
      DP_REQUIRE_MSG (pn2 == (pre + frame) * 2u
                          && cn2 == (pre + 2u * frame) * 2u,
                      "the segment's on-time follows its own description");
      /* The preamble is the coherent pull-in target: a code over "the whole
         frame" must not touch it, or every receiver's acquisition breaks. */
      DP_REQUIRE_MSG (
          memcmp (pbuf, cbuf, pre * 2u * sizeof (float _Complex)) == 0,
          "the unspread preamble is identical with and without the "
          "inner code");
      /* ...and the spread part is NOT identical, or the stage did nothing. */
      DP_REQUIRE_MSG (memcmp (pbuf + pre * 2u, cbuf + pre * 2u,
                              frame * 2u * sizeof (float _Complex))
                          != 0,
                      "the inner code changed the frame it covers");

      /* A record that omits a stage is a capture nobody can rebuild. The
         SUM path writes its sources through a different function than the
         lone-source path (which keeps its own field order for byte
         identity), so both are checked -- the Python face covers the lone
         one, this covers the sum. */
      wfm_source_t  mix2[2] = { { .type = WFM_SYNTH_TONE, .freq = 0.1 }, cv };
      wfm_segment_t gmix    = { .sources     = mix2,
                                .n_sources   = 2,
                                .fs          = 1e6,
                                .num_samples = 64,
                                .off_samples = 0 };
      dp_wfm_compose_state_t *cm = dp_wfm_compose_create (&gmix, 1, 0, 0);
      DP_REQUIRE_MSG (cm, "a sum carrying a coded dsss source composes");
      size_t               nm  = 0;
      const wfm_segment_t *gm  = dp_wfm_compose_segments (cm, &nm, NULL, NULL);
      char                *jm2 = dp_wfm_spec_to_json (gm, 1, 0, 0, 0, 0.0);
      dp_wfm_compose_destroy (cm);
      DP_REQUIRE_MSG (jm2, "sum spec serialises");
      DP_REQUIRE_MSG (strstr (jm2, "\"frame\"") != NULL
                          && strstr (jm2, "\"conv\"") != NULL,
                      "a summed source's record must carry its description, "
                      "inner code and all");
      free (jm2);
    }

    /* Invalid geometry (frame bits but no spreading code) is REFUSED at
     * create, on every face, and that is a deliberate change of answer. It
     * used to build and degrade the segment to a silent gap -- the same
     * "accepted and dropped" shape this file's own create-time check exists
     * to stop, and on the CLI it read as a zero-length capture with exit 0.
     * A DSSS burst spreads its frame, so a frame with nothing to spread it
     * by is a geometry no caller can have meant (doppler#1017). */
    wfm_source_t bad   = dsss;
    bad.data_code.bits = NULL;
    bad.data_code.len  = 0;
    wfm_segment_t gbad
        = { .sources = &bad, .n_sources = 1, .fs = 1e6, .off_samples = 4 };
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&bad) != NULL,
                    "a spread frame with no data code is named as an error");
    DP_REQUIRE_MSG (dp_wfm_compose_create (&gbad, 1, 0, 0) == NULL,
                    "bad dsss is refused at create, not silently gapped");

    /* The same rule for EVERY source the synth cannot build, not only a bad
     * frame (doppler#1590). A PN register with no m-sequence polynomial
     * (length 1, or past 64) made dp_wfm_synth_create return NULL, which the
     * streaming path turned into a silent gap: `wfmgen --pn-length 65` wrote
     * zero bytes and exited 0. Asked of the builder itself, so no second
     * copy of the synth's rules exists here. */
    for (int len = 0; len < 2; len++)
      {
        wfm_source_t pn
            = { .type = WFM_SYNTH_PN, .sps = 1, .pn_length = len ? 65 : 1 };
        wfm_segment_t gpn
            = { .sources = &pn, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
        DP_REQUIRE_MSG (dp_wfm_compose_create (&gpn, 1, 0, 0) == NULL,
                        "a PN length with no m-sequence is refused at create");
      }

    /* A POLY wider than its register is masked by the generator, silently:
     * 0x40 on a 5-bit register has no tap inside it, so it emits the seed
     * and then zeros -- a constant waveform that still looks like a PN
     * source, which `wfmgen --pn-poly 0x40 --pn-length 5` wrote at exit 0
     * (doppler#1636; the Field twin was #1624). Refused at create, and the
     * rule every face asks names it. */
    {
      wfm_source_t pn = {
        .type = WFM_SYNTH_PN, .sps = 1, .pn_length = 5, .pn_poly = 0x40
      };
      wfm_segment_t gpn
          = { .sources = &pn, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
      DP_REQUIRE_MSG (dp_wfm_compose_create (&gpn, 1, 0, 0) == NULL,
                      "a pn_poly wider than pn_length is refused at create");
      const char *why = dp_wfm_source_error (&pn);
      DP_CHECK_MSG (why && strstr (why, "pn_poly")
                        && strstr (why, "pn_length"),
                    "...and the source rule names pn_poly and pn_length");
      pn.pn_poly = 0x12; /* x^5 + x^2 + 1: inside the register */
      DP_CHECK_MSG (dp_wfm_source_error (&pn) == NULL,
                    "a pn_poly inside its register is not refused");
      dp_wfm_compose_state_t *ok = dp_wfm_compose_create (&gpn, 1, 0, 0);
      DP_CHECK_MSG (ok != NULL, "...and composes");
      dp_wfm_compose_destroy (ok);

      /* The seed's twin (doppler#1640) is NOT refused: a source's seed
       * also seeds its noise, so `1000 + k` over a scene is ordinary, and
       * 1 in 2^pn_length of those used to empty the register -- a CONSTANT
       * emitter in a gallery scene, silently. A seed whose low pn_length
       * bits are zero starts the register at 1, as seed 0 does; the source
       * rule has nothing to say about it. */
      pn.pn_poly   = 0;
      pn.pn_length = 7;
      pn.seed      = 128;
      DP_CHECK_MSG (dp_wfm_source_error (&pn) == NULL,
                    "a seed that masks to zero is not a source error");
    }

    /* Same answer inside a multi-source sum: one source that cannot be built
     * refuses the whole composition rather than summing the others and
     * quietly leaving this one out. */
    wfm_source_t  mix[2] = { { .type = WFM_SYNTH_TONE, .freq = 0.1 }, bad };
    wfm_segment_t gsum   = { .sources     = mix,
                             .n_sources   = 2,
                             .fs          = 1e6,
                             .num_samples = 16,
                             .off_samples = 4 };
    DP_REQUIRE_MSG (dp_wfm_compose_create (&gsum, 1, 0, 0) == NULL,
                    "a sum carrying a bad dsss source is refused too");
  }

  /* ── repeats: bounded per-segment instancing — N instances back-to-back,
   * instance 0 byte-compatible, fresh AWGN + fresh ranged draws per
   * instance, fixed signal, JSON round-trip ── */
  {
    wfm_source_t bpsk = { .type      = WFM_SYNTH_BPSK,
                          .snr       = 3.0,
                          .snr_mode  = 3,
                          .seed      = 11,
                          .sps       = 2,
                          .pn_length = 7 };

    /* fixed durations: total = repeats * (on + off), and the clean signal
     * repeats byte-identically while a noisy one gets fresh AWGN. */
    wfm_segment_t g3 = { .sources     = &bpsk,
                         .n_sources   = 1,
                         .fs          = 1e6,
                         .num_samples = 100,
                         .off_samples = 20,
                         .repeats     = 3 };

    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&g3, 1, 0, 0);
    DP_REQUIRE_MSG (c, "repeats create");
    static float _Complex rall[512];
    size_t rt = 0;
    while ((n = dp_wfm_compose_execute (c, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          rall[rt + i] = buf[i];
        rt += n;
      }
    dp_wfm_compose_destroy (c);
    DP_REQUIRE_MSG (rt == 3 * (100 + 20), "repeats=3 total = 3*(on+off)");

    /* instance 0 == a repeats-less segment (byte-compat). */
    wfm_segment_t g1           = g3;
    g1.repeats                 = 0; /* 0 and 1 both mean one instance */
    dp_wfm_compose_state_t *c1 = dp_wfm_compose_create (&g1, 1, 0, 0);
    DP_REQUIRE_MSG (c1, "repeats-less create");
    static float _Complex r1[256];
    size_t t1 = 0;
    while ((n = dp_wfm_compose_execute (c1, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          r1[t1 + i] = buf[i];
        t1 += n;
      }
    dp_wfm_compose_destroy (c1);
    DP_REQUIRE_MSG (t1 == 120, "repeats-less length");
    DP_REQUIRE_MSG (memcmp (rall, r1, 120 * sizeof (float _Complex)) == 0,
                    "instance 0 byte-identical to a repeats-less segment");

    /* noisy instances never share an AWGN realization ... */
    DP_REQUIRE_MSG (memcmp (rall, rall + 120, 100 * sizeof (float _Complex))
                        != 0,
                    "fresh noise per instance");
    /* ... while the underlying signal is fixed: clean instances repeat
     * byte-identically. */
    wfm_source_t cb            = bpsk;
    cb.snr                     = 100.0; /* clean: no AWGN at all */
    wfm_segment_t gc           = g3;
    gc.sources                 = &cb;
    dp_wfm_compose_state_t *cc = dp_wfm_compose_create (&gc, 1, 0, 0);
    DP_REQUIRE_MSG (cc, "clean repeats create");
    static float _Complex rc[512];
    size_t tc = 0;
    while ((n = dp_wfm_compose_execute (cc, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          rc[tc + i] = buf[i];
        tc += n;
      }
    dp_wfm_compose_destroy (cc);
    DP_REQUIRE_MSG (
        tc == 360 && memcmp (rc, rc + 120, 120 * sizeof (float _Complex)) == 0
            && memcmp (rc, rc + 240, 120 * sizeof (float _Complex)) == 0,
        "signal fixed: clean instances byte-identical");

    /* ranged off_samples re-draws per instance (a jittered burst train)
     * and the instance-0 draw matches the repeats-less draw. */
    wfm_segment_t gr           = g3;
    gr.off_samples             = 10;
    gr.off_samples_hi          = 200;
    gr.ranged                  = WFM_RANGE_OFF_SAMPLES;
    dp_wfm_compose_state_t *cr = dp_wfm_compose_create (&gr, 1, 0, 0);
    DP_REQUIRE_MSG (cr, "ranged repeats create");
    size_t rtot = 0;
    while ((n = dp_wfm_compose_execute (cr, buf, 777)) > 0)
      rtot += n;
    dp_wfm_compose_destroy (cr);
    DP_REQUIRE_MSG (rtot >= 3 * 110 && rtot <= 3 * 300,
                    "ranged gaps within bounds");
    wfm_segment_t gr1            = gr;
    gr1.repeats                  = 1;
    dp_wfm_compose_state_t *cs   = dp_wfm_compose_create (&gr1, 1, 0, 0);
    size_t                  stot = 0;
    while ((n = dp_wfm_compose_execute (cs, buf, 777)) > 0)
      stot += n;
    dp_wfm_compose_destroy (cs);
    DP_REQUIRE_MSG (rtot != 3 * stot,
                    "per-instance gap draws are distinct (not 3x the first)");

    /* ── gap noise (gh-409): a noisy segment's trailing gap carries its
     * noise floor — the same AWGN stream, continued — while gap_noise=off
     * and clean scenes keep exact-zero gaps ── */
    wfm_source_t  nsy = { .type      = WFM_SYNTH_BPSK,
                          .snr       = 0.0, /* esno 0 dB, sps 2 → −3 dB fs */
                          .snr_mode  = 3,
                          .seed      = 21,
                          .sps       = 2,
                          .pn_length = 7 };
    wfm_segment_t gn  = { .sources     = &nsy,
                          .n_sources   = 1,
                          .fs          = 1e6,
                          .num_samples = 200,
                          .off_samples = 300 };
    dp_wfm_compose_state_t *cgn = dp_wfm_compose_create (&gn, 1, 0, 0);
    DP_REQUIRE_MSG (cgn, "gap-noise create");
    static float _Complex gna[512];
    size_t gt = 0;
    while ((n = dp_wfm_compose_execute (cgn, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          gna[gt + i] = buf[i];
        gt += n;
      }
    dp_wfm_compose_destroy (cgn);
    DP_REQUIRE_MSG (gt == 500, "gap-noise length");
    int gap_nz = 0;
    for (size_t i = 200; i < 500; i++)
      gap_nz += (gna[i] != 0.0f);
    DP_REQUIRE_MSG (gap_nz > 250, "noisy gap carries noise");
    double gp = 0;
    for (size_t i = 200; i < 500; i++)
      gp += creal (gna[i]) * creal (gna[i]) + cimag (gna[i]) * cimag (gna[i]);
    gp /= 300.0;
    double floor_p = pow (10.0, -(0.0 - 10.0 * log10 (2.0)) / 10.0);
    DP_REQUIRE_MSG (fabs (gp - floor_p) / floor_p < 0.35,
                    "gap noise power is the resolved floor");
    /* continuity: the gap is the seamless continuation of the on-time
     * stream — byte-identical to hand-driving the same synth. */
    dp_wfm_synth_state_t *ref = dp_wfm_compose_build_synth (
        &nsy, 1e6, 200, nsy.freq, nsy.snr, nsy.f_end, 0, 0, 0);
    DP_REQUIRE_MSG (ref, "reference synth");
    static float _Complex rr[512];
    dp_wfm_synth_steps (ref, rr, 200);
    dp_wfm_synth_noise_steps (ref, rr + 200, 300);
    dp_wfm_synth_destroy (ref);
    DP_REQUIRE_MSG (memcmp (gna, rr, 500 * sizeof (float _Complex)) == 0,
                    "gap is the byte-exact continuation of the on-time noise");
    /* the escape hatch restores hard zeros */
    wfm_segment_t goff          = gn;
    goff.gap_noise              = 1;
    dp_wfm_compose_state_t *cof = dp_wfm_compose_create (&goff, 1, 0, 0);
    DP_REQUIRE_MSG (cof, "gap-noise off create");
    size_t ot = 0, zeros = 1;
    while ((n = dp_wfm_compose_execute (cof, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          if (ot + i >= 200 && buf[i] != 0.0f)
            zeros = 0;
        ot += n;
      }
    dp_wfm_compose_destroy (cof);
    DP_REQUIRE_MSG (ot == 500 && zeros, "gap_noise=off gap is exact zeros");

    /* ── delay_samples: a leading gap — clean prefix is zeros and shifts
     * the burst; a ranged delay re-draws per instance; the span replayer
     * reports the rendered timeline exactly ── */
    wfm_source_t  cln = { .type = WFM_SYNTH_TONE, .freq = 1e5, .snr = 100.0 };
    wfm_segment_t gd  = { .sources       = &cln,
                          .n_sources     = 1,
                          .fs            = 1e6,
                          .num_samples   = 100,
                          .off_samples   = 40,
                          .delay_samples = 60 };
    dp_wfm_compose_state_t *cd = dp_wfm_compose_create (&gd, 1, 0, 0);
    DP_REQUIRE_MSG (cd, "delay create");
    static float _Complex da[256];
    size_t dt2 = 0;
    while ((n = dp_wfm_compose_execute (cd, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          da[dt2 + i] = buf[i];
        dt2 += n;
      }
    dp_wfm_compose_destroy (cd);
    DP_REQUIRE_MSG (dt2 == 200, "delay + on + off length");
    for (size_t i = 0; i < 60; i++)
      DP_REQUIRE_MSG (da[i] == 0.0f, "clean delay is zeros");
    DP_REQUIRE_MSG (da[60] != 0.0f && da[159] != 0.0f,
                    "burst placed after delay");
    for (size_t i = 160; i < 200; i++)
      DP_REQUIRE_MSG (da[i] == 0.0f, "clean trailing gap is zeros");
    /* delay=0 byte-compat: same segment without delay == da shifted */
    wfm_segment_t g0           = gd;
    g0.delay_samples           = 0;
    dp_wfm_compose_state_t *c0 = dp_wfm_compose_create (&g0, 1, 0, 0);
    static float _Complex z0[256];
    size_t zt = 0;
    while ((n = dp_wfm_compose_execute (c0, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          z0[zt + i] = buf[i];
        zt += n;
      }
    dp_wfm_compose_destroy (c0);
    DP_REQUIRE_MSG (
        zt == 140 && memcmp (da + 60, z0, 140 * sizeof (float _Complex)) == 0,
        "delayed burst is the delay-less render, shifted");
    /* ranged delay × repeats: spans replay the rendered instance timeline */
    wfm_segment_t gr2    = gd;
    gr2.delay_samples    = 10;
    gr2.delay_samples_hi = 90;
    gr2.ranged           = WFM_RANGE_DELAY_SAMPLES;
    gr2.repeats          = 3;
    wfm_span_t spans[8];
    size_t     nsp = dp_wfm_compose_spans (&gr2, 1, spans, 8);
    DP_REQUIRE_MSG (nsp == 3, "three instances replayed");
    DP_REQUIRE_MSG (spans[0].delay != spans[1].delay
                        || spans[1].delay != spans[2].delay,
                    "per-instance delay draws are distinct");

    /* ── the DRAWN values, not just the timing ──────────────────────────
     *
     * dp_wfm_compose_draws() answers "when AND what". The sidecar used to take
     * its timing from spans and its frequency/SNR from the source struct --
     * which for a ranged field holds `lo` -- so a row was exact about when
     * and wrong about what, and the exact half hid the other (doppler#1086).
     * These assert the two halves come from ONE walk: identical timing to
     * spans, one row per source, and values that actually vary. */
    {
      wfm_source_t rsrc = *gr2.sources;
      rsrc.freq         = 1e5;
      rsrc.freq_hi      = 2e5;
      rsrc.snr          = 8.0;
      rsrc.snr_hi       = 14.0;
      rsrc.ranged       = WFM_RANGE_FREQ | WFM_RANGE_SNR;
      wfm_segment_t gdw = gr2;
      gdw.sources       = &rsrc;
      gdw.n_sources     = 1;

      DP_REQUIRE_MSG (dp_wfm_compose_draws (&gdw, 1, NULL, 0) == 3,
                      "size-then-fill: one row per source per instance");
      wfm_draw_t dr[8];
      size_t     ndr = dp_wfm_compose_draws (&gdw, 1, dr, 8);
      DP_REQUIRE_MSG (ndr == 3, "three rows for three instances");

      wfm_span_t sp2[8];
      DP_REQUIRE_MSG (dp_wfm_compose_spans (&gdw, 1, sp2, 8) == 3, "3 spans");
      for (size_t i = 0; i < 3; i++)
        DP_REQUIRE_MSG (
            dr[i].start == sp2[i].start && dr[i].delay == sp2[i].delay
                && dr[i].on == sp2[i].on && dr[i].off == sp2[i].off,
            "draws and spans report ONE timeline -- they walk "
            "the same draw, so they cannot disagree");

      for (size_t i = 0; i < 3; i++)
        {
          DP_REQUIRE_MSG (dr[i].seg == 0 && dr[i].instance == i
                              && dr[i].src == 0,
                          "each row names its own (segment, instance, src)");
          DP_REQUIRE_MSG (dr[i].freq >= 1e5 && dr[i].freq <= 2e5,
                          "a drawn freq lies inside its range");
          DP_REQUIRE_MSG (dr[i].snr >= 8.0 && dr[i].snr <= 14.0,
                          "a drawn snr lies inside its range");
        }
      /* The defect's signature: reading `lo` gives three identical rows
         sitting exactly on the bound. Both are refused. */
      DP_REQUIRE_MSG (dr[0].freq != dr[1].freq || dr[1].freq != dr[2].freq,
                      "per-instance freq draws are distinct -- three equal "
                      "values is what reading the range's lo looks like");
      DP_REQUIRE_MSG (dr[0].freq != 1e5 || dr[1].freq != 1e5,
                      "a drawn freq is not the range's lo");

      /* An UN-ranged field reports its scalar, so a consumer never has to
         branch on the `ranged` bitmask to know what it is looking at. */
      wfm_source_t fsrc = rsrc;
      fsrc.ranged       = 0;
      fsrc.freq         = 1234.0;
      fsrc.snr          = 5.5;
      fsrc.level        = -3.25;
      wfm_segment_t gfx = gdw;
      gfx.sources       = &fsrc;
      wfm_draw_t fx[8];
      DP_REQUIRE_MSG (dp_wfm_compose_draws (&gfx, 1, fx, 8) == 3, "3 fixed");
      DP_REQUIRE_MSG (fx[0].freq == 1234.0 && fx[2].freq == 1234.0
                          && fx[1].snr == 5.5 && fx[1].level == -3.25,
                      "an un-ranged field reports its own scalar");
    }
    dp_wfm_compose_state_t *cr2      = dp_wfm_compose_create (&gr2, 1, 0, 0);
    size_t                  rtot2    = 0;
    size_t                  first_on = 0, seen = 0;
    while ((n = dp_wfm_compose_execute (cr2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          if (!seen && buf[i] != 0.0f)
            {
              first_on = rtot2 + i;
              seen     = 1;
            }
        rtot2 += n;
      }
    dp_wfm_compose_destroy (cr2);
    size_t expect = 0;
    for (size_t q = 0; q < 3; q++)
      expect += spans[q].delay + spans[q].on + spans[q].off;
    DP_REQUIRE_MSG (rtot2 == expect, "rendered length == replayed span total");
    DP_REQUIRE_MSG (first_on == spans[0].delay,
                    "first burst lands where the span replay says");

    /* multi-source sum: the gap accumulates every source's noise term (the
     * resolved floor source keeps running while the cleaned signal sources
     * contribute zero) — long gap so the SCRATCH_CAP chunking path runs. */
    wfm_source_t mix2[2]
        = { { .type = WFM_SYNTH_TONE, .freq = 0.05, .snr = 3.0, .seed = 5 },
            { .type  = WFM_SYNTH_TONE,
              .freq  = -0.1,
              .level = -6.0,
              .snr   = 100.0,
              .seed  = 6 } };
    wfm_segment_t           gsum2 = { .sources     = mix2,
                                      .n_sources   = 2,
                                      .fs          = 1e6,
                                      .num_samples = 100,
                                      .off_samples = 6000, /* > SCRATCH_CAP */
                                      .delay_samples = 50 };
    dp_wfm_compose_state_t *cs2   = dp_wfm_compose_create (&gsum2, 1, 0, 0);
    DP_REQUIRE_MSG (cs2, "sum gap-noise create");
    size_t st2 = 0, nz2 = 0;
    double sp2 = 0;
    while ((n = dp_wfm_compose_execute (cs2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          {
            size_t pos = st2 + i;
            if (pos >= 150 && pos < 6150)
              {
                nz2 += (buf[i] != 0.0f);
                sp2 += creal (buf[i]) * creal (buf[i])
                       + cimag (buf[i]) * cimag (buf[i]);
              }
          }
        st2 += n;
      }
    dp_wfm_compose_destroy (cs2);
    DP_REQUIRE_MSG (st2 == 50 + 100 + 6000, "sum delay+on+off length");
    DP_REQUIRE_MSG (nz2 > 5000, "sum gap carries the resolved floor");
    sp2 /= 6000.0;
    /* anchor: tone at 3 dB over fs → floor power 10^(-3/10) ≈ 0.501 */
    DP_REQUIRE_MSG (fabs (sp2 - 0.501) / 0.501 < 0.35,
                    "sum gap power ≈ floor");

    /* the span replayer's ranged num_samples branch */
    wfm_segment_t gsp  = gd;
    gsp.num_samples    = 80;
    gsp.num_samples_hi = 120;
    gsp.ranged         = WFM_RANGE_NUM_SAMPLES;
    wfm_span_t sp1[1];
    DP_REQUIRE_MSG (dp_wfm_compose_spans (&gsp, 1, sp1, 1) == 1
                        && sp1[0].on >= 80 && sp1[0].on <= 120,
                    "spans replay a ranged on-time");

    /* noise_steps guards: NULL state / zero n are no-ops */
    dp_wfm_synth_noise_steps (NULL, buf, 4);
    dp_wfm_synth_state_t *g1s = dp_wfm_compose_build_synth (
        &cln, 1e6, 100, cln.freq, cln.snr, cln.f_end, 0, 0, 0);
    DP_REQUIRE_MSG (g1s, "guard synth");
    dp_wfm_synth_noise_steps (g1s, buf, 0);
    buf[0] = 1.0f;
    dp_wfm_synth_noise_steps (g1s, buf, 1); /* clean → writes exact zeros */
    DP_REQUIRE_MSG (buf[0] == 0.0f, "clean noise_steps writes zeros");
    dp_wfm_synth_destroy (g1s);

    /* sum form emits delay/gap_noise keys too */
    wfm_segment_t gse           = gsum2;
    gse.gap_noise               = 1;
    dp_wfm_compose_state_t *cse = dp_wfm_compose_create (&gse, 1, 0, 0);
    DP_REQUIRE_MSG (cse, "sum emit create");
    size_t               nse = 0;
    const wfm_segment_t *gge = dp_wfm_compose_segments (cse, &nse, NULL, NULL);
    char                *jse = dp_wfm_spec_to_json (gge, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (cse);
    DP_REQUIRE_MSG (jse && strstr (jse, "\"sum\"")
                        && strstr (jse, "\"delay_samples\"")
                        && strstr (jse, "\"gap_noise\""),
                    "sum form emits delay + gap_noise");
    dp_wfm_compose_state_t *cre2 = dp_wfm_compose_from_json (jse);
    free (jse);
    DP_REQUIRE_MSG (cre2, "sum delay/gap_noise json parses");
    dp_wfm_compose_destroy (cre2);

    /* JSON: delay + gap_noise round-trip; omitted at defaults */
    wfm_segment_t gjd           = gd;
    gjd.gap_noise               = 1;
    dp_wfm_compose_state_t *cj2 = dp_wfm_compose_create (&gjd, 1, 0, 0);
    size_t                  nj2 = 0;
    const wfm_segment_t *gg2 = dp_wfm_compose_segments (cj2, &nj2, NULL, NULL);
    char                *jd  = dp_wfm_spec_to_json (gg2, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (cj2);
    DP_REQUIRE_MSG (jd && strstr (jd, "\"delay_samples\"")
                        && strstr (jd, "\"gap_noise\""),
                    "delay + gap_noise keys emitted");
    dp_wfm_compose_state_t *jr2 = dp_wfm_compose_from_json (jd);
    free (jd);
    DP_REQUIRE_MSG (jr2, "delay json parses");
    static float _Complex jda[256];
    size_t jdt = 0;
    while ((n = dp_wfm_compose_execute (jr2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          jda[jdt + i] = buf[i];
        jdt += n;
      }
    dp_wfm_compose_destroy (jr2);
    DP_REQUIRE_MSG (
        jdt == 200 && memcmp (da, jda, 200 * sizeof (float _Complex)) == 0,
        "delay json round-trip byte-identical");
    dp_wfm_compose_state_t *cjd = dp_wfm_compose_create (&g0, 1, 0, 0);
    size_t                  njd = 0;
    const wfm_segment_t *ggd = dp_wfm_compose_segments (cjd, &njd, NULL, NULL);
    char                *j0  = dp_wfm_spec_to_json (ggd, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (cjd);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"delay_samples\"")
                        && !strstr (j0, "\"gap_noise\""),
                    "delay + gap_noise omitted at defaults");
    free (j0);

    /* JSON: repeats emitted (and only when > 1), round-trip byte-identical */
    dp_wfm_compose_state_t *cj = dp_wfm_compose_create (&g3, 1, 0, 0);
    size_t                  nj = 0;
    const wfm_segment_t    *gj = dp_wfm_compose_segments (cj, &nj, NULL, NULL);
    char                   *js = dp_wfm_spec_to_json (gj, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (cj);
    DP_REQUIRE_MSG (js && strstr (js, "\"repeats\""), "repeats key emitted");
    dp_wfm_compose_state_t *jr = dp_wfm_compose_from_json (js);
    free (js);
    DP_REQUIRE_MSG (jr, "repeats from_json");
    static float _Complex jall2[512];
    size_t jt2 = 0;
    while ((n = dp_wfm_compose_execute (jr, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          jall2[jt2 + i] = buf[i];
        jt2 += n;
      }
    dp_wfm_compose_destroy (jr);
    DP_REQUIRE_MSG (
        jt2 == rt && memcmp (rall, jall2, rt * sizeof (float _Complex)) == 0,
        "repeats json round-trip byte-identical");
    dp_wfm_compose_state_t *c1j = dp_wfm_compose_create (&g1, 1, 0, 0);
    size_t                  n1j = 0;
    const wfm_segment_t *g1j = dp_wfm_compose_segments (c1j, &n1j, NULL, NULL);
    char                *j1  = dp_wfm_spec_to_json (g1j, 1, 0, 0, 0, 0.0);
    dp_wfm_compose_destroy (c1j);
    DP_REQUIRE_MSG (j1 && !strstr (j1, "\"repeats\""),
                    "repeats omitted at 1 (old specs unchanged)");
    free (j1);
  }

  /* ── the resolved floor reproduces the bundled noise power ──────────────
   *
   * dp_wfm_snr_over_fs() decides where a multi-source segment's shared noise
   * floor sits; dp_wfm_synth_create() decides how much noise a single bundled
   * source makes. If those two disagree, the same requested SNR means two
   * different things depending on how many sources happen to share a segment
   * — and nothing else in the tree would say so, because each is internally
   * consistent. That claim used to be a comment ("mirrors the conversion in
   * wfm_synth_core.c"), over a second copy of the formula.
   *
   * The expected noise powers below are LITERALS, derived by hand, and that
   * is the whole point of the test. The first version of it computed the
   * expectation by calling dp_wfm_snr_over_fs() and compared that against the
   * noise the generator made — but both now route through one shared
   * conversion, so the two sides moved together: dropping the bits-per-symbol
   * term from the Eb/No branch left the test green. A known answer cannot
   * follow the code it checks.
   */
  {
    /* Unit-power sources, so P_total - 1 IS the noise power. N = 10^(-snr_fs
       /10) with snr_fs = snr - 10log10(span) for Es/No, plus 10log10(bps)
       first for Eb/No, and snr itself over fs. At snr = 9 dB:

         tone  fs    sps=8    snr_fs = +9.0000   N = 0.125893
         bpsk  Es/No sps=8    snr_fs = -0.0309   N = 1.007140
         bpsk  Eb/No sps=8    snr_fs = -0.0309   N = 1.007140  (bps=1)
         qpsk  Es/No sps=4    snr_fs = +2.9794   N = 0.503570
         qpsk  Eb/No sps=4    snr_fs = +5.9897   N = 0.251785  (bps=2)
         bpsk  auto  sps=16   snr_fs = -3.0412   N = 2.014281  (auto->Es/No)
         tone  auto  sps=16   snr_fs = +9.0000   N = 0.125893  (auto->fs) */
    const struct
    {
      int    type, mode, sps;
      double expect;
    } cases[] = {
      { 0, 1, 8, 0.125893 },  { 3, 3, 8, 1.007140 }, { 3, 2, 8, 1.007140 },
      { 4, 3, 4, 0.503570 },  { 4, 2, 4, 0.251785 }, { 3, 0, 16, 2.014281 },
      { 0, 0, 16, 0.125893 },
    };
    const double snr_db = 9.0;
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++)
      {
        wfm_source_t            s  = { .type      = cases[k].type,
                                       .freq      = 0.0,
                                       .snr       = snr_db,
                                       .snr_mode  = cases[k].mode,
                                       .seed      = 11,
                                       .sps       = cases[k].sps,
                                       .pn_length = 9,
                                       .pn_poly   = 0 };
        wfm_segment_t           g  = { .sources     = &s,
                                       .n_sources   = 1,
                                       .fs          = 1e6,
                                       .num_samples = 200000,
                                       .off_samples = 0 };
        dp_wfm_compose_state_t *cc = dp_wfm_compose_create (&g, 1, 0, 0);
        DP_REQUIRE_MSG (cc, "floor/bundled: create");
        double p   = 0.0;
        size_t got = 0, nn;
        float _Complex b[4096];
        while ((nn = dp_wfm_compose_execute (cc, b, 4096)) > 0)
          {
            for (size_t i = 0; i < nn; i++)
              p += (double)(crealf (b[i]) * crealf (b[i])
                            + cimagf (b[i]) * cimagf (b[i]));
            got += nn;
          }
        dp_wfm_compose_destroy (cc);
        DP_REQUIRE_MSG (got == 200000, "floor/bundled: sample count");
        double measured = p / (double)got - 1.0; /* minus the unit signal */
        char   msg[160];
        snprintf (msg, sizeof msg,
                  "type=%d mode=%d sps=%d: carries noise %.4f, the requested "
                  "%.0f dB means %.4f",
                  cases[k].type, cases[k].mode, cases[k].sps, measured, snr_db,
                  cases[k].expect);
        /* 3% covers the statistical spread at 200k samples; the errors this
           catches are factors (a missing 10log10(sps) is 9 dB at sps=8, and a
           dropped bits-per-symbol term is 3 dB at QPSK). */
        DP_REQUIRE_MSG (
            fabs (measured - cases[k].expect) < 0.03 * cases[k].expect, msg);
      }
  }

  /* ── an UNSPREAD frame: the descriptor is the waveform ──────────────────
   *
   * These fields were accepted, stored, readable back, and applied on no
   * unspread face at all: `dp_wfm_source_attach_dsss` returned early for every
   * non-dsss type and nothing else consumed them, so a caller who asked for a
   * framed BPSK waveform got an unframed one and no way to find out. What made
   * that survivable is that no test asserted the frame CHANGES anything — so
   * that is what these assert, at the layer both the CLI and Python cross.  */
  {
    static const uint8_t sync_bits[13]
        = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 }; /* Barker-13 */
    static const uint8_t acq_bits[8] = { 1, 0, 1, 0, 1, 0, 1, 0 };
    static const uint8_t payload[16]
        = { 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
    wfm_source_t plain   = { .type         = WFM_SYNTH_BITS,
                             .snr          = 100.0,
                             .sps          = 1,
                             .pn_length    = 7,
                             .payload.bits = (uint8_t *)payload,
                             .payload.len  = sizeof payload,
                             .modulation   = 1 /* bpsk */ };
    wfm_source_t framed  = plain;
    framed.acq_code.bits = acq_bits;
    framed.acq_code.len  = sizeof acq_bits;
    framed.acq_reps      = 4;
    framed.sync.bits     = sync_bits;
    framed.sync.len      = sizeof sync_bits;
    framed.crc           = 1;

    DP_REQUIRE_MSG (!dp_wfm_source_has_frame (&plain),
                    "a preamble-less, sync-less source is not framed");
    DP_REQUIRE_MSG (dp_wfm_source_has_frame (&framed), "this one is");
    /* `crc` alone must NOT read as a frame: it defaults to crc16 on every
       source, so treating it as intent would append a trailer to every
       unframed pattern ever generated. */
    wfm_source_t crc_only = plain;
    crc_only.crc          = 1;
    DP_REQUIRE_MSG (!dp_wfm_source_has_frame (&crc_only),
                    "crc alone is a default, not an intent to frame");

    /* The frame is honoured where the payload is explicit, and refused with a
       reason where it is not — never accepted and dropped. */
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&framed) == NULL,
                    "bits is fine");

    /* A PN-sourced waveform CAN be framed now, given a payload. #755 refused
       --type bpsk outright because the synth's LFSR is endless and nothing
       said where the payload stopped; a payload length is that bound, so the
       question is the one BITS always answered -- is there a payload at all
       (gh-762). */
    wfm_source_t framed_pn = framed;
    framed_pn.type         = WFM_SYNTH_BPSK; /* symbols from the PN LFSR */
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&framed_pn) == NULL,
                    "a bounded payload is what a framed PN-sourced waveform "
                    "was ever missing");
    dp_wfm_synth_state_t *psy = dp_wfm_source_to_synth (&framed_pn, 1e6);
    DP_REQUIRE_MSG (psy, "and it BUILDS on the standalone face");
    dp_wfm_synth_destroy (psy);

    /* The same source with a GENERATED payload -- the shape `--bits pn:N:REG`
       resolves to, and what makes a 100k-bit frame six numbers in a record. */
    wfm_source_t framed_gen = framed_pn;
    framed_gen.payload
        = (wfm_seq_t){ .kind = WFM_SEQ_PN, .len = 64, .reg_bits = 7 };
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&framed_gen) == NULL,
                    "a generated payload is a payload -- tested on LENGTH, "
                    "never on the array a generated kind does not have");
    dp_wfm_synth_state_t *gsy = dp_wfm_source_to_synth (&framed_gen, 1e6);
    DP_REQUIRE_MSG (gsy, "a framed waveform whose payload is GENERATED "
                         "builds -- the last place gh-762's flattening "
                         "survived was this descriptor's payload field");
    dp_wfm_synth_destroy (gsy);

    /* doppler#1592: the two paths that still read the POINTER. A generated
       payload has no array, so an unframed `bits` source refused it and a
       continuous dsss source silently swapped it for the PRBS default. Each is
       held to the same bits given LITERALLY, and the continuous one also to
       the diverging default, so a path that ignored the payload cannot pass
       by emitting something plausible. */
    {
      const wfm_seq_t gen = { .kind = WFM_SEQ_PN, .len = 64, .reg_bits = 7 };
      uint8_t         lit[64];
      DP_REQUIRE_MSG (dp_wfm_seq_bits (&gen, lit, 64) == 64,
                      "the generated payload materialises");
      const wfm_seq_t litq
          = { .kind = WFM_SEQ_LITERAL, .len = 64, .bits = lit };
      /* 8192 samples: ~100 data symbols at 80 samples each on the
         continuous leg. Fewer, and the first bits of two PN streams seeded
         alike can coincide, so the fixture stops telling them apart. */
      enum
      {
        N = 8192
      };
      static float _Complex a[N], b[N];

      wfm_source_t ub          = WFM_SOURCE_DEFAULTS;
      ub.type                  = WFM_SYNTH_BITS;
      ub.payload               = gen;
      dp_wfm_synth_state_t *ga = dp_wfm_source_to_synth (&ub, 1e6);
      ub.payload               = litq;
      dp_wfm_synth_state_t *la = dp_wfm_source_to_synth (&ub, 1e6);
      DP_REQUIRE_MSG (ga && la, "an unframed bits source builds from a "
                                "GENERATED payload, not only a literal one");
      dp_wfm_synth_steps (ga, a, N);
      dp_wfm_synth_steps (la, b, N);
      DP_CHECK_MSG (memcmp (a, b, sizeof a) == 0,
                    "unframed bits: generated == the same bits literally");
      dp_wfm_synth_destroy (ga);
      dp_wfm_synth_destroy (la);

      uint8_t      dcode[4] = { 1, 0, 1, 1 };
      wfm_source_t cd       = WFM_SOURCE_DEFAULTS;
      cd.type               = WFM_SYNTH_DSSS;
      cd.sps                = 2;
      cd.symbol_rate        = 12500.0;
      cd.crc                = 0; /* unframed: the default crc16 would make
                                    this a FRAME, and the payload would ride
                                    attach_frame instead of the data path
                                    under test -- which is exactly how the
                                    first version of this pin passed with the
                                    defect put back */
      cd.data_code
          = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 4, .bits = dcode };
      cd.payload               = gen;
      dp_wfm_synth_state_t *gc = dp_wfm_source_to_synth (&cd, 1e6);
      cd.payload               = litq;
      dp_wfm_synth_state_t *lc = dp_wfm_source_to_synth (&cd, 1e6);
      cd.payload               = (wfm_seq_t){ 0 };
      dp_wfm_synth_state_t *pc = dp_wfm_source_to_synth (&cd, 1e6);
      static float _Complex c[N];
      DP_REQUIRE_MSG (gc && lc && pc, "continuous dsss builds all three");
      dp_wfm_synth_steps (gc, a, N);
      dp_wfm_synth_steps (lc, b, N);
      dp_wfm_synth_steps (pc, c, N);
      DP_CHECK_MSG (memcmp (a, c, sizeof a) != 0,
                    "precondition: the payload and the PRBS default differ");
      DP_CHECK_MSG (memcmp (a, b, sizeof a) == 0,
                    "continuous dsss: a generated payload is SENT, not "
                    "silently replaced by the PRBS default");
      dp_wfm_synth_destroy (gc);
      dp_wfm_synth_destroy (lc);
      dp_wfm_synth_destroy (pc);
    }

    wfm_source_t framed_empty = framed;
    framed_empty.payload.bits = NULL;
    framed_empty.payload.len  = 0;
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&framed_empty) != NULL,
                    "a frame with no payload is refused");

    /* A type that carries no bit stream at all still cannot be framed, and
       the refusal REACHES the caller on both faces. Asserting only that the
       predicate returns a message would leave the two construction paths free
       to ignore it — which is precisely the shape of the bug: the fields were
       validated nowhere and dropped silently. */
    wfm_source_t framed_chirp = framed;
    framed_chirp.type         = WFM_SYNTH_CHIRP;
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&framed_chirp) != NULL,
                    "a chirp has no bit stream to frame, payload or not");
    DP_REQUIRE_MSG (!dp_wfm_source_to_synth (&framed_chirp, 1.0),
                    "the standalone face refuses a frame the waveform type "
                    "cannot carry");
    wfm_segment_t bad_seg = { .sources     = &framed_chirp,
                              .n_sources   = 1,
                              .fs          = 1e6,
                              .num_samples = 64,
                              .off_samples = 0 };
    DP_REQUIRE_MSG (!dp_wfm_compose_create (&bad_seg, 1, 0, 0),
                    "and the composer refuses the same segment, before it "
                    "builds anything");

    /* THE assertion whose absence was the bug. */
    size_t nb = 32 + 13 + 16 + 16; /* preamble + sync + payload + crc */
    float _Complex *a = malloc (nb * sizeof *a);
    float _Complex *b = malloc (nb * sizeof *b);
    DP_REQUIRE_MSG (a && b, "alloc");
    /* dp_wfm_compose_build_synth is THE single synth-construction path (the
       standalone Synth reaches the same attach through the shared bridge —
       covered from Python, where that face actually lives). */
    dp_wfm_synth_state_t *sp = dp_wfm_compose_build_synth (
        &plain, 1.0, nb, 0.0, 100.0, 0.0, 0, 0, 0);
    dp_wfm_synth_state_t *sf = dp_wfm_compose_build_synth (
        &framed, 1.0, nb, 0.0, 100.0, 0.0, 0, 0, 0);
    DP_REQUIRE_MSG (sp && sf, "both sources build");
    dp_wfm_synth_steps (sp, a, nb);
    dp_wfm_synth_steps (sf, b, nb);
    DP_REQUIRE_MSG (memcmp (a, b, nb * sizeof *a) != 0,
                    "a framed source must not emit the unframed waveform");

    /* And it is not merely DIFFERENT — it is the descriptor's own bits, so
       the layout, the CRC's position and its bit order come from the one
       place the receiver reads them from too. */
    const wfm_seq_t fpre = { .kind = WFM_SEQ_LITERAL,
                             .bits = acq_bits,
                             .len  = sizeof acq_bits };
    const wfm_seq_t fsyn = { .kind = WFM_SEQ_LITERAL,
                             .bits = sync_bits,
                             .len  = sizeof sync_bits };
    const wfm_seq_t fpay
        = { .kind = WFM_SEQ_LITERAL, .bits = payload, .len = sizeof payload };
    wfm_frame_desc_t        f;
    wfm_frame_desc_layout_t fl;
    DP_REQUIRE (dp_wfm_frame_fixed (&f, &fpre, 4, &fsyn, &fpay, 1) == 0);
    DP_REQUIRE_MSG (dp_wfm_frame_desc_layout (&f, &fl) == 0
                        && fl.frame_bits == nb,
                    "the frame is nb bits");
    uint8_t *want = malloc (nb);
    DP_REQUIRE_MSG (want && dp_wfm_frame_assemble (&f, NULL, want, nb) == nb,
                    "frame bits");
    for (size_t i = 0; i < nb; i++)
      {
        /* bpsk: bit 0 -> +1, bit 1 -> -1 (wfm_synth's mapping, sps == 1). */
        float expect = want[i] ? -1.0f : 1.0f;
        DP_REQUIRE_MSG (fabsf (crealf (b[i]) - expect) < 1e-6f,
                        "the framed stream IS the assembly of its own "
                        "description, symbol for symbol");
      }
    /* One frame, then it CYCLES — which is what turns a one-frame description
       into a multi-frame record without a repeat count in the descriptor. */
    float _Complex       *c2 = malloc (2 * nb * sizeof *c2);
    dp_wfm_synth_state_t *sc = dp_wfm_compose_build_synth (
        &framed, 1.0, 2 * nb, 0.0, 100.0, 0.0, 0, 0, 0);
    DP_REQUIRE_MSG (c2 && sc, "cycle alloc");
    dp_wfm_synth_steps (sc, c2, 2 * nb);
    DP_REQUIRE_MSG (memcmp (c2, c2 + nb, nb * sizeof *c2) == 0,
                    "the frame repeats verbatim");

    /* ── an UNBUILDABLE frame must FAIL the build, on both paths ──────────
     *
     * dp_wfm_frame_assemble() refuses a description it cannot materialise
     * rather than half-writing one, and the two construction paths have to
     * turn that into a NULL synth. Without that they would fall through to an
     * unframed waveform — the very failure the rest of this section exists to
     * pin, one layer down and this time silent even to a byte comparison,
     * because there would be nothing to compare against.
     *
     * A preamble LENGTH with no preamble ARRAY is that state. No face can
     * currently spell it — the CLI and wfm_json.c both derive the length FROM
     * the array — so it is a C-level guard and it is asserted in C. The
     * user-facing rule now catches it FIRST, with a sentence: the source's
     * frame is asked whether it assembles, and a field that cannot be built
     * is a frame that does not. The build paths still refuse on their own,
     * because a caller may build without asking. */
    wfm_source_t broken  = framed;
    broken.acq_code.bits = NULL; /* .len and acq_reps still set */
    DP_REQUIRE_MSG (dp_wfm_source_has_frame (&broken),
                    "still reads as framed");
    const char *bwhy = dp_wfm_source_frame_error (&broken);
    DP_REQUIRE_MSG (bwhy && strstr (bwhy, "does not assemble"),
                    "and is refused as a frame that does not assemble");
    DP_REQUIRE_MSG (
        !dp_wfm_compose_build_synth (&broken, 1.0, nb, 0.0, 100.0, 0.0, 0, 0,
                                     0),
        "the composer's build fails rather than emitting an unframed "
        "waveform");
    DP_REQUIRE_MSG (!dp_wfm_source_to_synth (&broken, 1.0),
                    "and the standalone bridge agrees — they share the attach "
                    "for exactly this reason");

    dp_wfm_synth_destroy (sp);
    dp_wfm_synth_destroy (sf);
    dp_wfm_synth_destroy (sc);
    free (a);
    free (b);
    free (c2);
    free (want);
  }

  /* ── the interleaver's span INCLUDES the outer code's check symbols ──
   *
   * The interleave stage covers the whole data group -- payload, its CRC,
   * and the outer code's check symbols -- and the
   * flag guard validated payload + CRC only. So the check ran against a
   * DIFFERENT span from the one the stage permutes, and refused the
   * canonical CCSDS arrangement: 223 octets under RS(255,223) interleaved 5
   * deep at unit 8. 1784 bits does not divide by 40; the 2040 the stage
   * actually covers divides exactly 51 times.
   *
   * That refusal was pinned in the flag-matrix golden as an expected exit 2,
   * which is how a guard rejecting valid input survives: the evidence for
   * the flag was the failure it caused. */
  {
    static uint8_t frame[223 * 8]; /* 223 octets, the RS(255,223) message */
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i & 1u);

    const wfm_seq_t pl
        = { .kind = WFM_SEQ_LITERAL, .bits = frame, .len = sizeof frame };
    wfm_frame_desc_t d5;
    const coded_t    c5 = { .payload = &pl, .rs = 1, .ilv = 5, .unit = 8 };
    DP_REQUIRE (coded_frame (&c5, &d5) == 0);
    wfm_source_t cadu = { .type       = WFM_SYNTH_BITS,
                          .snr        = 100.0,
                          .sps        = 1,
                          .pn_length  = 7,
                          .modulation = 1,
                          .crc        = 0,
                          .frame      = &d5 };
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&cadu) == NULL,
                    "223 octets + RS parity is 2040 bits, which 5 x 8 "
                    "divides 51 times -- the arrangement CCSDS specifies");

    /* The refusal still bites, or losing it would have passed the case
       above just as well. It is provoked WITHOUT disturbing the outer code's
       own geometry, so the interleaver is what refuses: keep the 223 octets
       and change the depth -- 2040 divides by 5*8 and does not divide by
       2*8, because 255 is odd. A bad kernel geometry no longer lays out
       cleanly and assembles to NOTHING: the source refuses it, with a
       sentence, before anything is generated. */
    wfm_frame_desc_t d2    = d5;
    d2.stage[1].depth      = 2;
    wfm_source_t odd_depth = cadu;
    odd_depth.frame        = &d2;
    const char *why        = dp_wfm_source_frame_error (&odd_depth);
    DP_REQUIRE_MSG (why != NULL,
                    "a data group that is not a whole number of units is "
                    "still refused");
    DP_REQUIRE_MSG (strstr (why, "does not assemble") != NULL,
                    "and refused as a frame that does not ASSEMBLE -- the "
                    "kernel's own rule, asked by running it");

    /* The outer code's rule, by the same question: a short payload is
       refused rather than padded (virtual fill is not implemented). */
    static const uint8_t sixteen[16] = { 0 };
    const wfm_seq_t      s16
        = { .kind = WFM_SEQ_LITERAL, .bits = sixteen, .len = sizeof sixteen };
    wfm_frame_desc_t dshort;
    const coded_t    cshort = { .payload = &s16, .rs = 1 };
    DP_REQUIRE (coded_frame (&cshort, &dshort) == 0);
    wfm_source_t short_rs = cadu;
    short_rs.frame        = &dshort;
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&short_rs) != NULL,
                    "an outer code over 16 bits is refused, not padded");

    /* And without an outer code the span is payload + CRC, unchanged: 16
       payload bits and no CRC is two units of 8, so depth 2 divides it. */
    wfm_frame_desc_t dno;
    const coded_t    cno = { .payload = &s16, .ilv = 2, .unit = 8 };
    DP_REQUIRE (coded_frame (&cno, &dno) == 0);
    wfm_source_t no_outer = cadu;
    no_outer.frame        = &dno;
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&no_outer) == NULL,
                    "with no outer code the group is payload + CRC");
  }

  /* ── a coded description assembles to the RIGHT frame, on the wire ────
   *
   * `coded_frame` above writes each shape by name, the way a `--frame FILE`
   * does, and the source carries it. The oracle is the frame built by hand
   * from the stage KERNELS,
   * in the order and over the spans frame-description.md states: the CRC
   * over the payload; the outer code over payload + CRC
   * (dp_ccsds_tm_frame_encode with only `rs_depth` set); the randomiser
   * and then the interleaver over that data group; then marker, preamble, sync
   * and the data group on the wire; then the inner code over all of it.
   *
   * No description is involved on the oracle's side, so a builder that
   * wired a stage over the wrong fields, or in the wrong order, disagrees
   * on the ASSEMBLED BITS. (The oracle used to be a second description
   * builder, `dp_ccsds_tm_frame_desc_of`; two derivations of the covers
   * agreeing was never evidence that either was right, which is why it was
   * deleted.)
   *
   * The stage LIST is asserted too, not only the bits (doppler#1031): a
   * spare zero-initialised stage reads as CRC16 over no fields -- "does not
   * run" -- so it assembles byte-identically and is still wrong.
   *
   * Every case must assemble (the REQUIRE below): a refusal on both sides
   * would be agreement about nothing.
   */
  {
    static uint8_t payload[223 * 8];
    for (size_t i = 0; i < sizeof payload; i++)
      payload[i] = (uint8_t)((i * 7u + (i >> 3)) & 1u); /* structured */
    static const uint8_t syncw[13] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };
    static const uint8_t pre[8]    = { 1, 0, 1, 0, 1, 0, 1, 0 };

    const struct
    {
      int         asm_, crc, rs, rand, conv;
      unsigned    ilv, unit;
      size_t      n_sync, n_pre, reps;
      const char *what;
    } CASES[] = {
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "payload alone" },
      { 0, 1, 0, 0, 0, 0, 0, 13, 0, 0, "sync + CRC" },
      { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, "ASM + CRC" },
      { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, "outer code" },
      { 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, "CRC inside the outer code" },
      { 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, "ASM + outer + randomiser" },
      { 1, 0, 1, 1, 0, 5, 8, 0, 0, 0, "and an interleaver" },
      { 1, 0, 1, 1, 1, 5, 8, 0, 0, 0, "and the inner code" },
      { 0, 1, 0, 0, 0, 0, 0, 13, 8, 4, "a preamble, repeated" },
    };

    for (size_t c = 0; c < sizeof CASES / sizeof CASES[0]; c++)
      {
        /* The outer code needs payload PLUS its CRC to be exactly
           223*depth octets -- virtual fill is not implemented and a short
           frame is refused rather than padded (the source's own guard says
           so). Size the payload to match, or the case tests a refusal
           instead of a frame. */
        size_t n_bits = sizeof payload;
        if (CASES[c].rs)
          n_bits = (size_t)223u * (size_t)CASES[c].rs * 8u
                   - (CASES[c].crc ? WFM_FRAME_CRC_BITS : 0u);

        const wfm_seq_t pls
            = { .kind = WFM_SEQ_LITERAL, .bits = payload, .len = n_bits };
        const wfm_seq_t sys = { .kind = WFM_SEQ_LITERAL,
                                .bits = syncw,
                                .len  = CASES[c].n_sync };
        const wfm_seq_t prs
            = { .kind = WFM_SEQ_LITERAL, .bits = pre, .len = CASES[c].n_pre };
        const coded_t    cc = { .asm_    = CASES[c].asm_,
                                .pre     = CASES[c].n_pre ? &prs : NULL,
                                .reps    = CASES[c].reps,
                                .sync    = CASES[c].n_sync ? &sys : NULL,
                                .payload = &pls,
                                .crc     = CASES[c].crc,
                                .rs      = (unsigned)CASES[c].rs,
                                .rand    = (unsigned)CASES[c].rand,
                                .ilv     = CASES[c].ilv,
                                .unit    = CASES[c].unit,
                                .conv    = CASES[c].conv };
        wfm_frame_desc_t d;
        DP_REQUIRE_MSG (coded_frame (&cc, &d) == 0, CASES[c].what);
        /* A source carries it, and has nothing to say against it. */
        const wfm_source_t src = { .type       = WFM_SYNTH_BITS,
                                   .snr        = 100.0,
                                   .sps        = 1,
                                   .pn_length  = 7,
                                   .modulation = 1,
                                   .frame      = &d };
        DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) == NULL,
                        CASES[c].what);

        const unsigned want_stages
            = (unsigned)(!!CASES[c].crc + !!CASES[c].rs + !!CASES[c].rand
                         + !!CASES[c].ilv + !!CASES[c].conv);
        DP_CHECK_MSG (d.n_stages == want_stages, CASES[c].what);
        for (unsigned i = 0; i < d.n_stages; i++)
          DP_CHECK_MSG (d.stage[i].n_fields > 0,
                        "every stage in the list covers something");

        wfm_frame_desc_layout_t la;
        DP_REQUIRE (dp_wfm_frame_desc_layout (&d, &la) == 0);
        uint8_t *got = (uint8_t *)malloc (la.out_bits);
        DP_REQUIRE (got != NULL);
        wfm_frame_ops_t ops;
        dp_ccsds_tm_frame_ops (&ops, NULL);
        const size_t ng = dp_wfm_frame_assemble (&d, &ops, got, la.out_bits);

        /* The oracle. Sized for the largest case: marker, preamble reps,
           sync and a 255-octet codeblock, doubled by the inner code. */
        enum
        {
          CAP = 2 * (32 + 8 * 4 + 13 + 255 * 8 * 2)
        };
        static uint8_t data[CAP], wire[CAP], want[CAP], oct[CAP / 8];
        size_t         nd = n_bits;
        memcpy (data, payload, nd);
        if (CASES[c].crc)
          {
            const uint16_t crc = dp_crc16_ccitt (data, nd);
            for (size_t i = 0; i < WFM_FRAME_CRC_BITS; i++)
              data[nd++] = (uint8_t)((crc >> (15 - i)) & 1u); /* MSB-first */
          }
        if (CASES[c].rs)
          {
            DP_REQUIRE (nd % 8u == 0);
            for (size_t i = 0; i < nd / 8u; i++)
              {
                uint8_t v = 0;
                for (unsigned b = 0; b < 8u; b++)
                  v = (uint8_t)((unsigned)(v << 1u) | data[i * 8u + b]);
                oct[i] = v;
              }
            const ccsds_tm_frame_cfg_t outer
                = { .rs_depth = (unsigned)CASES[c].rs };
            nd = dp_ccsds_tm_frame_encode (&outer, NULL, oct, nd / 8u, data,
                                           CAP);
            DP_REQUIRE_MSG (nd > 0, CASES[c].what);
          }
        /* The randomiser by its own kernel, not through the encoder: the
           encoder's `randomise` is on/off and always 10.4.1's generator,
           while a stage's value 2 selects 10.4.2's. */
        if (CASES[c].rand)
          dp_ccsds_tm_randomise_with (CASES[c].rand == 2
                                          ? &dp_CCSDS_TM_RAND_LEGACY
                                          : &dp_CCSDS_TM_RAND,
                                      data, nd);
        if (CASES[c].ilv)
          {
            const size_t per = (size_t)CASES[c].ilv * CASES[c].unit;
            DP_REQUIRE (nd % per == 0);
            dp_interleave_u8 (data, wire, CASES[c].ilv, nd / per,
                              CASES[c].unit);
            memcpy (data, wire, nd);
          }

        size_t nw = 0;
        if (CASES[c].asm_)
          {
            dp_ccsds_tm_asm_bits (wire);
            nw = CCSDS_TM_ASM_BITS;
          }
        for (size_t r = 0; r < CASES[c].reps; r++)
          for (size_t i = 0; i < CASES[c].n_pre; i++)
            wire[nw++] = pre[i];
        for (size_t i = 0; i < CASES[c].n_sync; i++)
          wire[nw++] = syncw[i];
        memcpy (wire + nw, data, nd);
        nw += nd;

        const uint8_t *expect = wire;
        size_t         ne     = nw;
        if (CASES[c].conv)
          {
            /* The inner code alone: no outer code makes the frame the
               block, so the encoder's input is exactly the wire above. */
            DP_REQUIRE (nw % 8u == 0);
            for (size_t i = 0; i < nw / 8u; i++)
              {
                uint8_t v = 0;
                for (unsigned b = 0; b < 8u; b++)
                  v = (uint8_t)((unsigned)(v << 1u) | wire[i * 8u + b]);
                oct[i] = v;
              }
            const ccsds_tm_frame_cfg_t inner = { .convolutional = 1 };
            ne = dp_ccsds_tm_frame_encode (&inner, NULL, oct, nw / 8u, want,
                                           CAP);
            expect = want;
          }

        DP_REQUIRE_MSG (ng == ne && ng > 0, CASES[c].what);
        DP_CHECK_MSG (la.out_bits == ne,
                      "the layout predicts the length the frame assembles to");
        DP_CHECK_MSG (memcmp (got, expect, ng) == 0, CASES[c].what);

        /* And the SOURCE puts exactly that on the wire -- the carried
           description is what its synth plays, stage kernels and all. */
        uint8_t *wire_b = (uint8_t *)malloc (ng);
        DP_REQUIRE (wire_b != NULL);
        DP_CHECK_MSG (wire_bits (&src, wire_b, ng) == ng
                          && memcmp (wire_b, got, ng) == 0,
                      CASES[c].what);
        free (wire_b);
        free (got);
      }
  }

  /* ── a GENERATED sync reaches the wire THROUGH THE SOURCE ─────────────
   *
   * gh-762 step 2. `wfm_seq_t` has had four kinds all along and the frame
   * layer materialises every one of them, but no caller could spell
   * anything but LITERAL: the source's frame builder rebuilt each field as
   * a fresh literal, so a source's kind was discarded one call before the
   * descriptor could see it. The source now carries `wfm_seq_t` (step 1) and
   * the bridge passes it through, which is the whole change.
   *
   * The truth is `dp_pn_generate` over the same three numbers -- an EXTERNAL
   * one. A round trip through the frame would agree with itself perfectly
   * while regenerating the wrong sequence, which is exactly the shape that
   * let a Gold field sit differentially-checked and wrong.
   */
  {
    static uint8_t pay[64];
    for (size_t i = 0; i < sizeof pay; i++)
      pay[i] = (uint8_t)((i * 5u + 1u) & 1u);

    wfm_source_t src;
    memset (&src, 0, sizeof src);
    src.type          = WFM_SYNTH_BITS;
    src.sps           = 2;
    src.payload.bits  = pay;
    src.payload.len   = sizeof pay;
    src.sync.kind     = WFM_SEQ_PN;
    src.sync.len      = 31u; /* one period of a 5-bit register */
    src.sync.reg_bits = 5u;
    src.sync.seed     = 3u;

    /* A generated sequence has NO array, so a source that tested its frame
       on the pointer read this as unframed and emitted the payload bare. */
    DP_REQUIRE_MSG (dp_wfm_source_has_frame (&src),
                    "a PN sync frames a source -- the test is on length, "
                    "not on an array a generated kind never has");
    DP_REQUIRE_MSG (dp_wfm_source_frame_error (&src) == NULL,
                    "and it is a buildable shape");

    /* Read off the WIRE: the frame is [sync | payload] (no preamble, no
       CRC), so the sync word is the first 31 bits the source emits. */
    static uint8_t got[31 + 64];
    DP_REQUIRE_MSG (wire_bits (&src, got, sizeof got) == sizeof got,
                    "the source builds");

    static uint8_t want[31];
    dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (5u), 3u, 5u, 0);
    DP_REQUIRE_MSG (pn != NULL, "dp_pn_create");
    DP_REQUIRE_MSG (dp_pn_generate (pn, 31u, want, 31u) == 31u,
                    "dp_pn_generate");
    dp_pn_destroy (pn);
    DP_REQUIRE_MSG (
        memcmp (got, want, 31u) == 0,
        "a PN sync declared on the SOURCE is dp_pn_generate of its "
        "own three numbers, on the wire -- the kind SURVIVED the bridge, "
        "which is the whole of gh-762 step 2");
    DP_REQUIRE_MSG (memcmp (got + 31, pay, sizeof pay) == 0,
                    "and the payload follows it");

    /* A literal source still sends a literal, unchanged. Both directions: a
       bridge that stamped PN on everything would pass the assertion above
       and break every existing caller. */
    static const uint8_t lit[4] = { 1, 0, 0, 1 };
    wfm_source_t         plain  = src;
    memset (&plain.sync, 0, sizeof plain.sync);
    plain.sync.kind = WFM_SEQ_LITERAL;
    plain.sync.bits = lit;
    plain.sync.len  = sizeof lit;
    uint8_t got2[sizeof lit + 64];
    DP_REQUIRE (wire_bits (&plain, got2, sizeof got2) == sizeof got2);
    DP_REQUIRE_MSG (memcmp (got2, lit, sizeof lit) == 0
                        && memcmp (got2 + sizeof lit, pay, sizeof pay) == 0,
                    "a literal sync is still the caller's literal bits");
  }

  /* ── a GENERATED sequence SURVIVES --record → --from-file ─────────────
   *
   * gh-762 step 3. `add_bit_string` writes nothing when there are no bits,
   * and a generated sequence has none by definition -- so before this a PN
   * sync VANISHED from the record and `--from-file` rebuilt an unframed
   * waveform at exit 0. That is the same silent-unframed shape
   * `add_frame_fields`'s own comment warns about for the type gate, and it
   * is worse here: the whole reason to carry (poly, seed, reg_bits) instead
   * of a million-symbol array is that the METADATA reproduces the capture.
   *
   * Asserted on the assembled BITS, not on the struct: a record that merely
   * looked alike would prove nothing about the waveform it rebuilds.
   */
  {
    static uint8_t pay[64];
    for (size_t i = 0; i < sizeof pay; i++)
      pay[i] = (uint8_t)((i * 3u + 1u) & 1u);

    wfm_source_t src;
    memset (&src, 0, sizeof src);
    src.type          = WFM_SYNTH_BITS;
    src.sps           = 2;
    src.payload.bits  = pay;
    src.payload.len   = sizeof pay;
    src.sync.kind     = WFM_SEQ_PN;
    src.sync.len      = 31u;
    src.sync.reg_bits = 5u;
    src.sync.seed     = 3u;
    src.sync.poly     = 0u; /* derive the maximal-length polynomial */

    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 256 };
    char *js = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (js, "to_json");
    DP_REQUIRE_MSG (strstr (js, "\"pn:31:5:0x3\""),
                    "a generated sync is RECORDED as its Field text -- it "
                    "has no bit string, so the numbers ARE the record");
    DP_REQUIRE_MSG (!strstr (js, "\"sync_gen\""),
                    "under ONE key: the retired sync_gen is never written");
    DP_REQUIRE_MSG (!strstr (js, "\"kind\""),
                    "and no parameter object beside it -- the text is the "
                    "whole of the field");

    dp_wfm_compose_state_t *jc = dp_wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jc, "from_json");

    /* The bits the RELOADED source puts on the wire must equal
       dp_pn_generate -- an EXTERNAL truth, so a record that round-tripped
       its own mistake perfectly would still fail. The frame is
       [sync | payload], so the sync word leads. */
    size_t               jn = 0;
    const wfm_segment_t *js_seg
        = dp_wfm_compose_segments (jc, &jn, NULL, NULL);
    DP_REQUIRE (jn == 1 && js_seg[0].n_sources == 1);
    static uint8_t got[31 + 64];
    DP_REQUIRE_MSG (wire_bits (&js_seg[0].sources[0], got, sizeof got)
                        == sizeof got,
                    "the reloaded source builds");

    static uint8_t want[31];
    dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (5u), 3u, 5u, 0);
    DP_REQUIRE (pn != NULL);
    DP_REQUIRE (dp_pn_generate (pn, 31u, want, 31u) == 31u);
    dp_pn_destroy (pn);
    DP_REQUIRE_MSG (
        memcmp (got, want, 31u) == 0,
        "the recorded PN sync is dp_pn_generate of its own numbers");

    dp_wfm_compose_destroy (jc);
    free (js);

    /* GOLD and DOTTED through the same round trip, and a DSSS source's
       spreading code, so every branch of the codec is driven by a test
       rather than by one kind standing in for three. The Gold taps are the
       header's own worked example, and the check is again EXTERNAL --
       dp_gold_generate of the same five numbers. */
    {
      wfm_source_t g;
      memset (&g, 0, sizeof g);
      g.type               = WFM_SYNTH_DSSS;
      g.sps                = 2;
      g.payload.bits       = pay;
      g.payload.len        = 8;
      g.acq_reps           = 2;
      g.acq_code.kind      = WFM_SEQ_DOTTED;
      g.acq_code.len       = 8u;
      g.data_code.kind     = WFM_SEQ_PN;
      g.data_code.len      = 7u;
      g.data_code.reg_bits = 3u;
      g.data_code.seed     = 1u;
      g.sync.kind          = WFM_SEQ_GOLD;
      g.sync.len           = 16u;
      g.sync.reg_bits      = 10u;
      g.sync.taps_a        = 934u;
      g.sync.seed_a        = 350u;
      g.sync.taps_b        = 567u;
      g.sync.seed_b        = 73u;

      wfm_segment_t gseg
          = { .sources = &g, .n_sources = 1, .fs = 1e6, .num_samples = 512 };
      char *gjs = dp_wfm_spec_to_json (&gseg, 1, 0, 0, 0, 0.0);
      DP_REQUIRE_MSG (gjs, "gold/dotted to_json");
      DP_REQUIRE_MSG (strstr (gjs, "\"dotted:8*2\"")
                          && strstr (gjs, "\"pn:7:3:0x1\"")
                          && strstr (gjs, "\"gold:16:10:"),
                      "all three sequences record their Field text, the "
                      "preamble's repetitions as its *REPS");
      DP_REQUIRE_MSG (strstr (gjs, "0x3a6:0x15e:0x237:0x49"),
                      "a Gold tap mask is recorded as HEX -- a uint64 does "
                      "not survive a JSON number, and text does");

      dp_wfm_compose_state_t *gc = dp_wfm_compose_from_json (gjs);
      DP_REQUIRE_MSG (gc, "gold/dotted from_json");

      /* The Gold sync's bits, against dp_gold_generate of the same numbers,
         from the RELOADED source. A burst's frame is the common frame over
         its sync, payload and CRC (the preamble is outside it, unspread),
         so that description -- built from what the record gave back -- is
         what is spread. */
      size_t               gn = 0;
      const wfm_segment_t *gseg2
          = dp_wfm_compose_segments (gc, &gn, NULL, NULL);
      DP_REQUIRE (gn == 1 && gseg2[0].n_sources == 1);
      const wfm_source_t *gr = &gseg2[0].sources[0];
      wfm_frame_desc_t    gd;
      DP_REQUIRE (
          dp_wfm_frame_fixed (&gd, NULL, 0, &gr->sync, &gr->payload, gr->crc)
          == 0);
      const int gi = dp_wfm_frame_field_index (&gd, "sync");
      DP_REQUIRE (gi >= 0);
      wfm_frame_desc_layout_t gl;
      DP_REQUIRE (dp_wfm_frame_desc_layout (&gd, &gl) == 0);
      static uint8_t gbits[4096];
      DP_REQUIRE (dp_wfm_frame_assemble (&gd, NULL, gbits, sizeof gbits)
                  == gl.out_bits);
      static uint8_t   gwant[16];
      dp_gold_state_t *gs = dp_gold_create (934u, 350u, 567u, 73u, 10u);
      DP_REQUIRE (gs != NULL);
      DP_REQUIRE (dp_gold_generate (gs, 16u, gwant, 16u) == 16u);
      dp_gold_destroy (gs);
      DP_REQUIRE_MSG (memcmp (gbits + gl.field_off[gi], gwant, 16u) == 0,
                      "a recorded Gold sync IS dp_gold_generate of its own "
                      "five numbers");
      dp_wfm_compose_destroy (gc);
      free (gjs);
    }

    /* Refusals. Each of these BUILDS a waveform if ignored rather than
       refused, and it is not the recorded one -- which is the one failure a
       record exists to prevent. */
#define BITS_SCENE(EXTRA)                                                     \
  "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","          \
  "\"payload\":\"0101\"," EXTRA "}]}"
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (BITS_SCENE ("\"sync\":\"martian:8\"")),
        "an unknown kind is refused, not silently dropped -- a newer writer's "
        "record must not load as a different waveform");
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (BITS_SCENE (
            "\"sync\":\"0110\",\"sync_gen\":{\"kind\":\"pn\",\"len\":8,"
            "\"reg_bits\":3}")),
        "a retired sync_gen beside the Field is refused, not merged");
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (BITS_SCENE ("\"sync\":\"pn:8:0\"")),
        "a PN with no register width is refused");
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (BITS_SCENE ("\"sync\":5")),
        "a Field that is not text is refused -- a number there is a writer "
        "this reader does not understand");
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (BITS_SCENE ("\"sync\":\"pn:0:5\"")),
        "a generator of no length is refused -- length is the one parameter "
        "no default can supply");
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (
            BITS_SCENE ("\"sync\":\"gold:8:65:1:1:1:1\"")),
        "a Gold register wider than the 64 bits dp_gold_create() holds is "
        "refused, not silently masked down");
    /* The coding sugar is retired, each key refused with what replaced it:
       a coded frame is a "frame" description. Loading any of them as though
       it were absent would build an UNCODED waveform from a coded record. */
    {
      static const char *const coding[]
          = { "\"rs_depth\":1",   "\"randomise\":\"ccsds\"",
              "\"asm\":true",     "\"conv\":true",
              "\"interleave\":4", "\"interleave_unit\":8" };
      for (size_t k = 0; k < sizeof coding / sizeof *coding; k++)
        {
          char scene[256];
          (void)snprintf (scene, sizeof scene,
                          "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,"
                          "\"type\":\"bits\",\"payload\":\"0101\",%s}]}",
                          coding[k]);
          const char             *why = NULL;
          dp_wfm_compose_state_t *rc
              = dp_wfm_compose_from_json_why (scene, &why);
          DP_CHECK_MSG (rc == NULL && why && strstr (why, "retired")
                            && strstr (why, "\"frame\""),
                        "a retired coding key is refused, naming \"frame\"");
          dp_wfm_compose_destroy (rc);
        }
    }
#undef BITS_SCENE
    DP_REQUIRE_MSG (
        !dp_wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"dsss\","
            "\"data_code\":\"martian:8\"}]}"),
        "a malformed data_code is refused on the dsss source too -- the "
        "spread half reads its Fields through the same parser");
  }

  /* ── the chip path MATERIALISES a generated code ─────────────────
   *
   * The DSSS chip builders take raw arrays, never a description, so a
   * generated `data_code`/`acq_code` -- whose parameters ARE the code, with
   * `bits == NULL` -- has to be expanded before it reaches them. Read through
   * as a pointer it dereferenced NULL; refused on the pointer it became "no
   * code at all". Both are asserted on the STANDALONE face, because that is
   * the one a `--from-file` record restores through. */
  {
    static const uint8_t dcode4[4] = { 0, 1, 1, 0 };
    static const uint8_t sync2[2]  = { 1, 0 };
    static uint8_t       pay5[5]   = { 1, 0, 0, 1, 1 };

    /* CONTINUOUS, spread by a generated PN. This is exactly the shape a
       recorded `data_code_gen` restores to. */
    wfm_source_t cgen
        = { .type        = WFM_SYNTH_DSSS,
            .snr         = 40.0,
            .snr_mode    = 1,
            .seed        = 7,
            .sps         = 2,
            .pn_length   = 7,
            .symbol_rate = 1e4,
            .data_code   = { .kind = WFM_SEQ_PN, .len = 31, .reg_bits = 5 } };
    dp_wfm_synth_state_t *cs = dp_wfm_source_to_synth (&cgen, 1e6);
    DP_REQUIRE_MSG (cs,
                    "a continuous stream spread by a GENERATED code builds -- "
                    "a generated code carries no array, so a pointer test "
                    "refused every recorded data_code_gen on this face");
    dp_wfm_synth_destroy (cs);

    /* A burst with NO acquisition preamble. The expansion still runs over the
       absent field and must yield nothing rather than invent one. */
    wfm_source_t          nopre = { .type      = WFM_SYNTH_DSSS,
                                    .snr       = 40.0,
                                    .snr_mode  = 1,
                                    .seed      = 7,
                                    .sps       = 2,
                                    .pn_length = 7,
                                    .data_code = { .bits = dcode4, .len = 4 },
                                    .sync      = { .bits = sync2, .len = 2 },
                                    .payload.bits = pay5,
                                    .payload.len  = 5,
                                    .crc          = 1 };
    dp_wfm_synth_state_t *ns    = dp_wfm_source_to_synth (&nopre, 1e6);
    DP_REQUIRE_MSG (ns, "a burst with a sync word and no preamble is a burst");
    dp_wfm_synth_destroy (ns);

    /* A code that is DECLARED and unbuildable -- a length with no array -- is
       refused on BOTH chip paths. Spreading whatever the fresh buffer held
       would produce a capture no receiver can be scored against. */
    wfm_source_t cbad = cgen;
    cbad.data_code    = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 8 };
    DP_REQUIRE_MSG (!dp_wfm_source_to_synth (&cbad, 1e6),
                    "a continuous spreading code with a length and no bits is "
                    "refused");

    wfm_source_t bbad = nopre;
    bbad.acq_code     = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 8 };
    bbad.acq_reps     = 3;
    DP_REQUIRE_MSG (!dp_wfm_source_to_synth (&bbad, 1e6),
                    "a burst preamble with a length and no bits is refused");
  }

  /* ── clock Doppler (gh-942) ───────────────────────────────────────────
   *
   * The regression that matters is BLOCK SIZE, not the split point. A
   * Doppler channel is a resampler: it consumes ~n*(1+d) inputs per n
   * outputs, so the composer's "pull k, get k" only holds because the
   * renderer keeps a holdover. Get that wrong and the output depends on how
   * the caller happened to chunk its reads -- which #939 shows a
   * split-resume test does NOT catch, because both halves can fit inside
   * one internal block and never exercise the boundary at all.
   *
   * So: render the same scene through a range of block sizes, including
   * ones that are not divisors of the feed, and require every one to be
   * bit-identical to a single-call render.
   */
  {
    wfm_source_t  src = { .type       = WFM_SYNTH_TONE,
                          .freq       = 1e5,
                          .snr        = 40.0,
                          .seed       = 7,
                          .doppler    = 25.0, /* ppm */
                          .carrier_hz = 2.5e9 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 20000 };

    enum
    {
      N = 20000
    };
    static float _Complex ref[N], got[N];

    dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (c, "a doppler source composes");
    size_t nref = 0;
    for (size_t n; (n = dp_wfm_compose_execute (c, ref + nref, N - nref)) > 0;)
      nref += n;
    dp_wfm_compose_destroy (c);
    DP_REQUIRE_MSG (nref == N, "doppler scene still yields its full on-time");

    /* Deliberately awkward sizes: 1 exercises the holdover every sample,
       4096 is exactly the internal feed, and 4095/4097 straddle it. */
    static const size_t blocks[] = { 1, 3, 511, 1000, 4095, 4096, 4097, 9973 };
    for (size_t bi = 0; bi < sizeof blocks / sizeof *blocks; bi++)
      {
        size_t b = blocks[bi];
        c        = dp_wfm_compose_create (&seg, 1, 0, 0);
        DP_REQUIRE (c);
        size_t ngot = 0;
        for (size_t n; ngot < N;)
          {
            size_t want = (N - ngot < b) ? N - ngot : b;
            n           = dp_wfm_compose_execute (c, got + ngot, want);
            if (n == 0)
              break;
            ngot += n;
          }
        dp_wfm_compose_destroy (c);
        DP_REQUIRE_MSG (ngot == nref,
                        "a doppler render's LENGTH is block-size invariant");
        DP_REQUIRE_MSG (memcmp (ref, got, nref * sizeof *ref) == 0,
                        "a doppler render is bit-identical at every block "
                        "size -- the holdover, not the chunking, decides it");
      }

    /* The channel must actually be doing something, or the invariance above
       is the invariance of a no-op. 25 ppm over 20000 samples at 1 MHz is
       half a sample of dilation and a 62.5 kHz carrier term, so the same
       scene without Doppler must differ. */
    wfm_source_t plain = src;
    plain.doppler      = 0.0;
    plain.carrier_hz   = 0.0;
    wfm_segment_t pseg = seg;
    pseg.sources       = &plain;
    c                  = dp_wfm_compose_create (&pseg, 1, 0, 0);
    DP_REQUIRE (c);
    size_t npl = 0;
    for (size_t n; (n = dp_wfm_compose_execute (c, got + npl, N - npl)) > 0;)
      npl += n;
    dp_wfm_compose_destroy (c);
    DP_REQUIRE_MSG (memcmp (ref, got, nref * sizeof *ref) != 0,
                    "doppler changes the waveform (else the block-size "
                    "invariance above proves nothing)");
  }

  /* ── a PERSIST pass keeps moving through the gap ──────────────────────
   *
   * An emitter does not stop moving because its burst ended, so the channel
   * runs over the off-time too -- on the noise floor, which is what a gap
   * carries. The observable consequence is that a LONGER gap leaves the pass
   * further along by the time the next burst starts.
   *
   * Two renders of the same two-burst scene differing ONLY in gap length,
   * compared over the second burst. If the channel were skipped over gaps
   * (or reset per instance), the second burst would be identical in both and
   * this would fail -- which is exactly what makes it a test of the
   * behaviour rather than of the plumbing.
   *
   * The source is CLEAN, so gaps are exact zeros and the synth's AWGN stream
   * cannot itself carry the gap-length difference into burst 2. The synths
   * are torn down per instance, so burst 2's signal is otherwise identical
   * between the two renders: the channel is the only thing that can differ.
   */
  {
    enum
    {
      B    = 4000, /* burst   */
      G1   = 1000, /* short gap */
      G2   = 9000, /* long gap  */
      CAP2 = 2 * (B + G2)
    };
    static float _Complex a[CAP2], b[CAP2];

    wfm_source_t src = { .type             = WFM_SYNTH_TONE,
                         .freq             = 5e4,
                         .snr              = WFM_SYNTH_SNR_CLEAN,
                         .seed             = 11,
                         .doppler          = 5.0,   /* ppm   */
                         .doppler_rate     = 200.0, /* ppm/s */
                         .carrier_hz       = 2.0e9,
                         .doppler_lifetime = WFM_DOPPLER_PERSIST };

    size_t          got[2];
    float _Complex *bufs[2] = { a, b };
    const size_t    gaps[2] = { G1, G2 };
    for (int g = 0; g < 2; g++)
      {
        wfm_segment_t           seg = { .sources     = &src,
                                        .n_sources   = 1,
                                        .fs          = 1e6,
                                        .num_samples = B,
                                        .off_samples = gaps[g],
                                        .repeats     = 2 };
        dp_wfm_compose_state_t *c   = dp_wfm_compose_create (&seg, 1, 0, 0);
        DP_REQUIRE_MSG (c, "a persisting-doppler scene composes");
        size_t nn = 0;
        for (size_t n;
             (n = dp_wfm_compose_execute (c, bufs[g] + nn, CAP2 - nn)) > 0;)
          nn += n;
        dp_wfm_compose_destroy (c);
        got[g] = nn;
      }

    /* Burst 1 starts at 0 in both and has seen no gap yet: identical. */
    DP_REQUIRE_MSG (memcmp (a, b, B * sizeof *a) == 0,
                    "the FIRST burst is unaffected by what follows it");
    /* Burst 2 starts after the gap in each render. */
    DP_REQUIRE_MSG (got[0] >= (size_t)(B + G1 + B)
                        && got[1] >= (size_t)(B + G2 + B),
                    "both renders reached their second burst");
    DP_REQUIRE_MSG (memcmp (a + B + G1, b + B + G2, B * sizeof *a) != 0,
                    "a longer gap leaves the pass further along -- the "
                    "channel runs on the noise floor while the burst is "
                    "absent, so doppler_rate is per SECOND, not per unit "
                    "of on-time");

    /* And the LIMIT of that, pinned so the header's claim stays true: the
       channel is keyed by (segment, source), because a position is the only
       source identity the composer has. Two SEGMENTS declaring the same
       parameters are therefore two passes, not one continued -- each starts
       at its own t=0, so segment 1's burst matches segment 0's rather than
       carrying on from it. Sharing across segments needs a declared source
       id that the scene format does not have (gh-942). */
    wfm_source_t two_src = src;
    two_src.doppler_rate = 0.0; /* offset only: a pass that has not moved */
    wfm_segment_t two[2] = {
      { .sources = &two_src, .n_sources = 1, .fs = 1e6, .num_samples = B },
      { .sources = &two_src, .n_sources = 1, .fs = 1e6, .num_samples = B }
    };
    dp_wfm_compose_state_t *c2 = dp_wfm_compose_create (two, 2, 0, 0);
    DP_REQUIRE_MSG (c2, "a two-segment persisting scene composes");
    size_t n2 = 0;
    for (size_t n; (n = dp_wfm_compose_execute (c2, a + n2, CAP2 - n2)) > 0;)
      n2 += n;
    dp_wfm_compose_destroy (c2);
    DP_REQUIRE_MSG (n2 == 2 * B, "both segments rendered");
    DP_REQUIRE_MSG (memcmp (a, a + B, B * sizeof *a) == 0,
                    "each SEGMENT gets its own pass: identical declarations "
                    "render identically, because the key is (segment, "
                    "source) and there is no cross-segment source id");
  }

  /* ── a RANGED doppler draws per instance, and replays ─────────────────
   *
   * `doppler`/`doppler_rate` join freq/snr/level/f_end as [lo, hi] fields,
   * which is the main testing path the capability is for: sweeping a
   * receiver over a span of geometries without writing a scene per point.
   * Two properties, and the second is the one that makes a recorded sweep
   * reproducible:
   *
   *   - instances DIFFER (the draw actually varies), and
   *   - a second identical render REPLAYS them exactly (the draw hashes its
   *     key rather than consuming RNG state).
   */
  {
    enum
    {
      B  = 2048,
      R  = 3,
      NT = R * B
    };
    static float _Complex r1[NT], r2[NT];

    /* BOTH ranged fields: doppler_rate draws from its own stream, and
       ranging only the offset would leave that one unexercised. */
    wfm_source_t src
        = { .type            = WFM_SYNTH_TONE,
            .freq            = 1e5,
            .snr             = WFM_SYNTH_SNR_CLEAN,
            .seed            = 3,
            .carrier_hz      = 1.0e9,
            .doppler         = -20.0,
            .doppler_hi      = 20.0,
            .doppler_rate    = -50.0,
            .doppler_rate_hi = 50.0,
            .ranged          = WFM_RANGE_DOPPLER | WFM_RANGE_DOPPLER_RATE };
    wfm_segment_t seg = { .sources     = &src,
                          .n_sources   = 1,
                          .fs          = 1e6,
                          .num_samples = B,
                          .repeats     = R };

    float _Complex *outs[2] = { r1, r2 };
    for (int pass = 0; pass < 2; pass++)
      {
        dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
        DP_REQUIRE_MSG (c, "a ranged-doppler scene composes");
        size_t nn = 0;
        for (size_t n;
             (n = dp_wfm_compose_execute (c, outs[pass] + nn, NT - nn)) > 0;)
          nn += n;
        dp_wfm_compose_destroy (c);
        DP_REQUIRE_MSG (nn == NT, "ranged-doppler scene yields every burst");
      }

    DP_REQUIRE_MSG (memcmp (r1, r2, NT * sizeof *r1) == 0,
                    "a ranged doppler REPLAYS: the draw hashes its key, so a "
                    "recorded sweep reproduces byte-for-byte");
    DP_REQUIRE_MSG (memcmp (r1, r1 + B, B * sizeof *r1) != 0,
                    "consecutive instances draw DIFFERENT doppler (else the "
                    "replay above is the replay of a constant)");
  }

  /* ── the JSON face carries Doppler (gh-942) ───────────────────────────
   *
   * Not "the keys appear" -- what a face is for is that a scene written
   * through it RENDERS the same. So each case round-trips a spec and
   * compares the two composes sample-for-sample: a key emitted but not
   * read, read into the wrong field, or silently dropped, all land as a
   * difference here rather than as a passing string match.
   *
   * The persist case is what pins `doppler_lifetime` specifically: with the
   * lifetime lost in the round trip the reparsed scene renders PER_INSTANCE,
   * which restarts the geometry at the second burst and cannot match.
   */
  {
    struct
    {
      const char  *what;
      wfm_source_t src;
      size_t       repeats;
      size_t       off;
    } cases[] = {
      { "scalar doppler",
        { .type       = WFM_SYNTH_TONE,
          .freq       = 1e5,
          .snr        = WFM_SYNTH_SNR_CLEAN,
          .seed       = 7,
          .doppler    = 25.0,
          .carrier_hz = 2.5e9 },
        1,
        0 },
      { "doppler_rate with no offset",
        { .type         = WFM_SYNTH_TONE,
          .freq         = 5e4,
          .snr          = WFM_SYNTH_SNR_CLEAN,
          .seed         = 9,
          .doppler_rate = 150.0,
          .carrier_hz   = 1.2e9 },
        1,
        0 },
      { "ranged doppler over repeats",
        { .type            = WFM_SYNTH_TONE,
          .freq            = 5e4,
          .snr             = WFM_SYNTH_SNR_CLEAN,
          .seed            = 11,
          .doppler         = -20.0,
          .doppler_hi      = 20.0,
          .doppler_rate    = -50.0,
          .doppler_rate_hi = 50.0,
          .carrier_hz      = 2.0e9,
          .ranged          = WFM_RANGE_DOPPLER | WFM_RANGE_DOPPLER_RATE },
        3,
        500 },
      { "persist across a gap",
        { .type             = WFM_SYNTH_TONE,
          .freq             = 5e4,
          .snr              = WFM_SYNTH_SNR_CLEAN,
          .seed             = 13,
          .doppler          = 5.0,
          .doppler_rate     = 200.0,
          .carrier_hz       = 2.0e9,
          .doppler_lifetime = WFM_DOPPLER_PERSIST },
        2,
        1500 },
      /* carrier_hz alone: no channel is built, so this pins that the key
         round-trips WITHOUT quietly turning into a warp. */
      { "carrier_hz with no doppler",
        { .type       = WFM_SYNTH_TONE,
          .freq       = 5e4,
          .snr        = WFM_SYNTH_SNR_CLEAN,
          .seed       = 17,
          .carrier_hz = 2.0e9 },
        1,
        0 },
    };

    enum
    {
      JN = 6000
    };
    static float _Complex ja[JN], jb[JN];

    for (size_t ci = 0; ci < sizeof cases / sizeof *cases; ci++)
      {
        wfm_segment_t seg = { .sources     = &cases[ci].src,
                              .n_sources   = 1,
                              .fs          = 1e6,
                              .num_samples = 1200,
                              .off_samples = cases[ci].off,
                              .repeats     = cases[ci].repeats };
        char         *js  = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
        DP_REQUIRE_MSG (js, "doppler spec serialises");

        dp_wfm_compose_state_t *c0 = dp_wfm_compose_create (&seg, 1, 0, 0);
        dp_wfm_compose_state_t *c1 = dp_wfm_compose_from_json (js);
        DP_REQUIRE_MSG (c0 && c1, "doppler spec reparses");

        size_t n0 = 0, n1 = 0;
        for (size_t n;
             (n = dp_wfm_compose_execute (c0, ja + n0, JN - n0)) > 0;)
          n0 += n;
        for (size_t n;
             (n = dp_wfm_compose_execute (c1, jb + n1, JN - n1)) > 0;)
          n1 += n;
        dp_wfm_compose_destroy (c0);
        dp_wfm_compose_destroy (c1);

        DP_REQUIRE_MSG (n0 == n1 && n0 > 0, cases[ci].what);
        DP_REQUIRE_MSG (memcmp (ja, jb, n0 * sizeof *ja) == 0,
                        "a doppler scene renders identically through JSON");
        free (js);
      }

    /* A ranged field records its SPAN, not one instance's draw: a spec
       answers "what does this permit", and freezing a draw into it would
       make --from-file replay one instance of a sweep instead of the sweep.
       (dp_wfm_compose_draws() answers the other question -- see below.) */
    {
      wfm_source_t  r = { .type       = WFM_SYNTH_TONE,
                          .snr        = WFM_SYNTH_SNR_CLEAN,
                          .doppler    = -20.0,
                          .doppler_hi = 20.0,
                          .ranged     = WFM_RANGE_DOPPLER };
      wfm_segment_t seg
          = { .sources = &r, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
      char *js = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
      DP_REQUIRE_MSG (js && strstr (js, "\"doppler\":\t[-20, 20]"),
                      "a ranged doppler records its span");
      free (js);
    }

    /* OMITTED at the default, exactly as level and background are: a scene
       that asks for no Doppler must serialise byte-for-byte as it did before
       these keys existed, or every recorded spec in the world churns. */
    {
      wfm_source_t q
          = { .type = WFM_SYNTH_TONE, .snr = WFM_SYNTH_SNR_CLEAN, .sps = 8 };
      wfm_segment_t seg
          = { .sources = &q, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
      char *js = dp_wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
      DP_REQUIRE_MSG (js, "clean spec serialises");
      DP_REQUIRE_MSG (!strstr (js, "doppler") && !strstr (js, "carrier_hz"),
                      "a scene with no Doppler emits no Doppler keys");
      free (js);
      /* And the absent keys read back as no channel at all, not as a warp
         of zero that still resamples. */
      dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (
          "{\"segments\":[{\"type\":\"tone\",\"fs\":1e6,"
          "\"num_samples\":64}]}");
      DP_REQUIRE_MSG (c, "a Doppler-free spec parses");
      dp_wfm_compose_destroy (c);
    }
  }

  /* ── dp_wfm_compose_draws() reports the DRAWN Doppler (gh-942) ────────────
   *
   * The surface exists so "what is rendered and what is reported cannot
   * disagree" (doppler#1086: the sidecar read timing from a replay and
   * VALUES from the source struct, so a ranged field annotated every
   * instance with its `lo`, measured up to 6.0 dB out). Doppler is drawn
   * like freq/snr/level/f_end, so it has to be reported like them.
   */
  {
    wfm_source_t src
        = { .type            = WFM_SYNTH_TONE,
            .snr             = WFM_SYNTH_SNR_CLEAN,
            .seed            = 5,
            .doppler         = 2.0,
            .doppler_hi      = 9.0,
            .doppler_rate    = 0.1,
            .doppler_rate_hi = 0.5,
            .carrier_hz      = 1.5e9,
            .ranged          = WFM_RANGE_DOPPLER | WFM_RANGE_DOPPLER_RATE };
    wfm_segment_t seg = { .sources     = &src,
                          .n_sources   = 1,
                          .fs          = 1e6,
                          .num_samples = 256,
                          .repeats     = 3 };
    wfm_draw_t    rows[8];
    DP_REQUIRE_MSG (dp_wfm_compose_draws (&seg, 1, rows, 8) == 3,
                    "one row per instance");
    for (int i = 0; i < 3; i++)
      {
        DP_REQUIRE_MSG (rows[i].doppler >= 2.0 && rows[i].doppler <= 9.0,
                        "the drawn doppler lies inside its declared span");
        DP_REQUIRE_MSG (rows[i].doppler_rate >= 0.1
                            && rows[i].doppler_rate <= 0.5,
                        "the drawn doppler_rate lies inside its span");
      }
    /* The #1086 failure exactly: reporting the struct's `lo` on every row
       is inside the span too, so the span check alone cannot see it. */
    DP_REQUIRE_MSG (rows[0].doppler != rows[1].doppler
                        || rows[1].doppler != rows[2].doppler,
                    "instances report DIFFERENT drawn doppler (reporting the "
                    "spec's lo on every row is what doppler#1086 was)");
    DP_REQUIRE_MSG (rows[0].doppler_rate != rows[1].doppler_rate
                        || rows[1].doppler_rate != rows[2].doppler_rate,
                    "instances report DIFFERENT drawn doppler_rate");

    /* A FIXED doppler passes its scalar through unchanged -- the same
       contract freq/snr have, and what makes the row readable without
       knowing whether the field was ranged. */
    wfm_source_t  f  = { .type    = WFM_SYNTH_TONE,
                         .snr     = WFM_SYNTH_SNR_CLEAN,
                         .doppler = 12.5 };
    wfm_segment_t fg = { .sources     = &f,
                         .n_sources   = 1,
                         .fs          = 1e6,
                         .num_samples = 256,
                         .repeats     = 2 };
    DP_REQUIRE_MSG (dp_wfm_compose_draws (&fg, 1, rows, 8) == 2,
                    "2 fixed rows");
    DP_REQUIRE_MSG (rows[0].doppler == 12.5 && rows[1].doppler == 12.5,
                    "a fixed doppler reports its scalar on every instance");
  }

  /* doppler#1596: a key a scene OMITS takes the manifest's default -- the
     same value the flags and Python give -- not one the JSON reader typed for
     itself. Compared against the generated macros, never against a restated
     number, so this cannot drift with them. */
  {
    const wfm_source_t      ds = WFM_SOURCE_DEFAULTS;
    const wfm_segment_t     dg = WFM_SEGMENT_DEFAULTS;
    dp_wfm_compose_state_t *jc = dp_wfm_compose_from_json (
        "{\"version\":1,\"segments\":[{\"type\":\"pn\",\"fs\":1000}]}");
    DP_REQUIRE_MSG (jc, "a scene that omits every default parses");
    size_t               jn = 0;
    int                  jr = 0, jct = 0;
    const wfm_segment_t *js = dp_wfm_compose_segments (jc, &jn, &jr, &jct);
    DP_REQUIRE_MSG (js && jn == 1 && js[0].n_sources == 1,
                    "one segment, one source");
    const wfm_source_t *jsrc = &js[0].sources[0];
    DP_CHECK_MSG (js[0].num_samples == dg.num_samples,
                  "omitted num_samples is the manifest default, not 0 (an "
                  "empty segment)");
    DP_CHECK_MSG (jsrc->seed == ds.seed, "omitted seed is the default");
    DP_CHECK_MSG (jsrc->sps == ds.sps, "omitted sps is the default");
    DP_CHECK_MSG (jsrc->pn_length == ds.pn_length,
                  "omitted pn_length is the default");
    DP_CHECK_MSG (jsrc->snr == ds.snr && jsrc->acq_reps == ds.acq_reps
                      && jsrc->rrc_beta == ds.rrc_beta
                      && jsrc->rrc_span == ds.rrc_span
                      && jsrc->modulation == ds.modulation,
                  "every other omitted default matches the manifest too");
    dp_wfm_compose_destroy (jc);
  }

  if (test_the_create_snr_seam ())
    return 1;
  if (test_the_two_faces_agree ())
    return 1;
  if (test_a_source_carries_the_frame_a_caller_built ())
    return 1;
  if (test_a_carried_frame_survives_the_scene_json ())
    return 1;
  if (test_a_framed_pn_type_sends_its_frame ())
    return 1;
  if (test_frame_from_json_directly ())
    return 1;

  printf (
      "test_wfm_compose: OK (total=%zu, json round-trip, level, sum, "
      "resolve, sum-json, headroom, seed_advance, ranged fields, "
      "dsss burst, unspread frame, repeats, doppler block-invariance, "
      "persist-through-gap, ranged doppler, doppler json, doppler draws)\n",
      total);
  /* Reports the DP_CHECK accumulator. Until doppler#1592's pins this file
     asserted only through DP_REQUIRE, so a bare `return 0` was latent; the
     first accumulating check would have been decoration without this. */
  /* ── a record's BYTES are pinned ─────────────────────────────────────
   *
   * The surface rows write a source's and a segment's fields in table order
   * (#853 item 9 changed that order once, by decision). Nothing else pins
   * the bytes: the flag-matrix golden compares parsed JSON, and the replay
   * tests compare a record with its own replay, both blind to order. A
   * stored scene re-recorded should diff clean, so the exact text is held
   * here, over each JSON policy a row can carry: the inline and the sum
   * form, a ranged field (its span), a field written only for one type
   * (f_end/span on a chirp, modulation on bits), the pulse=rrc pair,
   * omitted-at-default (level, doppler, repeats, gap_noise), a bool-free
   * spec. Regenerating this after a deliberate change is one paste. */
  {
    wfm_source_t chirp          = WFM_SOURCE_DEFAULTS;
    chirp.type                  = WFM_SYNTH_CHIRP;
    chirp.freq                  = 0.05;
    chirp.freq_hi               = 0.1;
    chirp.ranged                = WFM_RANGE_FREQ;
    chirp.f_end                 = 0.2;
    chirp.span                  = 256;
    chirp.level                 = -3.0;
    chirp.snr                   = 20.0;
    static const uint8_t pat[8] = { 1, 0, 1, 1, 0, 0, 1, 0 };
    wfm_source_t         bits   = WFM_SOURCE_DEFAULTS;
    bits.type                   = WFM_SYNTH_BITS;
    bits.modulation             = 2;
    bits.sps                    = 4;
    bits.pulse                  = 1;
    bits.payload.bits           = pat;
    bits.payload.len            = 8;
    wfm_source_t tone           = WFM_SOURCE_DEFAULTS;
    tone.freq                   = 0.125;
    tone.doppler                = 2.5;
    tone.carrier_hz             = 2.2e9;
    wfm_source_t  pair[2]       = { bits, tone };
    wfm_segment_t segs[2]    = { WFM_SEGMENT_DEFAULTS, WFM_SEGMENT_DEFAULTS };
    segs[0].sources          = &chirp;
    segs[0].n_sources        = 1;
    segs[0].num_samples      = 512;
    segs[1].sources          = pair;
    segs[1].n_sources        = 2;
    segs[1].repeats          = 3;
    segs[1].gap_noise        = 1;
    segs[1].off_samples      = 64;
    static const char want[] = "{\n"
                               "\t\"version\":\t1,\n"
                               "\t\"repeat\":\tfalse,\n"
                               "\t\"continuous\":\tfalse,\n"
                               "\t\"segments\":\t[{\n"
                               "\t\t\t\"fs\":\t1,\n"
                               "\t\t\t\"num_samples\":\t512,\n"
                               "\t\t\t\"off_samples\":\t0,\n"
                               "\t\t\t\"type\":\t\"chirp\",\n"
                               "\t\t\t\"freq\":\t[0.05, 0.1],\n"
                               "\t\t\t\"snr\":\t20,\n"
                               "\t\t\t\"snr_mode\":\t\"auto\",\n"
                               "\t\t\t\"seed\":\t0,\n"
                               "\t\t\t\"sps\":\t1,\n"
                               "\t\t\t\"pn_length\":\t15,\n"
                               "\t\t\t\"pn_poly\":\t0,\n"
                               "\t\t\t\"lfsr\":\t\"galois\",\n"
                               "\t\t\t\"level\":\t-3,\n"
                               "\t\t\t\"f_end\":\t0.2,\n"
                               "\t\t\t\"span\":\t256\n"
                               "\t\t}, {\n"
                               "\t\t\t\"fs\":\t1,\n"
                               "\t\t\t\"num_samples\":\t1024,\n"
                               "\t\t\t\"off_samples\":\t64,\n"
                               "\t\t\t\"repeats\":\t3,\n"
                               "\t\t\t\"gap_noise\":\t\"off\",\n"
                               "\t\t\t\"sum\":\t[{\n"
                               "\t\t\t\t\t\"type\":\t\"bits\",\n"
                               "\t\t\t\t\t\"freq\":\t0,\n"
                               "\t\t\t\t\t\"snr\":\t100,\n"
                               "\t\t\t\t\t\"snr_mode\":\t\"auto\",\n"
                               "\t\t\t\t\t\"seed\":\t0,\n"
                               "\t\t\t\t\t\"sps\":\t4,\n"
                               "\t\t\t\t\t\"pn_length\":\t15,\n"
                               "\t\t\t\t\t\"pn_poly\":\t0,\n"
                               "\t\t\t\t\t\"lfsr\":\t\"galois\",\n"
                               "\t\t\t\t\t\"payload\":\t\"0xb2\",\n"
                               "\t\t\t\t\t\"modulation\":\t\"qpsk\",\n"
                               "\t\t\t\t\t\"pulse\":\t\"rrc\",\n"
                               "\t\t\t\t\t\"rrc_beta\":\t0.35,\n"
                               "\t\t\t\t\t\"rrc_span\":\t8\n"
                               "\t\t\t\t}, {\n"
                               "\t\t\t\t\t\"type\":\t\"tone\",\n"
                               "\t\t\t\t\t\"freq\":\t0.125,\n"
                               "\t\t\t\t\t\"snr\":\t100,\n"
                               "\t\t\t\t\t\"snr_mode\":\t\"auto\",\n"
                               "\t\t\t\t\t\"seed\":\t0,\n"
                               "\t\t\t\t\t\"sps\":\t1,\n"
                               "\t\t\t\t\t\"pn_length\":\t15,\n"
                               "\t\t\t\t\t\"pn_poly\":\t0,\n"
                               "\t\t\t\t\t\"lfsr\":\t\"galois\",\n"
                               "\t\t\t\t\t\"doppler\":\t2.5,\n"
                               "\t\t\t\t\t\"carrier_hz\":\t2200000000\n"
                               "\t\t\t\t}]\n"
                               "\t\t}]\n"
                               "}";
    char             *got    = dp_wfm_spec_to_json (segs, 2, 0, 0, 0, 0.0);
    DP_REQUIRE (got != NULL);
    DP_CHECK_MSG (strcmp (got, want) == 0,
                  "a record's bytes: keys in table order, each JSON policy");
    if (strcmp (got, want) != 0)
      (void)fprintf (stderr, "got:\n%s\n", got);
    free (got);
  }

  /* ── the file readers: from_file and its reason-naming twin ───────────
   *
   * Python reaches only dp_wfm_compose_from_file_why (just-makeit#1706), so
   * the plain reader is pinned here: it must read a valid scene, refuse a
   * missing path, and refuse a retired key -- and the _why twin must name
   * that key, as dp_wfm_compose_from_json_why does for text. */
  {
    static const char ok[]
        = "{\"version\": 1, \"segments\": [{\"type\": \"tone\", "
          "\"num_samples\": 64}]}";
    static const char retired[]
        = "{\"version\": 1, \"segments\": [{\"type\": \"tone\", "
          "\"num_samples\": 64, \"sync_gen\": \"pn:63:6\"}]}";
    char ok_path[512], bad_path[512], missing[512];
    int  pid = (int)getpid ();
    snprintf (ok_path, sizeof ok_path, "%s/dp_wfm_from_file_ok_%d.json",
              dp_test_tmpdir (), pid);
    snprintf (bad_path, sizeof bad_path, "%s/dp_wfm_from_file_retired_%d.json",
              dp_test_tmpdir (), pid);
    snprintf (missing, sizeof missing, "%s/dp_wfm_from_file_absent_%d.json",
              dp_test_tmpdir (), pid);

    /* the text itself: valid and refused, before any file is involved */
    dp_wfm_compose_state_t *t = dp_wfm_compose_from_json (ok);
    DP_REQUIRE_MSG (t, "the valid scene parses as text");
    dp_wfm_compose_destroy (t);

    FILE *f = fopen (ok_path, "wb");
    DP_REQUIRE_MSG (f, "write the valid scene file");
    DP_REQUIRE (fwrite (ok, 1, sizeof ok - 1, f) == sizeof ok - 1);
    fclose (f);
    f = fopen (bad_path, "wb");
    DP_REQUIRE_MSG (f, "write the retired-key scene file");
    DP_REQUIRE (fwrite (retired, 1, sizeof retired - 1, f)
                == sizeof retired - 1);
    fclose (f);

    dp_wfm_compose_state_t *c = dp_wfm_compose_from_file (ok_path);
    DP_CHECK_MSG (c != NULL, "from_file reads a valid scene");
    dp_wfm_compose_destroy (c);
    DP_CHECK_MSG (dp_wfm_compose_from_file (missing) == NULL,
                  "from_file refuses a missing path");
    DP_CHECK_MSG (dp_wfm_compose_from_file (bad_path) == NULL,
                  "from_file refuses a retired key");

    const char *why = NULL;
    c               = dp_wfm_compose_from_file_why (bad_path, &why);
    DP_CHECK_MSG (c == NULL, "from_file_why refuses a retired key");
    DP_CHECK_MSG (why && strstr (why, "\"sync_gen\""),
                  "and its reason names the key");
    why = NULL;
    c   = dp_wfm_compose_from_file_why (missing, &why);
    DP_CHECK_MSG (c == NULL && why == NULL,
                  "an unreadable path is refused with no reason");
    why = NULL;
    c   = dp_wfm_compose_from_file_why (ok_path, &why);
    DP_CHECK_MSG (c != NULL, "from_file_why reads a valid scene");
    dp_wfm_compose_destroy (c);

    DP_CHECK_MSG (remove (ok_path) == 0, "remove the valid scene file");
    DP_CHECK_MSG (remove (bad_path) == 0, "remove the retired scene file");
  }

  DP_TEST_END ("test_wfm_compose");
}
