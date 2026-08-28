/**
 * @file test_dsss_burst_receiver_core.c
 * @brief DsssBurstReceiver — lifecycle and argument-validation tests.
 *
 * Phase 4 of the lifecycle spine covers `push()`; this file covers what
 * phase 2/3 built, and deliberately does NOT pretend to cover more. The
 * three stages are unimplemented (see `dsss_burst_receiver_push`), so the
 * only honest claims here are about construction, refusal, derived
 * geometry and reset.
 *
 * Converted off jm's scaffold harness: doppler does not adopt `jm_test.h`
 * (a second assertion harness beside `dp_test.h` is the duplication this
 * repo forbids -- see just-makeit.toml's status_allow note and gh-730), so
 * a scaffolded test is converted to DP_CHECK/DP_REQUIRE on arrival.
 */
#include "dsss_burst_receiver/dsss_burst_receiver_core.h"

#include "pn/pn_core.h"

#include "dp_rng_test.h"
#include "dp_test.h"

#define ACQ_SF 31u
#define DATA_SF 8u
#define REPS 4u
#define SPC 4u
#define PAYLOAD 32u
/* What push() returns per burst: the frame as received. */
#define FRAME_SYMS (SYNC_LEN + PAYLOAD + 16u)
#define SYNC_LEN 13u

/* ── Burst synthesis, the same shape test_burst_demod_core.c uses ─────────
 * Kept local rather than shared: these tests want a burst placed at an
 * arbitrary stream offset inside a noise floor, which is the receiver's
 * problem and not the demodulator's. */

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

/* A REAL spreading code: the maximal-length sequence of a 5-stage LFSR,
 * built with the library's own generator rather than an arithmetic pattern.
 *
 * This is not fussiness. The pattern previously used here --
 * `(i * 2654435761) >> 13 & 1` -- reads like a reasonable deterministic code
 * and has a peak-to-worst-sidelobe ratio of **1.07**: it barely spreads at
 * all. The characterization measured what that costs (53% of burst offsets
 * detected, against 100% for a real code), because the CFAR reference is
 * then set by the code's own autocorrelation rather than by noise. An MLS of
 * the same length has a ratio of 31 -- the ideal -1 sidelobe. Testing the
 * receiver on a code no caller would use measures the wrong thing. */
static const uint8_t *
acq_code (void)
{
  static uint8_t c[ACQ_SF];
  static int     built = 0;
  if (!built)
    {
      pn_state_t *pn = pn_create (pn_mls_poly (5), 1u, 5u, 0);
      for (size_t i = 0; i < ACQ_SF; i++)
        c[i] = pn_step (pn);
      pn_destroy (pn);
      built = 1;
    }
  return c;
}

static const uint8_t *
data_code (void)
{
  static uint8_t c[DATA_SF];
  for (size_t i = 0; i < DATA_SF; i++)
    c[i] = (uint8_t)((i * 40503u >> 7) & 1u);
  return c;
}

static const uint8_t *
sync_word (void)
{
  static uint8_t s[SYNC_LEN] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0 };
  return s;
}

/* The caller's half of the split: a frame's bits in, a verdict out.
 *
 * This object stops at decisions, so the CRC is computed HERE now — the
 * same arithmetic that used to be `frame_valid`, at the layer that owns it
 * (doppler#1022). `wfm.Frame.deframe()` is the shipped form of this; a
 * three-line local copy keeps this test linked against burst objects only.
 */
static int
frame_ok (const uint8_t *frame)
{
  uint16_t rx = 0;
  for (size_t j = 0; j < 16u; j++)
    rx = (uint16_t)((rx << 1) | (frame[SYNC_LEN + PAYLOAD + j] & 1u));
  return rx == crc16 (frame + SYNC_LEN, PAYLOAD);
}

static const uint8_t *
payload_bits (void)
{
  static uint8_t p[PAYLOAD];
  for (size_t i = 0; i < PAYLOAD; i++)
    p[i] = (uint8_t)((i * 7u + 3u) & 1u);
  return p;
}

static size_t
put_symbol (float complex *y, size_t n, const uint8_t *dcode, uint8_t bit)
{
  float a = csign (bit);
  for (size_t c = 0; c < DATA_SF; c++)
    for (size_t k = 0; k < SPC; k++)
      y[n++] = a * csign (dcode[c]);
  return n;
}

/** @brief One burst: preamble, sync, payload, CRC-16, on carrier @p f0. */
static size_t
build_burst (float complex *y, double f0)
{
  const uint8_t *acode = acq_code (), *dcode = data_code ();
  const uint8_t *sy = sync_word (), *pl = payload_bits ();
  size_t         n = 0;
  for (size_t r = 0; r < REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++)
        y[n++] = csign (acode[c]);
  for (size_t j = 0; j < SYNC_LEN; j++)
    n = put_symbol (y, n, dcode, sy[j]);
  for (size_t j = 0; j < PAYLOAD; j++)
    n = put_symbol (y, n, dcode, pl[j]);
  uint16_t crc = crc16 (pl, PAYLOAD);
  for (size_t j = 0; j < 16u; j++)
    n = put_symbol (y, n, dcode, (uint8_t)((crc >> (15u - j)) & 1u));
  for (size_t i = 0; i < n; i++)
    {
      double ph = 2.0 * M_PI * f0 * (double)i;
      y[i] *= (float)cos (ph) + (float)sin (ph) * I;
    }
  return n;
}

/**
 * @brief A capture with SEVERAL bursts, at @p n_at given offsets.
 *
 * The gate this suite never had. Every other helper here places exactly ONE
 * burst, which is why three separate input-discarding defects survived
 * certification: with a single burst, everything push() threw away was
 * noise, so nothing observable was lost (doppler#1008).
 */
static void
build_capture_multi (float complex *cap, size_t n_cap, const size_t *at,
                     size_t n_at, double sigma, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < n_cap; i++)
    {
      float re = (float)(sigma * dp_gauss (&st));
      float im = (float)(sigma * dp_gauss (&st));
      cap[i]   = re + im * I;
    }
  static float complex burst[1 << 16];
  size_t               nb = build_burst (burst, 0.0);
  for (size_t k = 0; k < n_at; k++)
    for (size_t i = 0; i < nb && at[k] + i < n_cap; i++)
      cap[at[k] + i] += burst[i];
}

/** @brief A capture: noise everywhere, one burst starting at @p at. */
static void
build_capture (float complex *cap, size_t n_cap, size_t at, double f0,
               double sigma, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < n_cap; i++)
    {
      /* Named locals: two dp_gauss() calls in one expression would be
         ordered by the compiler, and gcc and clang differ. */
      float re = (float)(sigma * dp_gauss (&st));
      float im = (float)(sigma * dp_gauss (&st));
      cap[i]   = re + im * I;
    }
  static float complex burst[1 << 16];
  size_t               nb = build_burst (burst, f0);
  for (size_t i = 0; i < nb && at + i < n_cap; i++)
    cap[at + i] += burst[i];
}

static dsss_burst_receiver_state_t *
make_rx (void)
{
  return dsss_burst_receiver_create (
      acq_code (), ACQ_SF, data_code (), DATA_SF, sync_word (), SYNC_LEN, REPS,
      SPC, 1.0e6, FRAME_SYMS, 55.0, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
}

static dsss_burst_receiver_state_t *
make (void)
{
  uint8_t acq[ACQ_SF], data[DATA_SF], sync[SYNC_LEN];
  for (size_t i = 0; i < ACQ_SF; i++)
    acq[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < DATA_SF; i++)
    data[i] = (uint8_t)((i >> 1) & 1u);
  for (size_t i = 0; i < SYNC_LEN; i++)
    sync[i] = (uint8_t)((i * 3u) & 1u);
  return dsss_burst_receiver_create (acq, ACQ_SF, data, DATA_SF, sync,
                                     SYNC_LEN, REPS, SPC, 1.0e6, FRAME_SYMS,
                                     50.0, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
}

/* A valid parameter set builds, and the codes are COPIED -- the caller's
 * arrays here are stack locals inside make(), already out of scope by the
 * time this reads back, so a borrowing constructor would show up as garbage
 * geometry rather than as a clean pointer. */
static int
test_create_copies_and_derives (void)
{
  dsss_burst_receiver_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  DP_CHECK (s->acq_code_len == ACQ_SF);
  DP_CHECK (s->data_code_len == DATA_SF);
  DP_CHECK (s->sync_len == SYNC_LEN);
  DP_CHECK (s->acq_code[1] == 1u); /* the copy survived its source */

  /* One preamble repetition, in samples -- the modulus every epoch
   * ambiguity in this object is stated against. */
  DP_CHECK (s->code_period == ACQ_SF * SPC);

  /* preamble + spread (sync | payload | CRC-16) frame. */
  DP_CHECK (s->burst_len
            == (REPS * ACQ_SF + (SYNC_LEN + PAYLOAD + 16u) * DATA_SF) * SPC);

  /* The look-back ring holds detection lag + refine span + the burst
   * itself, rounded to a power of two (§7.1). Checked as ">= the span"
   * rather than "== a literal": pinning the rounded number would pin the
   * rounding, and the CONTRACT is that the span fits. */
  DP_REQUIRE (s->hist != NULL);
  DP_CHECK (s->hist->capacity >= 2u * REPS * s->code_period + s->burst_len);
  DP_CHECK ((s->hist->capacity & (s->hist->capacity - 1u)) == 0u);

  DP_CHECK (s->acq != NULL);
  DP_CHECK (s->demod != NULL);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* Every rejected parameter set is an ARGUMENT error the manifest turns into
 * a ValueError naming the constraint. Each case varies ONE field from the
 * valid set above, so a NULL cannot be attributed to the wrong guard. */
static int
test_refuses_bad_arguments (void)
{
  uint8_t code[ACQ_SF], sync[SYNC_LEN];
  for (size_t i = 0; i < ACQ_SF; i++)
    code[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < SYNC_LEN; i++)
    sync[i] = 0u;

#define MK(ac, acl, dc, dcl, sy, syl, rp, sp, cr, pl, cn, du, fa, pdv)        \
  dsss_burst_receiver_create ((ac), (acl), (dc), (dcl), (sy), (syl), (rp),    \
                              (sp), (cr), (pl), (cn), (du), (fa), (pdv), 0.0, \
                              0.0, 10)

  DP_CHECK (MK (NULL, 0, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* no preamble code */
  DP_CHECK (MK (code, ACQ_SF, NULL, 0, sync, SYNC_LEN, REPS, SPC, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* no data code */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, NULL, 0, REPS, SPC, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* no sync word */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, 0, SPC, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* reps < 1 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, 0, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* spc < 1 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 0.0,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6, 0,
                50.0, 0.0, 1e-3, 0.9)
            == NULL); /* payload_len < 1 */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1.0, 0.9)
            == NULL); /* pfa out of (0,1) */
  DP_CHECK (MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
                FRAME_SYMS, 50.0, 0.0, 1e-3, 0.0)
            == NULL); /* pd out of (0,1) */

  /* The control: the same call with every field valid DOES build, so the
   * nine NULLs above are attributable to the field each varied and not to
   * a constructor that refuses everything. */
  dsss_burst_receiver_state_t *ok
      = MK (code, ACQ_SF, code, DATA_SF, sync, SYNC_LEN, REPS, SPC, 1e6,
            FRAME_SYMS, 50.0, 0.0, 1e-3, 0.9);
  DP_CHECK (ok != NULL);
  dsss_burst_receiver_destroy (ok);
#undef MK
  return 0;
}

/* THE claim the object exists to make: samples in, a decoded burst out,
 * with the preamble start recovered from the stream alone. Pushed in small
 * blocks, so the burst spans many calls and the look-back is doing real
 * work rather than the whole capture happening to be resident. */
static int
test_decodes_a_burst_from_a_stream (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);

  const size_t         AT    = 5000;
  const size_t         N_CAP = 40000;
  static float complex cap[40000];
  build_capture (cap, N_CAP, AT, 0.0, 0.02, 12345u);

  uint8_t out[FRAME_SYMS];
  size_t  got = 0;
  for (size_t off = 0; off < N_CAP && got == 0; off += 777)
    {
      size_t n = (off + 777 <= N_CAP) ? 777 : (N_CAP - off);
      got      = dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS);
    }

  DP_CHECK (got == FRAME_SYMS);
  DP_CHECK (frame_ok (out));
  DP_CHECK (dsss_burst_receiver_get_n_bursts (s) == 1);

  /* The field a caller cannot compute: recovered to the SAMPLE, from an
     acquisition that reports only an end anchor and a residue. */
  DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == AT);
  DP_CHECK_MSG (frame_ok (out),
                "the frame it handed back checks out against its own "
                "trailer — computed here, because this object stops at "
                "decisions (doppler#1022)");

  /* The payload sits inside the frame push() returned: sync word, then
     payload, then the trailer. Slicing it is the caller's arithmetic --
     this object hands back decisions, not fields (doppler#1022). */
  const uint8_t *want = payload_bits ();
  size_t         errs = 0;
  for (size_t i = 0; i < PAYLOAD; i++)
    errs += (out[SYNC_LEN + i] != want[i]);
  DP_CHECK (errs == 0);

  /* Refine's own confidence: the nearest whole-period competitor should sit
     near (reps-1)/reps = 0.75, well clear of 1. A margin at 1 means the
     period was not resolved -- the failure nothing else in the chain sees. */
  DP_CHECK (dsss_burst_receiver_get_refine_margin (s) < 0.9);
  DP_CHECK (dsss_burst_receiver_get_refine_margin (s) > 0.0);

  DP_CHECK (dsss_burst_receiver_get_dropped (s) == 0);
  dsss_burst_receiver_destroy (s);
  return 0;
}

/* The same burst under a residual Doppler the acquisition grid does not
 * resolve away. This is the case the obvious refine cannot survive: a
 * coherent whole-preamble correlation walks off by whole code periods once a
 * fraction of a bin is present, while the per-period non-coherent combine is
 * exact. Without this the implementation would be "verified" on the one
 * input that cannot fail.
 *
 * Exactly half a bin is deliberately NOT among the offsets. That is the
 * slow-time FFT's scalloping null -- acquisition returns NO hit there at any
 * SNR, because test_stat saturates against the code's own sidelobe floor so
 * raising the signal cannot buy the ~3.9 dB back. It is an acquisition
 * property, not a refine one (gh-1002), and asserting it here would be this
 * object failing a test for its dependency's grid. */
static int
test_decodes_under_residual_doppler (void)
{
  dsss_burst_receiver_state_t *probe = make_rx ();
  DP_REQUIRE (probe != NULL);
  double res_hz = probe->acq->engine->doppler_res_hz;
  double fs     = 1.0e6 * (double)SPC;
  dsss_burst_receiver_destroy (probe);

  const size_t AT      = 5000;
  const size_t N_CAP   = 40000;
  const double frac[3] = { 0.25, 0.75, 1.0 };

  for (int q = 0; q < 3; q++)
    {
      double               f_hz = frac[q] * res_hz;
      static float complex cap[40000];
      build_capture (cap, N_CAP, AT, f_hz / fs, 0.02, 999u);

      dsss_burst_receiver_state_t *s = make_rx ();
      DP_REQUIRE (s != NULL);
      uint8_t out[FRAME_SYMS];
      size_t  got = 0;
      for (size_t off = 0; off < N_CAP && got == 0; off += 777)
        {
          size_t n = (off + 777 <= N_CAP) ? 777 : (N_CAP - off);
          got = dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS);
        }
      DP_CHECK (got == FRAME_SYMS);
      DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == AT);
      DP_CHECK (frame_ok (out));
      DP_CHECK (dsss_burst_receiver_get_refine_margin (s) < 0.9);
      dsss_burst_receiver_destroy (s);
    }
  return 0;
}

/* One TRUE burst is reported once, false alarms are marked, and
 * refine_margin agrees with the CRC on which is which.
 *
 * Three claims in one capture because they are one property. Acquisition
 * fires on every frame lying inside the preamble, so without suppression a
 * single burst is claimed `reps` times over. It also false-alarms in noise at
 * a finite rate, and a false alarm legitimately RETURNS bits -- the caller's
 * verdict is the frame's own CRC, not the fact that something came back. An
 * earlier version asserted "exactly one return" and failed the moment the
 * acquisition code was good enough to reach its designed false-alarm rate.
 *
 * The margin claim is the one that matters and it is the object's whole
 * reason for existing: a mis-windowed burst still has a carrier, so a
 * lock-style indicator reads healthy through a broken hand-off. Here the
 * true burst resolves its period (margin well below 1) and every false alarm
 * does not (margin near 1) -- the two agree with the CRC without being told
 * about it.
 *
 * The noise is deterministic (dp_gauss from a fixed seed), so the false
 * alarm below is reproducible rather than hoped for; the count is asserted
 * so this cannot quietly become vacuous if the realization changes. */
static int
test_true_burst_once_and_false_alarms_marked (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);

  const size_t         AT = 5000;
  static float complex cap[40000];
  build_capture (cap, 40000, AT, 0.0, 0.02, 12345u);

  uint8_t out[FRAME_SYMS];
  size_t  n_valid = 0, n_false = 0;
  double  valid_margin = 1.0, worst_false_margin = 0.0;

  for (size_t off = 0; off < 40000; off += 777)
    {
      size_t n = (off + 777 <= 40000) ? 777 : (40000 - off);
      if (!dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS))
        continue;
      double m = dsss_burst_receiver_get_refine_margin (s);
      if (frame_ok (out))
        {
          n_valid++;
          valid_margin = m;
          DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == AT);
        }
      else
        {
          n_false++;
          if (m > worst_false_margin)
            worst_false_margin = m;
        }
    }

  /* The real burst, claimed exactly once despite firing on every frame of
     its own preamble. */
  DP_CHECK (n_valid == 1);
  DP_CHECK (dsss_burst_receiver_get_n_bursts (s) >= 1);

  /* Non-vacuous: this capture really does contain a false alarm, so the
     comparison below is measuring something. */
  DP_CHECK_MSG (n_false >= 1,
                "no false alarm in this capture -- the margin comparison "
                "below would be vacuous");

  /* The margin tells them apart, unprompted. */
  DP_CHECK (valid_margin < 0.9);
  DP_CHECK (worst_false_margin > valid_margin);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* Silence decodes nothing, and says so cheaply. The control for the two
 * above: a receiver that reported a burst for anything would pass them. */
static int
test_silence_yields_no_burst (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);

  static float complex cap[20000];
  uint32_t             st = 7u;
  for (size_t i = 0; i < 20000; i++)
    {
      float re = (float)(0.02 * dp_gauss (&st));
      float im = (float)(0.02 * dp_gauss (&st));
      cap[i]   = re + im * I;
    }

  uint8_t out[FRAME_SYMS];
  size_t  total = 0;
  for (size_t off = 0; off < 20000; off += 1024)
    {
      size_t n = (off + 1024 <= 20000) ? 1024 : (20000 - off);
      total += dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS);
    }
  DP_CHECK (total == 0);
  DP_CHECK (dsss_burst_receiver_get_n_bursts (s) == 0);
  /* no burst, so nothing to check */

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* Any block size, including one larger than the ring itself -- the reason
 * push() slices rather than refusing. A single push of the whole capture
 * must find the same burst at the same sample as the 777-sample blocks. */
static int
test_one_giant_push_finds_the_same_burst (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);
  DP_REQUIRE (40000 > s->hist->capacity); /* genuinely larger than the ring */

  const size_t         AT = 5000;
  static float complex cap[40000];
  build_capture (cap, 40000, AT, 0.0, 0.02, 12345u);

  /* push() returns EVERY burst the call completed, so the buffer is sized
     from the input and the burst is located through its EVENT rather than
     through the scalar read-backs -- those describe the LAST burst returned,
     which need not be the real one when a capture also carries a false
     alarm. */
  size_t   cap_bits = dsss_burst_receiver_push_max_out (s, 40000);
  uint8_t *out      = malloc (cap_bits);
  DP_REQUIRE (out != NULL);
  size_t got = dsss_burst_receiver_push (s, cap, 40000, out, cap_bits);
  DP_CHECK (got >= FRAME_SYMS);
  DP_CHECK (got % FRAME_SYMS == 0);

  size_t           n_ev = dsss_burst_receiver_events_max_out (s);
  dsss_br_event_t *evs  = malloc (n_ev * sizeof *evs);
  DP_REQUIRE (evs != NULL);
  DP_CHECK (dsss_burst_receiver_events (s, n_ev, evs, n_ev) == n_ev);
  DP_CHECK (n_ev == got / FRAME_SYMS);

  int found = 0;
  for (size_t i = 0; i < n_ev; i++)
    if (evs[i].preamble_start == AT && frame_ok (out + i * FRAME_SYMS))
      {
        found               = 1;
        const uint8_t *want = payload_bits ();
        const uint8_t *rxf  = out + i * FRAME_SYMS;
        size_t         errs = 0;
        for (size_t k = 0; k < PAYLOAD; k++)
          errs += (rxf[SYNC_LEN + k] != want[k]);
        DP_CHECK (errs == 0);
      }
  DP_CHECK (found);

  /* The whole point: consuming the entire input costs nothing. */
  DP_CHECK (dsss_burst_receiver_get_dropped (s) == 0);

  free (evs);
  free (out);
  dsss_burst_receiver_destroy (s);
  return 0;
}

/* A burst near the START of the stream, where refine cannot back off its
 * full search span.
 *
 * The candidates are `anchor + k*P`, and backing off must therefore move in
 * WHOLE code periods -- clamping to sample 0 instead puts the entire grid on
 * multiples of P and throws away the very phase the anchor carries. Measured
 * before the fix, with a 127-chip code (where the sizer picks a coherent
 * depth of 1, so `k_lo*P` is large): refine returned exactly 11*P while the
 * burst sat 588 samples off it.
 *
 * This is placed early enough that `k_lo * code_period` exceeds the anchor,
 * which is the only condition that reaches the clamp. */
static int
test_a_burst_near_the_stream_start (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);

  /* Inside the clamp's reach: the refine span cannot be taken in full. */
  const size_t AT = 600;
  DP_REQUIRE (AT < s->k_lo * s->code_period);
  /* ...and NOT on a code-period boundary, so a grid that lost the phase
     could not land on the right answer by accident. */
  DP_REQUIRE (AT % s->code_period != 0);

  static float complex cap[40000];
  build_capture (cap, 40000, AT, 0.0, 0.02, 4242u);

  uint8_t out[FRAME_SYMS];
  size_t  got = 0;
  for (size_t off = 0; off < 40000 && got == 0; off += 777)
    {
      size_t n = (off + 777 <= 40000) ? 777 : (40000 - off);
      got      = dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS);
    }
  DP_CHECK (got == FRAME_SYMS);
  DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == AT);
  DP_CHECK (frame_ok (out));

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* One burst, many detections: the suppression path.
 *
 * With the search grid pinned to a single coherent bin the acquisition frame
 * is ONE code period, so `reps` frames fall inside the preamble and every one
 * of them fires. Without suppression the same burst is queued that many times
 * and the receiver reports bursts that do not exist. The auto-sized grid used
 * by the other tests here spends its depth on Doppler instead, making the
 * frame the whole preamble and firing only once -- so this path needs its own
 * stimulus rather than sharing theirs. */
static int
test_one_burst_many_detections_is_claimed_once (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);
  DP_REQUIRE (dsss_burst_receiver_configure_search_raw (s, 1, 1) == 0);
  /* The frame really is one code period now -- otherwise the preamble would
     not span several frames and this test would prove nothing. */
  DP_REQUIRE (s->acq->engine->n == s->code_period);

  const size_t         AT = 5000;
  static float complex cap[40000];
  build_capture (cap, 40000, AT, 0.0, 0.02, 777u);

  uint8_t out[FRAME_SYMS];
  size_t  n_valid = 0;
  for (size_t off = 0; off < 40000; off += 777)
    {
      size_t n = (off + 777 <= 40000) ? 777 : (40000 - off);
      if (!dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS))
        continue;
      if (frame_ok (out))
        {
          n_valid++;
          DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == AT);
        }
    }
  DP_CHECK_MSG (n_valid == 1, "the same burst was decoded more than once");

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* push_max_out() is the payload length whatever the input block size, since
 * at most one burst is returned per call. */
/* A WEAKER spurious detection arriving BEFORE a real burst must not cost the
 * burst (doppler#1004).
 *
 * The old rule armed `suppress_until = epoch + burst_len` on every detection,
 * unconditionally and at detection time, and took the FIRST hit in that
 * window. So any spurious crossing -- noise, or a neighbouring burst's
 * payload firing against the acquisition code -- blinded the search for a
 * whole burst and the next real preamble was discarded before it was ever
 * queued. Measured on the 5-burst example capture: 3 of 5 bursts lost, with
 * the two losses attributable 1:1 to two such crossings, each ~10 dB weaker
 * than the burst it killed.
 *
 * WHY NOTHING HERE CAUGHT IT. This file's geometry has a payload so short
 * that burst_len (2448) is under refine_span (2480) -- the suppression window
 * is narrower than refine's own reach, so it can never extend past the burst
 * it belongs to and can never swallow a distinct one. The defect needs
 * burst_len > refine_span, which is every realistic link (the example's ratio
 * is 5.5x). The decoy below reaches into the window from inside instead, so
 * the same rule is exercised without changing the file's geometry.
 *
 * The decoy is a bare preamble at 0.35 amplitude: strong enough to cross the
 * CFAR gate, too weak to win a greatest-of comparison, and carrying no
 * payload so it cannot pass a CRC. Under the fixed rule it is coalesced into
 * the real burst -- same preamble by refine's reach -- and the stronger hit
 * takes the slot, so the burst decodes at its exact start. Under the old rule
 * the decoy owns the slot and the burst is gone.
 */
static int
test_a_weak_decoy_does_not_cost_the_burst (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);

  const size_t AT    = 5000;
  const size_t GAP   = 1500; /* < burst_len, so the old rule suppresses */
  const size_t N_CAP = 40000;
  DP_REQUIRE (GAP < s->burst_len);

  static float complex cap[40000];
  build_capture (cap, N_CAP, AT, 0.0, 0.02, 12345u);

  /* The decoy: one preamble only, no payload, at a third the amplitude. */
  const uint8_t *acode = acq_code ();
  size_t         d     = AT - GAP;
  for (size_t r = 0; r < REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++, d++)
        cap[d] += 0.35f * csign (acode[c]);

  uint8_t out[FRAME_SYMS];
  size_t  got = 0;
  for (size_t off = 0; off < N_CAP && got == 0; off += 777)
    {
      size_t n = (off + 777 <= N_CAP) ? 777 : (N_CAP - off);
      got      = dsss_burst_receiver_push (s, cap + off, n, out, FRAME_SYMS);
    }

  /* The burst survives the decoy, exactly: same start, same bits, valid. */
  DP_CHECK (got == FRAME_SYMS);
  DP_CHECK (frame_ok (out));
  DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == AT);

  /* The payload sits inside the frame push() returned: sync word, then
     payload, then the trailer. Slicing it is the caller's arithmetic --
     this object hands back decisions, not fields (doppler#1022). */
  const uint8_t *want = payload_bits ();
  size_t         errs = 0;
  for (size_t i = 0; i < PAYLOAD; i++)
    errs += (out[SYNC_LEN + i] != want[i]);
  DP_CHECK (errs == 0);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* THE REGRESSION TEST FOR doppler#1008: the same capture, decoded
 * identically at every block size.
 *
 * push() used to abandon the rest of its input once a burst was ready, so a
 * block carrying several bursts lost all but the first -- measured on the
 * example capture at 6/6 decoded with 333-sample blocks against 1/6 with one
 * large one. The block size is the caller's choice and must not change what
 * is decoded, so this asserts the SET of recovered bursts is identical
 * across block sizes, including a single push of the whole capture.
 *
 * Gaps are generous on purpose: this test is about block size, and pinning
 * it to a marginal burst separation would make it fail for a second reason.
 */
static int
test_every_burst_survives_any_block_size (void)
{
  const size_t         N_CAP = 40000;
  const size_t         GAP   = 3000;
  static float complex cap[40000];

  size_t at[3];
  {
    dsss_burst_receiver_state_t *g = make_rx ();
    DP_REQUIRE (g != NULL);
    at[0] = 5000;
    at[1] = at[0] + g->burst_len + GAP;
    at[2] = at[1] + g->burst_len + GAP;
    DP_REQUIRE (at[2] + g->burst_len < N_CAP);
    dsss_burst_receiver_destroy (g);
  }
  build_capture_multi (cap, N_CAP, at, 3, 0.02, 4242u);

  const size_t blocks[] = { 333, 777, 4096, N_CAP };
  for (size_t b = 0; b < sizeof blocks / sizeof blocks[0]; b++)
    {
      dsss_burst_receiver_state_t *s = make_rx ();
      DP_REQUIRE (s != NULL);
      size_t   cap_bits = dsss_burst_receiver_push_max_out (s, N_CAP);
      uint8_t *out      = malloc (cap_bits);
      DP_REQUIRE (out != NULL);

      int found[3] = { 0, 0, 0 };
      for (size_t off = 0; off < N_CAP; off += blocks[b])
        {
          size_t n = (off + blocks[b] <= N_CAP) ? blocks[b] : (N_CAP - off);
          size_t got
              = dsss_burst_receiver_push (s, cap + off, n, out, cap_bits);
          size_t n_ev = dsss_burst_receiver_events_max_out (s);
          DP_CHECK (n_ev == got / FRAME_SYMS);
          if (!n_ev)
            continue;

          dsss_br_event_t *evs = malloc (n_ev * sizeof *evs);
          DP_REQUIRE (evs != NULL);
          DP_CHECK (dsss_burst_receiver_events (s, n_ev, evs, n_ev) == n_ev);
          const uint8_t *want = payload_bits ();
          for (size_t i = 0; i < n_ev; i++)
            for (size_t k = 0; k < 3; k++)
              if (evs[i].preamble_start == at[k]
                  && frame_ok (out + i * FRAME_SYMS))
                {
                  size_t errs = 0;
                  for (size_t j = 0; j < PAYLOAD; j++)
                    errs += (out[i * FRAME_SYMS + SYNC_LEN + j] != want[j]);
                  DP_CHECK (errs == 0);
                  found[k] = 1;
                }
          free (evs);
        }

      /* Every burst, at every block size. Losing one here is doppler#1008. */
      DP_CHECK (found[0] && found[1] && found[2]);
      /* And consuming the whole input costs nothing. */
      DP_CHECK (dsss_burst_receiver_get_dropped (s) == 0);

      free (out);
      dsss_burst_receiver_destroy (s);
    }
  return 0;
}

/* acq_push() stops once it has filled the caller's result array and abandons
 * the rest of its input (acq_core.c:925). push() slices at chunk_max, so a
 * chunk can easily carry more than DSSS_BR_HITS dumps -- and a single
 * burst_acq_push() per chunk then leaves acq un-fed over samples this object
 * is holding, losing detections rather than samples.
 *
 * Pinning the grid to one coherent bin makes the acquisition frame ONE code
 * period, so a chunk spans ~92 frames and saturation is certain.
 */
static int
test_acq_saturation_does_not_lose_bursts (void)
{
  const size_t         N_CAP = 40000;
  const size_t         GAP   = 3000;
  static float complex cap[40000];

  /* A DELIBERATELY loose false-alarm rate, so noise crosses the gate often
     enough that one batch of DSSS_BR_HITS cannot hold a chunk's detections.
     Saturation is the condition under test; leaving it to chance would make
     this test pass for the wrong reason. */
  dsss_burst_receiver_state_t *s = dsss_burst_receiver_create (
      acq_code (), ACQ_SF, data_code (), DATA_SF, sync_word (), SYNC_LEN, REPS,
      SPC, 1.0e6, FRAME_SYMS, 55.0, 0.0, 0.2, 0.9, 0.0, 0.0, 10);
  DP_REQUIRE (s != NULL);
  DP_REQUIRE (dsss_burst_receiver_configure_search_raw (s, 1, 1) == DP_OK);

  size_t at[3] = { 5000, 0, 0 };
  at[1]        = at[0] + s->burst_len + GAP;
  at[2]        = at[1] + s->burst_len + GAP;
  DP_REQUIRE (at[2] + s->burst_len < N_CAP);
  /* The premise: a chunk spans far more frames than one batch of hits. */
  DP_REQUIRE (s->chunk_max / s->code_period > DSSS_BR_HITS);
  build_capture_multi (cap, N_CAP, at, 3, 0.02, 909u);

  size_t   cap_bits = dsss_burst_receiver_push_max_out (s, N_CAP);
  uint8_t *out      = malloc (cap_bits);
  DP_REQUIRE (out != NULL);
  size_t got = dsss_burst_receiver_push (s, cap, N_CAP, out, cap_bits);

  size_t           n_ev = dsss_burst_receiver_events_max_out (s);
  dsss_br_event_t *evs  = malloc ((n_ev ? n_ev : 1) * sizeof *evs);
  DP_REQUIRE (evs != NULL);
  DP_CHECK (dsss_burst_receiver_events (s, n_ev, evs, n_ev) == n_ev);
  DP_CHECK (n_ev == got / FRAME_SYMS);

  int found = 0;
  for (size_t i = 0; i < n_ev; i++)
    for (size_t k = 0; k < 3; k++)
      if (evs[i].preamble_start == at[k] && frame_ok (out + i * FRAME_SYMS))
        found++;
  DP_CHECK (found == 3);
  DP_CHECK (dsss_burst_receiver_get_dropped (s) == 0);

  free (evs);
  free (out);
  dsss_burst_receiver_destroy (s);
  return 0;
}

/* A burst SPLIT across two push() calls comes out of the second one, whole,
 * and `pending` says so in between.
 *
 * This is the streaming contract, and nothing exercised it: every other test
 * here hands push() a burst that is already complete. A refactor that merged
 * the `pending` read-back with the queue length passed all 152 tests in this
 * tree while silently discarding any burst that straddled a call boundary --
 * because push() zeroed the old per-call counter on entry, which after the
 * merge zeroed the queue itself. The suite could not see it.
 *
 * Two things are asserted, and both matter. The bits must survive the split
 * (the behaviour), and `pending` must be non-zero between the calls (the
 * read-back a caller needs at end-of-stream: closing a file while it is set
 * discards a burst that would have decoded, and no other field distinguishes
 * that from an empty capture). */
static int
test_a_burst_split_across_two_pushes_survives (void)
{
  static float complex cap[40000];
  const size_t         at = 3000;
  build_capture (cap, sizeof cap / sizeof *cap, at, 0.0, 0.05, 4242u);

  const size_t burst_len
      = REPS * ACQ_SF * SPC + (SYNC_LEN + PAYLOAD + 16u) * DATA_SF * SPC;

  /* Cuts INSIDE the burst and PAST the preamble, so a detection exists to
     be held. A cut at a quarter of the burst is deliberately not here: at
     that point acquisition has not finished the dwell carrying the preamble,
     so `pending` is legitimately 0 and the burst is found from scratch on
     the second call. Holding is what this test is about.

     The last cut is the sharp one -- everything has arrived except the final
     sample, and the honest answer is still to hold rather than guess. */
  const size_t cuts[] = { at + burst_len / 2u, at + (3u * burst_len) / 4u,
                          at + burst_len - 1u };

  for (size_t c = 0; c < sizeof cuts / sizeof *cuts; c++)
    {
      dsss_burst_receiver_state_t *s = make_rx ();
      DP_REQUIRE (s != NULL);
      uint8_t bits[4u * PAYLOAD];

      size_t n1
          = dsss_burst_receiver_push (s, cap, cuts[c], bits, sizeof bits);
      DP_CHECK (n1 == 0); /* incomplete: emit nothing rather than guess */
      /* Through the ACCESSOR, not `s->pending`. The field and the getter are
         different surfaces: Python reads the getter, and a getter hardwired
         to 0 passes every test in this file that reads the field directly.
         Verified by sabotage -- it did. */
      DP_CHECK (dsss_burst_receiver_get_pending (s) == 1u);

      size_t n2 = dsss_burst_receiver_push (
          s, cap + cuts[c], (sizeof cap / sizeof *cap) - cuts[c], bits,
          sizeof bits);
      /* the whole FRAME, from the second call */
      DP_CHECK (n2 == FRAME_SYMS);
      DP_CHECK_MSG (frame_ok (bits),
                    "a burst split across two pushes still checks out");
      DP_CHECK (dsss_burst_receiver_get_pending (s) == 0u);
      DP_CHECK (frame_ok (bits));
      for (size_t i = 0; i < PAYLOAD; i++)
        DP_CHECK (bits[SYNC_LEN + i] == payload_bits ()[i]);

      dsss_burst_receiver_destroy (s);
    }
  return 0;
}

static int
test_push_max_out_scales_with_input (void)
{
  dsss_burst_receiver_state_t *s = make_rx ();
  DP_REQUIRE (s != NULL);
  /* The bound scales with the input, because push() returns every burst the
     call completed. A constant payload_len would under-size the buffer the
     moment one call carried two bursts -- which is doppler#1008. */
  DP_CHECK (dsss_burst_receiver_push_max_out (s, 1) >= PAYLOAD);
  DP_CHECK (dsss_burst_receiver_push_max_out (s, 1u << 20)
            > dsss_burst_receiver_push_max_out (s, 1));
  {
    size_t n = (1u << 20) / s->burst_len + 1u + s->q_cap;
    DP_CHECK (dsss_burst_receiver_push_max_out (s, 1u << 20)
              == n * FRAME_SYMS);
  }
  dsss_burst_receiver_destroy (s);
  return 0;
}

/* reset() clears every read-back, and the PRECONDITION is asserted first --
 * otherwise the check passes against state that was already zero, which is
 * the vacuous-reset shape this campaign keeps finding (burst_demod F4).
 * The lifetime counters deliberately survive. */
static int
test_reset_clears_the_event_but_not_the_counters (void)
{
  dsss_burst_receiver_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  /* Stand in for a completed burst. Written directly because push() cannot
   * produce one yet; phase 3 replaces this with a real burst. */
  s->preamble_start = 4096;
  s->doppler_hz_est = 1234.5;
  s->est_snr_db     = 12.0;
  s->refine_margin  = 0.75;
  s->pending        = 2;
  s->n_bursts       = 7;
  s->dropped        = 3;
  s->samples_fed    = 99999;

  DP_REQUIRE (s->preamble_start == 4096 && s->doppler_hz_est != 0.0);

  dsss_burst_receiver_reset (s);

  DP_CHECK (dsss_burst_receiver_get_preamble_start (s) == 0);
  /* no burst, so nothing to check */
  DP_CHECK (dsss_burst_receiver_get_doppler_hz_est (s) == 0.0);
  DP_CHECK (dsss_burst_receiver_get_est_snr_db (s) == 0.0);
  DP_CHECK (dsss_burst_receiver_get_refine_margin (s) == 0.0);
  DP_CHECK (dsss_burst_receiver_get_pending (s) == 0);
  DP_CHECK (s->samples_fed == 0);

  /* Lifetime, on purpose: a reset that zeroed these could hide that the
   * receiver had already lost samples. */
  DP_CHECK (dsss_burst_receiver_get_n_bursts (s) == 7);
  DP_CHECK (dsss_burst_receiver_get_dropped (s) == 3);

  dsss_burst_receiver_destroy (s);
  return 0;
}

/* The state triplet, mid-stream. Split inside the preamble -- before the
 * frame has arrived -- so the restored receiver has to carry the retained
 * LOOK-BACK with it. A blob that omitted the history would still round-trip
 * on a fresh receiver and still fail here, which is why the split is placed
 * where the burst straddles it.
 *
 * state_bytes() is asserted equal across two differently-loaded receivers
 * first: it is a pure function of configuration by contract (jm's binding
 * compares an incoming blob's length against it), and a size that drifted
 * with the stream would make every other check here pass by coincidence. */
static int
test_state_resumes_mid_burst (void)
{
  const size_t         AT = 5000, N_CAP = 40000, CUT = 5800;
  static float complex cap[40000];
  build_capture (cap, N_CAP, AT, 0.0, 0.02, 12345u);

  dsss_burst_receiver_state_t *a = make_rx ();
  dsss_burst_receiver_state_t *b = make_rx ();
  DP_REQUIRE (a != NULL && b != NULL);

  uint8_t out[FRAME_SYMS];
  for (size_t off = 0; off < CUT; off += 777)
    {
      size_t n = (off + 777 <= CUT) ? 777 : (CUT - off);
      DP_CHECK (dsss_burst_receiver_push (a, cap + off, n, out, FRAME_SYMS)
                == 0);
    }

  /* Differently loaded, identical size -- the contract. */
  DP_CHECK (dsss_burst_receiver_state_bytes (a)
            == dsss_burst_receiver_state_bytes (b));

  size_t   nb   = dsss_burst_receiver_state_bytes (a);
  uint8_t *blob = malloc (nb);
  DP_REQUIRE (blob != NULL);
  dsss_burst_receiver_get_state (a, blob);
  DP_CHECK (dsss_burst_receiver_set_state (b, blob) == DP_OK);

  size_t got = 0;
  for (size_t off = CUT; off < N_CAP && got == 0; off += 777)
    {
      size_t n = (off + 777 <= N_CAP) ? 777 : (N_CAP - off);
      got      = dsss_burst_receiver_push (b, cap + off, n, out, FRAME_SYMS);
    }
  DP_CHECK (got == FRAME_SYMS);
  DP_CHECK (frame_ok (out));
  DP_CHECK (dsss_burst_receiver_get_preamble_start (b) == AT);

  /* Envelope reject: clobber the magic and the restore must refuse, not
     reinterpret. Same length, so only the header can catch it. */
  blob[0] = (uint8_t)(blob[0] ^ 0xFFu);
  DP_CHECK (dsss_burst_receiver_set_state (b, blob) == DP_ERR_INVALID);

  free (blob);
  dsss_burst_receiver_destroy (a);
  dsss_burst_receiver_destroy (b);
  return 0;
}

/* destroy(NULL) is a no-op: the create() failure path calls it on a
 * partially built object, so this is the guard that makes `goto fail` safe
 * rather than a double-free waiting for an allocation to fail. */
static int
test_destroy_null_is_safe (void)
{
  dsss_burst_receiver_destroy (NULL);
  DP_CHECK (1);
  return 0;
}

int
main (void)
{
  if (test_create_copies_and_derives ())
    return 1;
  if (test_refuses_bad_arguments ())
    return 1;
  if (test_decodes_a_burst_from_a_stream ())
    return 1;
  if (test_decodes_under_residual_doppler ())
    return 1;
  if (test_true_burst_once_and_false_alarms_marked ())
    return 1;
  if (test_silence_yields_no_burst ())
    return 1;
  if (test_one_giant_push_finds_the_same_burst ())
    return 1;
  if (test_a_burst_near_the_stream_start ())
    return 1;
  if (test_one_burst_many_detections_is_claimed_once ())
    return 1;
  if (test_a_weak_decoy_does_not_cost_the_burst ())
    return 1;
  if (test_every_burst_survives_any_block_size ())
    return 1;
  if (test_acq_saturation_does_not_lose_bursts ())
    return 1;
  if (test_a_burst_split_across_two_pushes_survives ())
    return 1;
  if (test_push_max_out_scales_with_input ())
    return 1;
  if (test_reset_clears_the_event_but_not_the_counters ())
    return 1;
  if (test_state_resumes_mid_burst ())
    return 1;
  if (test_destroy_null_is_safe ())
    return 1;
  DP_TEST_END ("test_dsss_burst_receiver_core");
}
