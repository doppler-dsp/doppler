/**
 * @file test_dll_core.c
 * @brief Unit tests for the DLL (early/prompt/late code-tracking loop).
 *
 * Tests:
 *   1. Lifecycle / NULL-code guard / init==create parity
 *   1b. dp_dll_init defends against a dirty caller stack (rate_aid init)
 *   2. On-time alignment — discriminator ~0, code_rate ~1
 *   3. Code Doppler — code_rate converges to the incoming chip rate
 *   4. Static phase offset is pulled in (discriminator decays)
 *   5. Reset reproducibility
 *   6. segments > 1: sub-epoch partials recover an async symbol clock
 *   6b. Symbol-period aid: the lock detector's looks become coherent over
 *       a symbol
 *   6c. Symbol-period aid: the code loop steers on the aided window, once
 *       per symbol, with bn kept per epoch (the rate is continuous across
 *       the switch)
 *   6d. segments > 1: the discriminator's sums are coherent over its
 *       window -- a residual carrier of one cycle per period nulls it
 *   7. Always-on lock detector: locks on signal, not on noise
 *   8. Long-run false-lock regression (segments=1, ~4188 periods, several
 *      noise seeds, zero Doppler/carrier)
 *   8b. A put across the period wrap dumps nothing and skips nothing, on
 *       both correlation paths (#1287)
 *   11. segments > 1: the output's error is circular -- no decision-SNR
 *       gain over the ideal period sum (#2495)
 *   12. segments > 1: the output's level does not follow the input's,
 *       from -30 to +30 dB of gain (#2495)
 */
#include "doppler/awgn/awgn_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_complex.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Carrier-free spread signal with an ASYNCHRONOUS BPSK data clock: symbol
 * period `tsym` samples (any real), first boundary at `phi` samples, random
 * data; the incoming code runs at rate (1+delta). `snr` > 0 adds the
 * shipped complex AWGN at that per-sample amplitude SNR against the unit
 * signal -- the detection module's convention, power SNR `snr^2` -- and 0
 * leaves it clean. The stream 6b and 6c share. Until #689 this took a
 * "per-rail" sigma for a hand-scaled dp_cgauss, whose E|z|^2 = 1 put the
 * noise 3 dB under the snr its caller sized the detector with. */
static void
make_async_signal (float _Complex *rx, size_t N, const uint8_t *code,
                   size_t sf, size_t sps, double delta, double tsym,
                   double phi, double snr, uint32_t dseed, uint32_t nseed)
{
  uint32_t ds   = dseed;
  int      data = 1;
  long     cur  = -1;
  double   inv  = 1.0 / (double)sps;
  double   cph  = 0.0; /* incoming code phase, chips */
  for (size_t nn = 0; nn < N; nn++)
    {
      long sidx = (long)floor (((double)nn - phi) / tsym);
      if (sidx != cur)
        {
          data = (dp_xs32 (&ds) & 1u) ? 1 : -1;
          cur  = sidx;
        }
      size_t ci = (size_t)fmod (cph, (double)sf);
      float  cs = (code[ci] & 1u) ? -1.0f : 1.0f;
      rx[nn]    = (float)data * cs;
      cph += inv * (1.0 + delta);
    }
  if (snr > 0.0)
    {
      dp_awgn_state_t *g = dp_awgn_create (
          nseed,
          dp_awgn_amplitude_for_snr ((float)(20.0 * log10 (snr)), 1.0f));
      float _Complex *nz = malloc (N * sizeof *nz);
      if (!g || !nz)
        abort (); /* a fixture out of memory */
      (void)dp_awgn_generate (g, N, nz, N);
      for (size_t nn = 0; nn < N; nn++)
        rx[nn] += nz[nn];
      free (nz);
      dp_awgn_destroy (g);
    }
}

/* Build a deterministic 0/1 spreading code (xorshift bits). */
static void
make_code (uint8_t *code, size_t sf, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < sf; i++)
    code[i] = dp_bit (&st) > 0 ? 0u : 1u;
}

/* Carrier-free spread signal at code rate (1+delta), BPSK data per period
 * (random, or held at +1 when `const_data` to isolate code tracking from the
 * data-symbol vs code-period async). `sps` samples per nominal chip. Returns
 * the sample count. */
static size_t
make_signal (float _Complex *rx, const uint8_t *code, size_t sf, size_t sps,
             double delta, size_t nper, uint32_t seed, int const_data)
{
  uint32_t dst    = seed;
  size_t   tsamps = sf * sps;
  double   inv    = 1.0 / (double)sps;
  int      data   = dp_bit (&dst);
  size_t   k      = 0;
  double   cph    = 0.0; /* incoming code phase, chips */
  for (size_t p = 0; p < nper; p++)
    {
      data = const_data ? 1 : dp_bit (&dst);
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          size_t idx  = (size_t)fmod (cph, (double)sf);
          float  csgn = (code[idx] & 1u) ? -1.0f : 1.0f;
          rx[k]       = (float)data * csgn;
          cph += inv * (1.0 + delta); /* incoming chip rate */
        }
    }
  return k;
}

/* Forge one field of @p x's own blob, at @p off within the struct, and
 * require set_state() to refuse it and leave @p x giving the same blob.
 * Returns 1 if it did not. */
static int
forged_is_refused (dp_dll_state_t *x, size_t off, const void *val, size_t len,
                   const char *what)
{
  const size_t   cb     = dp_dll_state_bytes (x);
  unsigned char *before = malloc (cb), *bad = malloc (cb), *now = malloc (cb);
  if (!before || !bad || !now)
    return 1;
  dp_dll_get_state (x, before);
  memcpy (bad, before, cb);
  memcpy (bad + sizeof (dp_state_hdr_t) + off, val, len);
  const int rc = dp_dll_set_state (x, bad);
  dp_dll_get_state (x, now);
  const int failed = rc != DP_ERR_INVALID || memcmp (now, before, cb) != 0;
  if (failed)
    DP_TEST_ERR ("  refused blob: %s\n", what);
  free (before);
  free (bad);
  free (now);
  return failed;
}

/* The field's type is spelled out, and held to the field's size, so the
   forged bytes are exactly the field's. */
#define DLL_FORGE(x, field, type, value, what)                                \
  do                                                                          \
    {                                                                         \
      type v_ = (value);                                                      \
      if (sizeof v_ != sizeof (((dp_dll_state_t *)0)->field))                 \
        fails++;                                                              \
      else                                                                    \
        fails += forged_is_refused ((x), offsetof (dp_dll_state_t, field),    \
                                    &v_, sizeof v_, (what));                  \
    }                                                                         \
  while (0)

/* A refused blob changes nothing (doppler#2092).
 *
 * set_state() used to read the whole struct from the blob into the object
 * and check the symbol aid's geometry after. A refusal put back only the aid
 * rings, so the instance kept the blob's NULLs for its code and buffers --
 * the next push dereferenced NULL -- and leaked its own. The size check is no
 * guard: it compares the header with THIS instance's size and never reads the
 * blob's fields. A 4-segment Dll with the aid at 13.5 partials and a
 * 49-segment one without it have the same state_bytes() (48*45 = 32*64 +
 * 8*14), and any one field forged keeps the size. Each blob below is refused
 * by the SAME instance, which then gives the same blob it gave before, keeps
 * its telemetry, tracks on and destroys cleanly. Refused: the collision;
 * another code length or samples per chip; a forged segment count (into 49
 * segments it overran the chunk buffers, into 1 it left chunk_p NULL); a
 * chunk index past the segments; a non-finite or out-of-code noise offset; a
 * non-finite or negative noise guard; the aid switched on for an unaided
 * instance, or its ring or hypotheses forged; and on an aided instance an
 * infinite period, a window as long as its ring, a best hypothesis past its
 * hypotheses, and a last look ending past the partials counted. Returns the
 * number of failed checks, for main's tally. */
static int
refused_blob_changes_nothing (void)
{
  int fails = 0;
  enum
  {
    SF  = 127,
    SPS = 2
  };
  uint8_t code[SF];
  make_code (code, SF, 2092u);
  float _Complex rx[40 * SF * SPS];
  size_t nrx = make_signal (rx, code, SF, SPS, 0.0, 40, 2092u, 0);
  /* the bound for the largest call below: 49 segments, 3 samples a chip, 5080
     samples is 3,969 partials (dp_dll_steps_max_out) */
  float _Complex out[4096];
  dp_tlm_rec_t recs[512];

  dp_dll_state_t *b
      = dp_dll_create (code, SF, SPS, 0.0, 0.002, 0.707, 0.5, 49);
  dp_dll_state_t *a = dp_dll_create (code, SF, SPS, 0.0, 0.002, 0.707, 0.5, 4);
  dp_dll_state_t *f
      = dp_dll_create (code, 63, SPS, 0.0, 0.002, 0.707, 0.5, 49);
  dp_dll_state_t *g = dp_dll_create (code, SF, 3, 0.0, 0.002, 0.707, 0.5, 49);
  dp_dll_state_t *c = dp_dll_create (code, SF, SPS, 0.0, 0.002, 0.707, 0.5, 4);
  dp_dll_state_t *h = dp_dll_create (code, SF, SPS, 0.0, 0.002, 0.707, 0.5, 1);
  dp_tlm_t       *tlm = dp_tlm_create (4096);
  if (!a || !b || !c || !f || !g || !h || !tlm
      || dp_dll_set_symbol_period (a, 13.5) != DP_OK
      || dp_dll_set_symbol_period (c, 13.5) != DP_OK)
    return 1;
  dp_dll_state_t *all[6] = { a, b, c, f, g, h };
  for (size_t k = 0; k < 6u; k++)
    (void)dp_dll_steps (all[k], rx, nrx / 2, out, sizeof out / sizeof *out);
  if (dp_dll_set_telemetry (b, tlm, "b", 1) != DP_OK)
    return 1;

  const size_t cb = dp_dll_state_bytes (b);
  if (dp_dll_state_bytes (a) != cb || dp_dll_state_bytes (f) != cb
      || dp_dll_state_bytes (g) != cb) /* the premise: equal sizes */
    return 1;
  unsigned char *before = malloc (cb), *blob = malloc (cb), *now = malloc (cb);
  if (!before || !blob || !now)
    return 1;

  /* Blobs of the same size from other configurations. */
  dp_dll_get_state (b, before);
  dp_dll_state_t *from[3] = { a, f, g };
  const char *what[3] = { "the 4-segment aided collision",
                          "another code length", "another samples per chip" };
  for (size_t k = 0; k < 3u; k++)
    {
      dp_dll_get_state (from[k], blob);
      const int rc = dp_dll_set_state (b, blob);
      dp_dll_get_state (b, now);
      if (rc != DP_ERR_INVALID || memcmp (now, before, cb) != 0)
        {
          DP_TEST_ERR ("  refused blob: %s\n", what[k]);
          fails++;
        }
    }

  /* One field forged in the instance's own blob. */
  DLL_FORGE (b, segments, size_t, 50, "a forged segment count, into 49");
  DLL_FORGE (h, segments, size_t, 2, "a forged segment count, into 1");
  DLL_FORGE (b, seg_idx, size_t, 49, "a chunk index past the segments");
  DLL_FORGE (b, off_chips, double, NAN, "a NaN noise offset");
  DLL_FORGE (b, off_chips, double, SF, "a noise offset past the code");
  DLL_FORGE (b, noise_guard, double, NAN, "a NaN noise guard");
  DLL_FORGE (b, noise_guard, double, -1.0, "a negative noise guard");
  DLL_FORGE (b, sym_period, double, 13.5,
             "the aid switched on for an unaided instance");
  DLL_FORGE (c, aid_ring, size_t, 2u * c->aid_ring, "a forged aid ring");
  DLL_FORGE (c, aid_nhyp, size_t, c->aid_nhyp + 1u, "forged aid hypotheses");
  DLL_FORGE (c, sym_period, double, INFINITY, "an infinite symbol period");
  DLL_FORGE (c, aid_len, size_t, c->aid_ring, "a window as long as the ring");
  DLL_FORGE (c, aid_best, size_t, c->aid_nhyp,
             "a best hypothesis past the aid's");
  DLL_FORGE (c, aid_last_end, uint64_t, c->aid_count + 1u,
             "a last look past the partials counted");
  /* The output's level divides every partial (doppler#2495). */
  DLL_FORGE (b, out_level, double, NAN, "a NaN output level");
  DLL_FORGE (b, out_level, double, -1.0, "a negative output level");
  DLL_FORGE (b, out_level, double, INFINITY, "an infinite output level");
  DLL_FORGE (b, out_level_n, uint32_t, DLL_OUT_LEVEL_PERIODS + 1u,
             "an output level counted past its mean's length");

  /* And they track on -- b still telling its telemetry -- then go cleanly. */
  (void)dp_tlm_read (tlm, 512, recs, 512);
  for (size_t k = 0; k < 6u; k++)
    (void)dp_dll_steps (all[k], rx + nrx / 2, nrx - nrx / 2, out,
                        sizeof out / sizeof *out);
  if (dp_tlm_read (tlm, 512, recs, 512) == 0)
    {
      DP_TEST_ERR ("  refused blob: the refusing instance went silent\n");
      fails++;
    }
  if (fabs (dp_dll_get_code_rate (b) - 1.0) > 1e-2
      || fabs (dp_dll_get_code_rate (c) - 1.0) > 1e-2)
    {
      DP_TEST_ERR ("  refused blob: a refusal moved the loop\n");
      fails++;
    }
  free (before);
  free (blob);
  free (now);
  for (size_t k = 0; k < 6u; k++)
    dp_dll_destroy (all[k]);
  dp_tlm_destroy (tlm);
  return fails;
}

/* A bad float is refused, never run (doppler#2103).
 *
 * create() took every float unchecked: a NaN spacing made dll_replica index
 * code[2^62] on x86-64, and a NaN or out-of-domain bn/zeta gave NaN gains
 * that never recover. set_symbol_period() let a NaN past both its guards to
 * ceil(NaN) = 2^63 in the ring sizing, and dp_xcalloc aborted the process.
 * Asserted: each bad argument refused, the edges inside the domain accepted
 * (a spacing just under half the code, a frozen bn = 0, a long but sane
 * period), and a refused period leaving the one already set in place.
 * Returns the number of failed checks, for main's tally. */
static int
bad_floats_are_refused (void)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 2103u);
  const double half = 0.5 * 31.0;
  struct
  {
    double      init_chip, bn, zeta, spacing;
    const char *what;
  } bad[] = {
    { 0.0, 0.01, 0.707, NAN, "a NaN spacing" },
    { 0.0, 0.01, 0.707, INFINITY, "an infinite spacing" },
    { 0.0, 0.01, 0.707, 0.0, "a zero spacing" },
    { 0.0, 0.01, 0.707, -0.5, "a negative spacing" },
    { 0.0, 0.01, 0.707, half, "a spacing of half the code" },
    { NAN, 0.01, 0.707, 0.5, "a NaN seed phase" },
    { INFINITY, 0.01, 0.707, 0.5, "an infinite seed phase" },
    { 0.0, NAN, 0.707, 0.5, "a NaN bn" },
    { 0.0, -0.01, 0.707, 0.5, "a negative bn" },
    { 0.0, INFINITY, 0.707, 0.5, "an infinite bn" },
    { 0.0, 0.01, 0.0, 0.5, "a zero zeta" },
    { 0.0, 0.01, NAN, 0.5, "a NaN zeta" },
  };
  for (size_t k = 0; k < sizeof bad / sizeof *bad; k++)
    {
      dp_dll_state_t *d
          = dp_dll_create (code, 31, 2, bad[k].init_chip, bad[k].bn,
                           bad[k].zeta, bad[k].spacing, 4);
      if (d)
        {
          DP_TEST_ERR ("  bad float accepted by create: %s\n", bad[k].what);
          fails++;
          dp_dll_destroy (d);
        }
    }
  /* Samples a chip: a double since fractional rates track, so a NaN, an
     infinity, zero, a negative or under one sample a chip (the code NCO
     would step more than a chip a sample) is refused rather than quietly
     run at 1. */
  static const struct
  {
    double      sps;
    const char *what;
  } bad_sps[] = {
    { NAN, "a NaN sps" },
    { INFINITY, "an infinite sps" },
    { 0.0, "a zero sps" },
    { -2.0, "a negative sps" },
    { 0.5, "half a sample a chip" },
  };
  for (size_t k = 0; k < sizeof bad_sps / sizeof *bad_sps; k++)
    {
      dp_dll_state_t *d
          = dp_dll_create (code, 31, bad_sps[k].sps, 0.0, 0.01, 0.707, 0.5, 4);
      if (d)
        {
          DP_TEST_ERR ("  bad sps accepted by create: %s\n", bad_sps[k].what);
          fails++;
          dp_dll_destroy (d);
        }
    }
  /* dp_dll_init(), the by-value door, holds the same rule: refused with
     nothing written. */
  for (size_t k = 0; k < sizeof bad_sps / sizeof *bad_sps; k++)
    {
      dp_dll_state_t v;
      memset (&v, 0x5a, sizeof v);
      const int rc
          = dp_dll_init (&v, code, 31, bad_sps[k].sps, 0.0, 0.01, 0.707, 0.5);
      const unsigned char *b         = (const unsigned char *)&v;
      int                  untouched = 1;
      for (size_t i = 0; i < sizeof v; i++)
        untouched &= b[i] == 0x5a;
      if (rc != DP_ERR_INVALID || !untouched)
        {
          DP_TEST_ERR ("  bad sps accepted by init: %s\n", bad_sps[k].what);
          fails++;
        }
    }
  /* The edges inside the domain are taken: one sample a chip, and a
     fractional rate (11 MSa/s over 5 Mcps). */
  for (int k = 0; k < 2; k++)
    {
      dp_dll_state_t *g
          = dp_dll_create (code, 31, k ? 2.2 : 1.0, 0.0, 0.01, 0.707, 0.5, 4);
      if (!g)
        {
          DP_TEST_ERR ("  create refused sps %.1f\n", k ? 2.2 : 1.0);
          fails++;
        }
      dp_dll_destroy (g);
    }
  /* The edges inside the domain are taken. */
  dp_dll_state_t *e
      = dp_dll_create (code, 31, 2, 7.25, 0.0, 0.707, half - 0.1, 4);
  if (!e)
    {
      DP_TEST_ERR ("  create refused an argument inside the domain\n");
      return fails + 1;
    }
  dp_dll_destroy (e);

  dp_dll_state_t *d = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  if (!d)
    return fails + 1;
  /* A negative period is refused, not taken as zero: -1.0 and a tiny negative
     are both the case the zero branch used to swallow. */
  const double periods[]
      = { NAN, INFINITY, -INFINITY, -1.0, -1e-300, 1048576.0 + 1.0 };
  for (int pass = 0; pass < 2; pass++)
    {
      const double held = d->sym_period;
      for (size_t k = 0; k < sizeof periods / sizeof *periods; k++)
        if (dp_dll_set_symbol_period (d, periods[k]) != DP_ERR_INVALID
            || d->sym_period != held)
          {
            DP_TEST_ERR ("  bad period %g accepted (pass %d)\n", periods[k],
                         pass);
            fails++;
          }
      /* A long but sane period is taken, then held through the refusals. */
      if (pass == 0 && dp_dll_set_symbol_period (d, 1.0e5) != DP_OK)
        {
          DP_TEST_ERR ("  a period of 1e5 partials was refused\n");
          fails++;
        }
    }
  /* The cap, pinned at its edge: 2^20 partials taken, one more refused. */
  if (dp_dll_set_symbol_period (d, 1048576.0) != DP_OK)
    {
      DP_TEST_ERR ("  the period cap itself (2^20) was refused\n");
      fails++;
    }
  if (dp_dll_set_symbol_period (d, 1048576.0 + 1.0) != DP_ERR_INVALID
      || d->sym_period != 1048576.0)
    {
      DP_TEST_ERR ("  a period one past the cap was taken\n");
      fails++;
    }

  /* configure() and the bn setter refuse what create refuses, and change
     nothing doing it: a NaN, and a negative bn with zeta >= 1 (a zero
     denominator in the gains). */
  const double kp = d->lf.kp, ki = d->lf.ki, bn0 = d->bn, z0 = d->zeta;
  if (dp_dll_configure (d, NAN, 0.707) != DP_ERR_INVALID
      || dp_dll_configure (d, -1.25, 1.0) != DP_ERR_INVALID
      || dp_dll_configure (d, 0.01, INFINITY) != DP_ERR_INVALID
      || dp_dll_set_bn (d, NAN) != DP_ERR_INVALID || d->lf.kp != kp
      || d->lf.ki != ki || d->bn != bn0 || d->zeta != z0)
    {
      DP_TEST_ERR ("  configure or set_bn took a bad loop parameter\n");
      fails++;
    }
  if (dp_dll_configure (d, 0.02, 0.707) != DP_OK || d->bn != 0.02)
    {
      DP_TEST_ERR ("  configure refused a good loop parameter\n");
      fails++;
    }
  dp_dll_destroy (d);
  return fails;
}

/* A refused retune changes nothing, and a forged config is refused
 * (doppler#2103).
 *
 * set_symbol_period() scaled the integrator before the loop filter refused
 * the new interval, then returned DP_OK anyway, so a refused period left
 * integ multiplied while the gains kept the old interval. The Dll now checks
 * the interval before anything is freed or sized. set_state() restored
 * spacing, bn and zeta unchecked, and a forged NaN spacing indexed the code
 * out of bounds. Asserted, against the whole state's blob rather than one
 * field: each refused period leaves the object byte-identical, and each
 * forged config field is refused with the object unchanged. Returns the
 * number of failed checks. */
static int
refused_retune_changes_nothing (void)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 2103u);

  /* The two repros: a bandwidth valid at the create's interval whose gains
     overflow at the symbol's. Each period divided by the segments is
     refused by the loop filter. */
  struct
  {
    size_t      sps;
    double      bn;
    size_t      segments;
    double      period;
    const char *what;
  } retune[] = {
    { 1, 1.0e153, 4, 40.0, "bn 1e153, segments 4, period 40" },
    { 2, 1.0e150, 4, 1048576.0, "bn 1e150, segments 4, period 2^20" },
  };
  for (size_t k = 0; k < sizeof retune / sizeof *retune; k++)
    {
      dp_dll_state_t *d
          = dp_dll_create (code, 31, retune[k].sps, 0.0, retune[k].bn, 0.707,
                           0.5, retune[k].segments);
      if (!d)
        {
          DP_TEST_ERR ("  create refused: %s\n", retune[k].what);
          fails++;
          continue;
        }
      /* A nonzero integrator, so a scale that ran would show. */
      d->lf.integ           = 3.0;
      const size_t   cb     = dp_dll_state_bytes (d);
      unsigned char *before = malloc (cb), *now = malloc (cb);
      dp_dll_get_state (d, before);
      if (dp_dll_set_symbol_period (d, retune[k].period) != DP_ERR_INVALID)
        {
          DP_TEST_ERR ("  refused retune returned DP_OK: %s\n",
                       retune[k].what);
          fails++;
        }
      /* The size must hold after the refusal, or the compare below reads past
         the blob it was sized for. */
      DP_CHECK (dp_dll_state_bytes (d) == cb);
      dp_dll_get_state (d, now);
      if (memcmp (now, before, cb) != 0)
        {
          DP_TEST_ERR ("  refused retune changed state: %s\n", retune[k].what);
          fails++;
        }
      free (before);
      free (now);
      dp_dll_destroy (d);
    }

  /* Forged config in a live blob: spacing, bn and zeta, at their edges. */
  dp_dll_state_t *d = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  if (!d)
    return fails + 1;
  const double nan = NAN, inf = INFINITY, zero = 0.0, half = 15.5;
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, spacing), &nan,
                              sizeof nan, "a NaN spacing");
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, spacing), &inf,
                              sizeof inf, "an infinite spacing");
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, spacing), &zero,
                              sizeof zero, "a zero spacing");
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, spacing), &half,
                              sizeof half, "a spacing of half the code");
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, bn), &nan,
                              sizeof nan, "a NaN bn");
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, zeta), &zero,
                              sizeof zero, "a zero zeta");
  dp_dll_destroy (d);
  return fails;
}

/* The replica read at a PHASE WORD (doppler#2397) against its definition,
 * in double: the code held at 2 samples/chip on a grid a quarter chip in,
 * linearly interpolated between grid points. The word w is the tap's chip
 * phase c = (w + tap_grid) / 2^32 * sf, already moved to the grid origin,
 * so its grid position is 2c - 0.5 folded over 2*sf. Random words over the
 * whole period, at a short code and a long one, plus the words on either
 * side of the wrap, where the last grid point interpolates toward chip 0.
 * The tolerance is the float weight's rounding and tap_grid's half word, a
 * few 1e-7; a swapped weight or a missing wrap is an error near 2. Returns
 * the number of failed checks. */
static int
replica_word_matches_its_definition (void)
{
  int          fails  = 0;
  const size_t sfs[2] = { 7, 1023 };
  for (int k = 0; k < 2; k++)
    {
      const size_t sf   = sfs[k];
      uint8_t     *code = malloc (sf);
      make_code (code, sf, 2397u + (uint32_t)sf);
      dp_dll_state_t *d
          = dp_dll_create (code, sf, 2, 0.0, 0.01, 0.707, 0.5, 1);
      DP_CHECK (code != NULL && d != NULL);
      if (!code || !d)
        return fails + 1;
      const double sf2   = 2.0 * (double)sf;
      double       worst = 0.0;
      uint32_t     rng   = 0x2397u;
      for (int n = 0; n < 200000 + 4; n++)
        {
          uint32_t w;
          if (n < 4) /* both sides of the wrap, and its two ends */
            w = (uint32_t)((uint32_t[]){ 0u, 1u, 0xFFFFFFFFu,
                                         0xFFFFFFFEu }[n]);
          else
            w = dp_xs32 (&rng);
          const double c = ((double)(uint32_t)(w + d->tap_grid) / 4294967296.0)
                           * (double)sf;
          double       p = fmod (2.0 * c - 0.5 + sf2, sf2);
          if (p >= sf2)
            p -= sf2;
          const size_t i  = (size_t)p;
          const double mu = p - (double)i;
          const size_t j  = (i + 1) % (2 * sf);
          const double v0 = (code[i >> 1] & 1u) ? -1.0 : 1.0;
          const double v1 = (code[j >> 1] & 1u) ? -1.0 : 1.0;
          const double e  = fabs ((double)dll_replica_word (d, w)
                                  - ((1.0 - mu) * v0 + mu * v1));
          if (e > worst)
            worst = e;
        }
      if (!(worst < 1e-5))
        (void)DP_TEST_ERR ("  sf=%zu: replica off its definition by %g\n", sf,
                           worst);
      DP_CHECK (worst < 1e-5);
      dp_dll_destroy (d);
      free (code);
    }
  return fails;
}

/* Each tap's phase word is its input as a fraction of the period in 32
 * bits, to within the half word llround leaves: tap_grid a quarter chip,
 * tap_spacing the spacing, tap_off this look's noise offset. Returns the
 * number of words that are not. */
static int
tap_words_off (const dp_dll_state_t *d, const char *when)
{
  const double per     = 4294967296.0 / (double)d->sf;
  const double want[3] = { 0.25 * per, d->spacing * per, d->off_chips * per };
  const uint32_t have[3] = { d->tap_grid, d->tap_spacing, d->tap_off };
  int            off     = 0;
  for (int k = 0; k < 3; k++)
    {
      /* The circular distance in words: 2^32 is the period itself. */
      double gap = fabs ((double)have[k] - want[k]);
      if (gap > 2147483648.0)
        gap = 4294967296.0 - gap;
      if (gap > 1.0)
        {
          (void)DP_TEST_ERR ("  %s: tap word %d is %u, its input says %.1f\n",
                             when, k, have[k], want[k]);
          off++;
        }
    }
  return off;
}

/* The taps' phase words are DERIVED (doppler#2397), never stored apart from
 * their inputs: right after create; after epochs whose boundaries redraw
 * the noise offset; after reset; and after a set_state whose blob carries
 * forged words, which must be recomputed from the blob's own spacing and
 * offset rather than taken. Each writer of an input -- configure_geometry,
 * draw_offset, set_state -- refreshes them; a writer that forgot would
 * leave a tap reading the code at a stale phase, with no other symptom
 * than a slightly wrong lock statistic. Returns the number of failed
 * checks. */
static int
tap_words_track_their_inputs (void)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 2397u);
  dp_dll_state_t *d = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  DP_CHECK (d != NULL);
  if (!d)
    return fails;
  DP_CHECK (tap_words_off (d, "after create") == 0);

  /* Epochs of noise: each boundary draws a new offset. */
  enum
  {
    N = 31 * 2 * 40
  };
  static float _Complex x[N], out[N];
  uint32_t st = 7u;
  for (size_t i = 0; i < N; i++)
    x[i] = dp_bit (&st) - 0.5f * I;
  const double off0  = d->off_chips;
  int          moved = 0;
  for (size_t b = 0; b < N; b += 31 * 2)
    {
      (void)dp_dll_steps (d, x + b, 31 * 2, out, N);
      moved |= d->off_chips != off0;
      DP_CHECK (tap_words_off (d, "after an epoch") == 0);
    }
  DP_CHECK (moved); /* the check above saw a redraw, not one offset */

  dp_dll_reset (d);
  DP_CHECK (tap_words_off (d, "after reset") == 0);

  (void)dp_dll_steps (d, x, N / 2, out, N);
  const size_t   nb   = dp_dll_state_bytes (d);
  unsigned char *blob = malloc (nb);
  DP_CHECK (blob != NULL);
  if (!blob)
    return fails;
  dp_dll_get_state (d, blob);
  /* Forge the three words in the blob's struct image (it follows the
     envelope header): set_state must derive them, not take them. */
  const uint32_t junk = 0x12345678u;
  const size_t   base = sizeof (dp_state_hdr_t);
  memcpy (blob + base + offsetof (dp_dll_state_t, tap_grid), &junk, 4);
  memcpy (blob + base + offsetof (dp_dll_state_t, tap_spacing), &junk, 4);
  memcpy (blob + base + offsetof (dp_dll_state_t, tap_off), &junk, 4);
  dp_dll_state_t *e = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  DP_CHECK (e != NULL && dp_dll_set_state (e, blob) == DP_OK);
  if (e)
    {
      DP_CHECK (tap_words_off (e, "after set_state of forged words") == 0);
      DP_CHECK (e->off_chips == d->off_chips);
      dp_dll_destroy (e);
    }
  free (blob);
  dp_dll_destroy (d);
  return fails;
}

/* set_state() refuses a forged blob or recomputes what a blob cannot be
 * trusted with (doppler#2103). An aided Dll is set up at a symbol period, so
 * the blob carries a real update interval and a held snapshot.
 *
 * Refused, the target unchanged: a NaN integrator (the filter's or the held
 * snapshot's), a NaN seed phase, a NaN chip phase.
 * Recomputed, the target restored to the honest state: a forged loop gain
 * (kp), a forged update interval (lf.t), a forged rate table (rate_p).
 * Accepted, a blob written at the aided interval resumes into a fresh object
 * set to the same period. Returns the number of failed checks. */
static int
forged_state_is_refused_or_recomputed (void)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 2103u);
  const double period = 8.0; /* 8 partials over 4 segments: interval 2 */

  dp_dll_state_t *d = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  if (!d || dp_dll_set_symbol_period (d, period) != DP_OK)
    return 1;
  d->lf.integ      = 3.0;
  d->held_lf.integ = 1.5;
  d->seed_chip     = 4.0;
  /* A hold, so the snapshot is a live filter and the held rows below forge
     a real one. */
  dp_dll_hold_here (d);
  /* The held integrator set apart from the live one: hold_here copies lf, so
     without this a swap of the two fields would pass. */
  d->held_lf.integ      = 1.5;
  const size_t   cb     = dp_dll_state_bytes (d);
  unsigned char *honest = malloc (cb);
  dp_dll_get_state (d, honest);

  /* The own blob resumes at the aided interval. */
  dp_dll_state_t *fresh
      = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  DP_CHECK (fresh != NULL
            && dp_dll_set_symbol_period (fresh, period) == DP_OK);
  DP_CHECK (fresh && dp_dll_set_state (fresh, honest) == DP_OK);
  /* Both integrators come back, each in its own field: the held one is the
     distinct value, so a restore that swapped or dropped it would show. */
  DP_CHECK (fresh && fresh->lf.integ == 3.0 && fresh->held_lf.integ == 1.5);

  const double subnormal = 1e-310; /* positive, but no interval at all */
  /* Refused: the target is byte-identical afterwards. A NaN integrator or
     seed is deliberately not guarded: a caller who forges a non-finite
     value gets garbage out, and no check is spent refusing it. */
  const double nan = NAN;
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, chip_pos), &nan,
                              sizeof nan, "a NaN chip phase");
  /* The filters and the update rate are restored verbatim, so a forged gain,
     interval or rate is validated and refused, never patched up. */
  fails += forged_is_refused (
      d, offsetof (dp_dll_state_t, lf) + offsetof (dp_loop_filter_state_t, kp),
      &nan, sizeof nan, "a NaN loop gain");
  fails += forged_is_refused (
      d, offsetof (dp_dll_state_t, lf) + offsetof (dp_loop_filter_state_t, t),
      &nan, sizeof nan, "a NaN update interval");
  fails += forged_is_refused (d,
                              offsetof (dp_dll_state_t, held_lf)
                                  + offsetof (dp_loop_filter_state_t, kp),
                              &nan, sizeof nan, "a NaN held loop gain");
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, inv_upd), &nan,
                              sizeof nan, "a NaN update rate");

  /* The repro: a bandwidth refused at a symbol interval below one. segments 4,
     a period of 2 partials (interval 0.5): configure at 5e153 must refuse. */
  dp_dll_state_t *r = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  DP_CHECK (r != NULL && dp_dll_set_symbol_period (r, 2.0) == DP_OK);
  DP_CHECK (r != NULL && dp_dll_configure (r, 5e153, 0.707) == DP_ERR_INVALID);
  if (r)
    dp_dll_destroy (r);

  /* A bandwidth valid at t = 1 but overflowing at the aided interval (2) is
     refused by configure and by set_state at that interval. The edge is
     found, not assumed: bisect the t = 1 bound, then take three quarters of
     it, which is valid at 1 and past the bound at 2. */
  double hi = 1.0e308, lo = 0.0;
  for (int i = 0; i < 2000; i++)
    {
      const double mid = 0.5 * (lo + hi);
      if (dp_loop_filter_params_ok (mid, 0.707, 1.0))
        lo = mid;
      else
        hi = mid;
    }
  const double edge_bn = 0.75 * lo;
  DP_CHECK (dp_loop_filter_params_ok (edge_bn, 0.707, 1.0));
  DP_CHECK (!dp_loop_filter_params_ok (edge_bn, 0.707, 2.0));
  DP_CHECK (dp_dll_configure (d, edge_bn, 0.707) == DP_ERR_INVALID);
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, bn), &edge_bn,
                              sizeof edge_bn,
                              "a bandwidth valid at t = 1 but not at the "
                              "aided interval");

  /* Refused: an interval or period that is no symbol timing at all. A
     subnormal period made inv_upd infinite, then NaN; a held snapshot with no
     interval is no filter a hold leaves. */
  fails += forged_is_refused (d, offsetof (dp_dll_state_t, sym_period),
                              &subnormal, sizeof subnormal,
                              "a subnormal symbol period");
  const double zero = 0.0;
  fails += forged_is_refused (d,
                              offsetof (dp_dll_state_t, held_lf)
                                  + offsetof (dp_loop_filter_state_t, t),
                              &zero, sizeof zero, "a held interval of zero");
  fails += forged_is_refused (d,
                              offsetof (dp_dll_state_t, held_lf)
                                  + offsetof (dp_loop_filter_state_t, t),
                              &subnormal, sizeof subnormal,
                              "a held interval of 1e-310");

  /* Ignored: the rate table is the live object's, not the blob's, so a forged
     one is ignored and the target ends in the honest state. */
  struct
  {
    size_t      off;
    const char *what;
  } recompute[] = {
    { offsetof (dp_dll_state_t, rate_p), "a NaN rate table" },
  };
  for (size_t k = 0; k < sizeof recompute / sizeof *recompute; k++)
    {
      dp_dll_state_t *x
          = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
      if (!x || dp_dll_set_symbol_period (x, period) != DP_OK)
        return fails + 1;
      unsigned char *bad = malloc (cb), *now = malloc (cb);
      memcpy (bad, honest, cb);
      memcpy (bad + sizeof (dp_state_hdr_t) + recompute[k].off, &nan,
              sizeof nan);
      if (dp_dll_set_state (x, bad) != DP_OK)
        {
          DP_TEST_ERR ("  forged blob refused, not recomputed: %s\n",
                       recompute[k].what);
          fails++;
        }
      dp_dll_get_state (x, now);
      if (memcmp (now, honest, cb) != 0)
        {
          DP_TEST_ERR ("  forged blob restored, not recomputed: %s\n",
                       recompute[k].what);
          fails++;
        }
      free (bad);
      free (now);
      dp_dll_destroy (x);
    }

  free (honest);
  dp_dll_destroy (fresh);
  dp_dll_destroy (d);
  return fails;
}

/* The hold path resumes bit-for-bit (doppler#2103). A hold, then a retune of
 * the bandwidth, then a round trip must coast and step exactly as the object
 * that was never serialized. The held snapshot keeps the bandwidth it was
 * taken with, so the retune after the hold must not change what the coast
 * replays. Returns the number of failed checks. */
/* The per-steer telemetry set (dp_dll_set_steer_telemetry()) reports at
 * the symbol aid's steers and moves nothing: two Dlls on one stream, one
 * with the set attached, give the same bytes out and end in the same
 * state, and the set's last records are the filter's own fields. With the
 * aid off it reports nothing (the epoch set is the steer's then).
 * Sabotage: the emit placed before dll_steer() -> its last record is the
 * previous steer's integrator, not the field; an emit that stores into the
 * filter -> the bytes differ. */
static int
steer_telemetry_reports_the_steer_and_moves_nothing (void)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 3215u);
  dp_dll_state_t *a = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  dp_dll_state_t *b = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  dp_tlm_t       *t = dp_tlm_create (1 << 14);
  if (!a || !b || !t || dp_dll_set_symbol_period (a, 8.0) != DP_OK
      || dp_dll_set_symbol_period (b, 8.0) != DP_OK)
    return 1;
  DP_CHECK (dp_dll_set_steer_telemetry (a, t, "code.steer", 1) == DP_OK);
  float _Complex rx[512], out_a[256], out_b[256];
  for (size_t i = 0; i < 512; i++)
    rx[i] = CMPLXF (cosf (0.3f * (float)i), sinf (0.7f * (float)i));
  const size_t na = dp_dll_steps (a, rx, 512, out_a, 256);
  const size_t nb = dp_dll_steps (b, rx, 512, out_b, 256);
  DP_CHECK (na == nb && na > 0
            && memcmp (out_a, out_b, na * sizeof *out_a) == 0);
  const size_t   cb = dp_dll_state_bytes (a);
  unsigned char *sa = malloc (cb), *sb = malloc (cb);
  dp_dll_get_state (a, sa);
  dp_dll_get_state (b, sb);
  DP_CHECK (memcmp (sa, sb, cb) == 0);
  free (sa);
  free (sb);
  const int    id_e = dp_tlm_probe_id (t, "code.steer.e");
  const int    id_i = dp_tlm_probe_id (t, "code.steer.integ");
  dp_tlm_rec_t recs[1 << 12];
  const size_t nr = dp_tlm_read (t, 1 << 12, recs, 1 << 12);
  float        le = NAN, li = NAN;
  size_t       ne = 0;
  for (size_t j = 0; j < nr; j++)
    {
      if ((int)recs[j].probe == id_e)
        {
          le = recs[j].value;
          ne++;
        }
      else if ((int)recs[j].probe == id_i)
        li = recs[j].value;
    }
  DP_CHECK (ne > 0);
  DP_CHECK (le == (float)a->last_error && li == (float)a->lf.integ);
  /* The aid off: no steer of the aid, nothing reported. */
  DP_CHECK (dp_dll_set_symbol_period (a, 0.0) == DP_OK);
  (void)dp_dll_steps (a, rx, 512, out_a, 256);
  DP_CHECK (dp_tlm_read (t, 1 << 12, recs, 1 << 12) == 0);
  dp_tlm_destroy (t);
  dp_dll_destroy (a);
  dp_dll_destroy (b);
  return fails;
}

static int
hold_then_retune_resumes_exact (void)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 2103u);
  dp_dll_state_t *a = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  if (!a || dp_dll_set_symbol_period (a, 8.0) != DP_OK)
    return 1;
  float _Complex rx[128], out[64], out_a[64], out_c[64];
  for (size_t i = 0; i < 128; i++)
    rx[i] = CMPLXF (cosf (0.3f * (float)i), sinf (0.7f * (float)i));
  (void)dp_dll_steps (a, rx, 128, out, 64); /* warm up: a live state */
  dp_dll_hold_here (a);
  DP_CHECK (dp_dll_set_bn (a, 0.02) == DP_OK); /* a retune after the hold */

  const size_t   cb   = dp_dll_state_bytes (a);
  unsigned char *blob = malloc (cb);
  dp_dll_get_state (a, blob);
  dp_dll_state_t *c = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  DP_CHECK (c != NULL && dp_dll_set_symbol_period (c, 8.0) == DP_OK);
  DP_CHECK (c != NULL && dp_dll_set_state (c, blob) == DP_OK);
  if (c)
    {
      dp_dll_set_coast (a, 1);
      dp_dll_set_coast (c, 1);
      const size_t na = dp_dll_steps (a, rx, 128, out_a, 64);
      const size_t nc = dp_dll_steps (c, rx, 128, out_c, 64);
      DP_CHECK (na == nc && na > 0);
      DP_CHECK (memcmp (out_a, out_c, na * sizeof *out_a) == 0);
      const size_t   cb_a = dp_dll_state_bytes (a);
      unsigned char *sa = malloc (cb_a), *sc = malloc (cb_a);
      dp_dll_get_state (a, sa);
      dp_dll_get_state (c, sc);
      DP_CHECK (memcmp (sa, sc, cb_a) == 0);
      free (sa);
      free (sc);
      dp_dll_destroy (c);
    }
  free (blob);
  dp_dll_destroy (a);
  return fails;
}

/* The coast-first orders resume bit-for-bit (doppler#2103). A hold, then one
 * of three orders of operations, then a round trip, must coast, uncoast and
 * step exactly as the object that was never serialized. order 0: a bandwidth
 * retune after the hold; order 1: a symbol period change after the hold;
 * order 2: a re-hold while coasting. Returns the number of failed checks. */
static int
coast_order_resumes_exact (int order)
{
  int     fails = 0;
  uint8_t code[31];
  make_code (code, 31, 2103u);
  const double    period_a = order == 1 ? 4.0 : 8.0;
  dp_dll_state_t *a = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  if (!a || dp_dll_set_symbol_period (a, 8.0) != DP_OK)
    return 1;
  float _Complex rx[128], warm[64], out_a[64], out_c[64];
  for (size_t i = 0; i < 128; i++)
    rx[i] = CMPLXF (cosf (0.3f * (float)i), sinf (0.7f * (float)i));
  (void)dp_dll_steps (a, rx, 128, warm, 64); /* warm up: a live state */
  dp_dll_hold_here (a);
  if (order == 0)
    DP_CHECK (dp_dll_set_bn (a, 0.02) == DP_OK);
  else if (order == 1)
    DP_CHECK (dp_dll_set_symbol_period (a, period_a) == DP_OK);
  else
    {
      dp_dll_set_coast (a, 1);
      dp_dll_hold_here (a); /* the re-hold, while coasting */
    }
  dp_dll_set_coast (a, 1);

  const size_t   cb   = dp_dll_state_bytes (a);
  unsigned char *blob = malloc (cb);
  dp_dll_get_state (a, blob);
  dp_dll_state_t *c = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 4);
  DP_CHECK (c != NULL && dp_dll_set_symbol_period (c, period_a) == DP_OK);
  DP_CHECK (c != NULL && dp_dll_set_state (c, blob) == DP_OK);
  if (c)
    {
      dp_dll_set_coast (a, 0); /* the uncoast, after the round trip */
      dp_dll_set_coast (c, 0);
      const size_t na = dp_dll_steps (a, rx, 128, out_a, 64);
      const size_t nc = dp_dll_steps (c, rx, 128, out_c, 64);
      DP_CHECK (na == nc && na > 0);
      DP_CHECK (memcmp (out_a, out_c, na * sizeof *out_a) == 0);
      const size_t   cb_a = dp_dll_state_bytes (a);
      unsigned char *sa = malloc (cb_a), *sc = malloc (cb_a);
      dp_dll_get_state (a, sa);
      dp_dll_get_state (c, sc);
      DP_CHECK (memcmp (sa, sc, cb_a) == 0);
      free (sa);
      free (sc);
      dp_dll_destroy (c);
    }
  free (blob);
  dp_dll_destroy (a);
  return fails;
}

/* A coast reports the rate it runs at. Entering the hold restores the
 * filter to the last dp_dll_hold_here() and steers the NCO at that filter's
 * integrator; code_rate -- dp_dll_get_code_rate(), the ".rate" probe, and
 * the receiver's and the pool's status records -- must be that same rate,
 * less the carrier aid, and not the last steer's. The loop is held on one
 * code Doppler, then driven by another so the live integrator moves well
 * off the held one, then coasted. Before the fix code_rate kept the last
 * steer's value for the whole coast while the NCO ran the held filter. A
 * new aid while coasting re-steers the NCO and must leave the two agreeing.
 * Returns the number of failed checks. */
static int
coast_reports_the_held_rate (size_t segments)
{
  int          fails = 0;
  const size_t sf = 63, sps = 4, nper = 800;
  uint8_t     *code = malloc (sf);
  make_code (code, sf, 23u);
  float _Complex *rx = malloc (sf * sps * nper * sizeof *rx);
  dp_dll_state_t *d
      = dp_dll_create (code, sf, sps, 0.0, 0.01, 0.707, 0.5, segments);
  DP_REQUIRE (code && rx && d);
  /* sized from the bound for the longest call, nper periods: a carried
     phase closes one more period than the nominal count */
  const size_t    cap = dp_dll_steps_max_out (d, sf * sps * nper);
  float _Complex *sym = malloc (cap * sizeof *sym);
  DP_REQUIRE (sym);
  const double aid = 1e-5;
  dp_dll_set_rate_aid (d, aid);
  size_t n = make_signal (rx, code, sf, sps, 3e-4, nper, 9u, 1);
  (void)dp_dll_steps (d, rx, n, sym, cap);
  dp_dll_hold_here (d);
  n = make_signal (rx, code, sf, sps, -3e-4, nper, 11u, 1);
  (void)dp_dll_steps (d, rx, n, sym, cap);
  const double live = dp_dll_get_code_rate (d);
  dp_dll_set_coast (d, 1);

  /* The held rate reaches the NCO at the next period boundary, where it is
     converted through the carried remainder: one epoch on, the NCO runs it.
     One phase-increment step, in chips per chip, is the NCO's resolution,
     and a rate that is not a whole word alternates between the two either
     side of it. */
  (void)dp_dll_steps (d, rx, sf * sps, sym, cap);
  const double q    = 1.0 / 4294967296.0 / d->inv_tsamps;
  double       cmd  = (double)d->code_nco.phase_inc * q;
  double       rate = dp_dll_get_code_rate (d);
  DP_CHECK_MSG (fabs (live - rate) > 100.0 * q,
                "the second Doppler moved the live integrator off the hold");
  DP_CHECK_MSG (fabs (cmd - (rate + aid)) <= q,
                "coasting, code_rate is the rate the NCO runs at, less the "
                "aid");
  if (fabs (cmd - (rate + aid)) > q)
    DP_TEST_ERR ("  segments %zu: NCO %.9f, code_rate %.9f + aid %.1e\n",
                 segments, cmd, rate, aid);

  /* A new aid while coasting re-steers the held NCO at the next boundary;
     the rate stays. */
  dp_dll_set_rate_aid (d, 4.0 * aid);
  (void)dp_dll_steps (d, rx, sf * sps, sym, cap);
  cmd = (double)d->code_nco.phase_inc * q;
  DP_CHECK (dp_dll_get_code_rate (d) == rate);
  DP_CHECK_MSG (fabs (cmd - (rate + 4.0 * aid)) <= q,
                "a coasting loop's new aid reaches the NCO, not the rate");

  /* And it stays so while coasting: no steer moves either. */
  (void)dp_dll_steps (d, rx, n, sym, cap);
  DP_CHECK (dp_dll_get_code_rate (d) == rate);
  DP_CHECK (fabs ((double)d->code_nco.phase_inc * q - cmd) <= q);

  dp_dll_destroy (d);
  free (sym);
  free (rx);
  free (code);
  return fails;
}

/* ------------------------------------------------------------------ *
 * 11. segments > 1: the output's error is circular (doppler#2495).   *
 *     Each partial is its window's prompt sum over the samples it    *
 *     spans and a SLOWLY tracked level -- never over its period's    *
 *     own signal-plus-noise magnitude. A divisor drawn from the noise *
 *     it divides is an AGC on that noise: it compresses the in-phase *
 *     fluctuations (which inflate the divisor) and not the           *
 *     quadrature ones, so the error stops being circular, and the    *
 *     decision SNR reads HIGHER than the despread's own while the    *
 *     error rate is worse. Measured on the receiver's chain: an I/Q  *
 *     variance ratio of -2.2 dB and a decision SNR 1.4 dB above the  *
 *     despread's at Es/N0 5 dB, with 0.2 dB more BER loss.           *
 *     The property is pinned, not a number: on a held loop at truth, *
 *     with noise, the output's in-phase and quadrature errors have   *
 *     equal power, and its decision SNR is the ideal period sum's.   *
 * ---------------------------------------------------------------- */
static int
output_error_is_circular (void)
{
  const size_t sf = 63, sps = 2, segs = 4, nper = 20000;
  const size_t tsamps = sf * sps;
  /* The noise: sigma times dp_cgauss(), so sigma^2 / 2 a rail. A period's
     coherent decision SNR is then 3 dB (126 / 63), a partial's -3 dB --
     where a divisor taken from the period's own magnitude is mostly
     noise. */
  const double sigma = sqrt (63.0);
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 0x2495u);
  float _Complex *rx = malloc (tsamps * nper * sizeof *rx);
  size_t          n  = make_signal (rx, code, sf, sps, 0.0, nper, 7u, 1);
  uint32_t        ns = 0x5eed2495u;
  for (size_t i = 0; i < n; i++)
    rx[i] += (float)sigma * dp_cgauss (&ns);
  dp_dll_state_t *d
      = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segs);
  DP_CHECK (d != NULL);
  dp_dll_hold_here (d); /* held at truth: the code loop is not under test */
  dp_dll_set_coast (d, 1);
  size_t          cap = segs * (nper + 1);
  float _Complex *out = malloc (cap * sizeof *out);
  size_t          k   = dp_dll_steps (d, rx, n, out, cap);
  DP_CHECK (k >= segs * (nper - 1));

  /* Scored a period at a time, the sum of its `segs` partials: data is
     held at +1, so every sum's mean is real and positive, and a divisor
     shared by a period's partials shows in their sum -- as it does in a
     receiver's symbol, which sums the partials it spans. */
  double si = 0, si2 = 0, sq = 0, sq2 = 0, m = 0;
  for (size_t e = 1; (e + 1) * segs <= k; e++) /* period 0: no look-back */
    {
      float _Complex z = 0;
      for (size_t c = 0; c < segs; c++)
        z += out[e * segs + c];
      si += crealf (z);
      si2 += crealf (z) * crealf (z);
      sq += cimagf (z);
      sq2 += cimagf (z) * cimagf (z);
      m += 1;
    }
  double A = si / m, vi = si2 / m - A * A;
  double vq    = sq2 / m - (sq / m) * (sq / m);
  double iq_db = 10.0 * log10 (vi / vq);
  /* The ideal period sum's decision SNR, A^2 / (2 var_I): tsamps^2 over
     twice tsamps sigma^2 / 2, since dp_cgauss() puts half its unit power
     on each rail -- whatever constant then scales it. */
  double ideal_db = 10.0 * log10 ((double)tsamps / (sigma * sigma));
  double dec_db   = 10.0 * log10 (A * A / (2.0 * vi));
  printf ("  11: I/Q error power %+.3f dB, decision SNR %.2f dB against the "
          "ideal period sum's %.2f dB (%.0f periods)\n",
          iq_db, dec_db, ideal_db, m);
  /* 20k periods put each estimate's own spread near 0.05 dB; the bands
     are five times that. The per-period normalization this replaces read
     -2.29 dB and a 1.75 dB gain here. Circular, and no gain: */
  DP_CHECK (fabs (iq_db) < 0.25);
  DP_CHECK (dec_db - ideal_db < 0.25);
  dp_dll_destroy (d);
  free (out);
  free (rx);
  free (code);
  return 0;
}

/* ------------------------------------------------------------------ *
 * 12. segments > 1: the output's level does not follow the input's   *
 *     (doppler#2495). Every timing detector downstream assumes a     *
 *     stream near unit amplitude, and MpskReceiver's AGC, 20x slower *
 *     than its own loops, cannot set one from scratch: a despread    *
 *     whose output scaled with its input left the receiver deciding  *
 *     differently at +/-15 dB and emitting almost nothing at +30.    *
 *     Pinned at the primitive: the same signal and noise at -30, 0   *
 *     and +30 dB of gain give the same output level once the level   *
 *     has settled, to a part in a thousand (float32's rounding of a  *
 *     non-power-of-two gain, not a level).                           *
 * ---------------------------------------------------------------- */
static int
output_level_is_input_level_free (void)
{
  const size_t sf = 63, sps = 2, segs = 4, nper = 400;
  const size_t tsamps      = sf * sps;
  const double gains_db[3] = { -30.0, 0.0, 30.0 };
  uint8_t     *code        = malloc (sf);
  make_code (code, sf, 0x2495u);
  float _Complex *rx = malloc (tsamps * nper * sizeof *rx);
  float _Complex *x  = malloc (tsamps * nper * sizeof *x);
  size_t          n  = make_signal (rx, code, sf, sps, 0.0, nper, 7u, 1);
  uint32_t        ns = 0x1e7e1u;
  for (size_t i = 0; i < n; i++)
    rx[i] += 4.0f * dp_cgauss (&ns); /* a period's SNR about 9 dB */
  size_t          cap = segs * (nper + 1);
  float _Complex *out = malloc (cap * sizeof *out);
  /* The level over the last half, past the settle the header derives
     (146 periods at DLL_OUT_LEVEL_PERIODS = 32). */
  double lvl[3];
  for (int g = 0; g < 3; g++)
    {
      const float a = (float)pow (10.0, gains_db[g] / 20.0);
      for (size_t i = 0; i < n; i++)
        x[i] = a * rx[i];
      dp_dll_state_t *d
          = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segs);
      DP_CHECK (d != NULL);
      dp_dll_hold_here (d);
      dp_dll_set_coast (d, 1);
      size_t k  = dp_dll_steps (d, x, n, out, cap);
      double s  = 0.0;
      size_t m0 = k / 2;
      for (size_t j = m0; j < k; j++)
        s += cabsf (out[j]);
      lvl[g] = s / (double)(k - m0);
      dp_dll_destroy (d);
    }
  printf ("  12: output level %.4f at -30 dB, %.4f at 0 dB, %.4f at +30 dB "
          "of input gain\n",
          lvl[0], lvl[1], lvl[2]);
  DP_CHECK (fabs (lvl[0] / lvl[1] - 1.0) < 1e-3);
  DP_CHECK (fabs (lvl[2] / lvl[1] - 1.0) < 1e-3);
  free (out);
  free (x);
  free (rx);
  free (code);
  return 0;
}

/* dp_dll_get_window_sum(): the prompt's coherent sum over the code-period
 * window the look-back picked. A clean, aligned stream whose data symbol is
 * 2.5 code periods long, its first transition half a period in: a period
 * then holds at most one transition, so some window the look-back compares
 * (the natural one, or one borrowing the previous period's tail) misses it
 * by at most a partial, and the winner's magnitude stays near tsamps every
 * epoch -- where the natural window, split in half, is near 0. With and
 * without the symbol-period aid: the look-back runs either way. Sabotage:
 * keep the natural window's sum -> the split epochs read near 0, red. */
static int
_test_window_sum_is_the_look_back (void)
{
  const size_t sf = 63, sps = 2, seg = 8, tsamps = sf * sps, nper = 40;
  uint8_t      code[63];
  make_code (code, sf, 11u);
  float _Complex *rx = malloc (nper * tsamps * sizeof *rx);
  DP_REQUIRE (rx != NULL);
  make_async_signal (rx, nper * tsamps, code, sf, sps, 0.0, 2.5 * tsamps,
                     0.5 * tsamps, 0.0, 5u, 0u);
  for (int aid = 0; aid < 2; aid++)
    {
      dp_dll_state_t *d
          = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, seg);
      DP_REQUIRE (d != NULL);
      if (aid)
        DP_REQUIRE (dp_dll_set_symbol_period (d, 2.5 * seg) == DP_OK);
      DP_CHECK (cabsf (dp_dll_get_window_sum (d)) == 0.0f);
      static float _Complex out[256];
      double worst = HUGE_VAL;
      for (size_t p = 0; p < nper; p++)
        {
          const size_t n = dp_dll_steps (d, rx + p * tsamps, tsamps, out, 256);
          if (p >= 2 && n > 0)
            {
              const double m = cabsf (dp_dll_get_window_sum (d));
              worst          = m < worst ? m : worst;
            }
        }
      DP_CHECK (worst >= (1.0 - 1.5 / (double)seg) * (double)tsamps);
      dp_dll_reset (d);
      DP_CHECK (cabsf (dp_dll_get_window_sum (d)) == 0.0f);
      dp_dll_destroy (d);
    }
  /* One segment: the period is the only window, so the sum is that
     epoch's own prompt sum -- the emitted prompt times the period (the
     prompt is it divided by (float)tsamps). Compared within 4 ulp a part,
     not to the bit: under fast-math the library's division and this
     one may round differently (a reciprocal multiply for one, a divide
     for the other), so the last bit is the build's (#3102's coverage
     job, clang Debug).
     Sabotage: no write on the one-segment path -> it stays 0, red. */
  {
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, 1);
    DP_REQUIRE (d != NULL);
    static float _Complex out1[8];
    size_t epochs = 0, exact = 0;
    for (size_t p = 0; p < nper; p++)
      {
        const size_t n = dp_dll_steps (d, rx + p * tsamps, tsamps, out1, 8);
        if (n == 1)
          {
            epochs++;
            const float _Complex q = dp_dll_get_window_sum (d) / (float)tsamps;
            const float tr
                = 4.0f * FLT_EPSILON * fabsf (crealf (out1[0])) + FLT_MIN;
            const float ti
                = 4.0f * FLT_EPSILON * fabsf (cimagf (out1[0])) + FLT_MIN;
            exact += fabsf (crealf (q) - crealf (out1[0])) <= tr
                     && fabsf (cimagf (q) - cimagf (out1[0])) <= ti
                     && cabsf (out1[0]) > 0.0f;
          }
      }
    DP_REQUIRE (epochs > nper / 2);
    DP_CHECK (exact == epochs);
    dp_dll_destroy (d);
  }
  free (rx);
  return 0;
}

/* The block correlate (dll_block) against the per-sample path it stands in
 * for. A block is two samples or more, so a stream pushed one sample a call
 * never takes it: dp_chunk_inv.h's single-sample partition IS the
 * per-sample path, and every other partition moves where the blocks start
 * and end. Three paths, one answer, to the bit:
 *
 *   the per-sample path   (pulls of one sample)
 *   blocks over rtab       (a create()d Dll, its table)
 *   blocks without it      (the same Dll with rtab dropped: dp_dll_init's
 *                           case, the taps through dll_replica_word)
 *
 * at segments 4 (blocks stop four words short of each partial's close) and
 * 1 (at the wrap), at 2.2 samples a chip so the phase word is never on the
 * grid. Puts across the wrap both ways arrive mid-stream at fixed SAMPLES,
 * whatever the partition, so wrap_pending and wrap_await are crossed by the
 * partitions too; the run asserts it put each at least once.
 *
 * Sabotage, each red: a block one sample longer than dll_block_len allows
 * (it swallows the sample that closes a partial or wraps: 60 partitions
 * disagree); the block loop's early and late taps swapped (58).
 *
 * And the table is no part of the state: the blob of a create()d Dll is
 * the blob with rtab dropped, set_state keeps the live table, and a Dll
 * resumed mid-stream from a blob into a fresh create()d one continues
 * bit-for-bit. Sabotage, red: get_state leaving rtab in the snapshot;
 * set_state taking the blob's NULL. */
enum
{
  BLK_SF   = 63,
  BLK_NPER = 240,
  BLK_NPUT = 12
};

typedef struct
{
  const uint8_t *code;
  size_t         segments;
  int            table;
  const size_t  *put_at; /* BLK_NPUT absolute sample indices, ascending */
  int           *saw_pending, *saw_await;
} blk_cfg_t;

typedef struct
{
  dp_dll_state_t  *d;
  const blk_cfg_t *c;
  size_t           pos, next_put;
} blk_obj_t;

static void *
blk_create (void *arg)
{
  const blk_cfg_t *c = arg;
  blk_obj_t       *o = calloc (1, sizeof *o);
  if (!o)
    return NULL;
  o->c = c;
  o->d = dp_dll_create (c->code, BLK_SF, 2.2, 3.0, 0.01, 0.707, 0.5,
                        c->segments);
  if (!o->d)
    {
      free (o);
      return NULL;
    }
  if (!c->table)
    {
      free (o->d->rtab);
      o->d->rtab = NULL;
    }
  return o;
}

static void
blk_destroy (void *obj)
{
  blk_obj_t *o = obj;
  dp_dll_destroy (o->d);
  free (o);
}

/* Push n samples, stopping at each fixed put sample to move the phase. */
static size_t
blk_process (void *obj, const void *in, size_t n, void *out, size_t cap)
{
  blk_obj_t            *o   = obj;
  const float _Complex *x   = in;
  float _Complex       *y   = out;
  size_t                got = 0, done = 0;
  while (done < n)
    {
      size_t m = n - done;
      if (o->next_put < BLK_NPUT)
        {
          const size_t at = o->c->put_at[o->next_put];
          if (at == o->pos)
            {
              /* Across the wrap at every put: an even put walks the phase
                 forward 0.3 of a period at a time until it is past 0.6,
                 then jumps 0.45 forward over the end (wrap_pending); an
                 odd one the mirror, back over the start (wrap_await). A
                 step is under half a period, so it goes the way it says. */
              const double sfd = (double)BLK_SF,
                           fwd = (o->next_put & 1u) ? -1.0 : 1.0;
              while (fwd > 0 ? o->d->chip_pos <= 0.6 * sfd
                             : o->d->chip_pos >= 0.4 * sfd)
                dp_dll_set_code_phase (o->d, o->d->chip_pos + fwd * 0.3 * sfd);
              dp_dll_set_code_phase (o->d, o->d->chip_pos + fwd * 0.45 * sfd);
              *o->c->saw_pending |= o->d->wrap_pending;
              *o->c->saw_await |= o->d->wrap_await;
              o->next_put++;
              continue;
            }
          if (at - o->pos < m)
            m = at - o->pos;
        }
      got += dp_dll_steps (o->d, x + done, m, y + got, cap - got);
      done += m;
      o->pos += m;
    }
  return got;
}

static int
_test_block_correlate_is_the_per_sample_path (void)
{
  uint8_t         code[BLK_SF];
  const size_t    n = (size_t)((double)(BLK_SF * BLK_NPER) * 2.2);
  float _Complex *x = malloc (n * sizeof *x);
  float _Complex *a = malloc (n * sizeof *a);
  float _Complex *b = malloc (n * sizeof *b);
  size_t          put_at[BLK_NPUT];
  DP_REQUIRE (x && a && b);
  make_code (code, BLK_SF, 3141u);
  /* The code at a rate 1e-4 off, a carrier spinning a few degrees a
     period, and noise: every tap and the loop's steer see changing data. */
  uint32_t r = 2718u;
  for (size_t i = 0; i < n; i++)
    {
      const double chip = (double)i * (1.0 + 1e-4) / 2.2;
      const float  v    = code[(size_t)floor (chip) % BLK_SF] ? -1.0f : 1.0f;
      const float  ph   = (float)(1e-3 * (double)i);
      const float  nr   = (float)(dp_xs32 (&r) >> 8) / 16777216.0f - 0.5f;
      const float  ni   = (float)(dp_xs32 (&r) >> 8) / 16777216.0f - 0.5f;
      x[i] = v * (cosf (ph) + I * sinf (ph)) + 0.8f * (nr + I * ni);
    }
  for (size_t k = 0; k < BLK_NPUT; k++)
    put_at[k] = (k + 1) * (n / (BLK_NPUT + 1)) + 7 * k;

  const size_t segs[2] = { 4, 1 };
  for (size_t si = 0; si < 2; si++)
    {
      int          pend = 0, wait = 0;
      blk_cfg_t    ct   = { code, segs[si], 1, put_at, &pend, &wait };
      blk_cfg_t    cn   = { code, segs[si], 0, put_at, &pend, &wait };
      dp_ci_spec_t spec = { .name     = "dp_dll (table)",
                            .create   = blk_create,
                            .destroy  = blk_destroy,
                            .process  = blk_process,
                            .arg      = &ct,
                            .in_size  = sizeof (float _Complex),
                            .out_size = sizeof (float _Complex),
                            .out_cap  = n,
                            .frame_n  = (size_t)(BLK_SF * 2.2) };
      DP_CHECK_MSG (dp_chunk_invariance (&spec, x, n) == 0,
                    "blocks over the table give the per-sample bits");
      spec.name = "dp_dll (no table)";
      spec.arg  = &cn;
      DP_CHECK_MSG (dp_chunk_invariance (&spec, x, n) == 0,
                    "blocks without the table give the per-sample bits");
      /* The two one-shots, table against none. */
      void *ot = blk_create (&ct), *on = blk_create (&cn);
      DP_REQUIRE (ot && on);
      const size_t kt = blk_process (ot, x, n, a, n);
      const size_t kn = blk_process (on, x, n, b, n);
      DP_CHECK (kt > 0 && kt == kn);
      DP_CHECK_MSG (memcmp (a, b, kt * sizeof *a) == 0,
                    "the table changes no bit");
      blk_destroy (ot);
      blk_destroy (on);
      DP_CHECK_MSG (pend && wait, "the puts crossed the wrap both ways");
      printf ("  block correlate, segments %zu: %zu outputs, one answer by "
              "every partition, table or not\n",
              segs[si], kt);
    }

  /* The table is no part of the state. */
  {
    dp_dll_state_t *d
        = dp_dll_create (code, BLK_SF, 2.2, 3.0, 0.01, 0.707, 0.5, 4);
    DP_REQUIRE (d && d->rtab);
    const size_t   half = n / 2 + 13;
    const size_t   na   = dp_dll_steps (d, x, half, a, n);
    const size_t   cb   = dp_dll_state_bytes (d);
    unsigned char *b1 = malloc (cb), *b2 = malloc (cb);
    DP_REQUIRE (b1 && b2);
    dp_dll_get_state (d, b1);
    float *keep = d->rtab;
    d->rtab     = NULL;
    dp_dll_get_state (d, b2);
    d->rtab = keep;
    DP_CHECK_MSG (memcmp (b1, b2, cb) == 0,
                  "a create()d Dll's blob is its blob without the table");
    DP_CHECK (dp_dll_set_state (d, b1) == DP_OK);
    DP_CHECK_MSG (d->rtab == keep, "set_state keeps the live table");
    /* Resume mid-stream into a fresh create()d Dll, its own table. */
    dp_dll_state_t *e
        = dp_dll_create (code, BLK_SF, 2.2, 0.0, 0.01, 0.707, 0.5, 4);
    DP_REQUIRE (e && e->rtab && e->rtab != keep);
    DP_CHECK (dp_dll_set_state (e, b1) == DP_OK);
    const size_t nd = dp_dll_steps (d, x + half, n - half, a + na, n - na);
    const size_t ne = dp_dll_steps (e, x + half, n - half, b, n);
    DP_CHECK (nd > 0 && nd == ne);
    DP_CHECK_MSG (memcmp (a + na, b, nd * sizeof *b) == 0,
                  "a Dll resumed from the blob continues bit-for-bit");
    free (b1);
    free (b2);
    dp_dll_destroy (d);
    dp_dll_destroy (e);
  }
  free (x);
  free (a);
  free (b);
  return 0;
}

/* ---------------------------------------------------------------- *
 * The replica against rectangular chips: rho = 1 - p/4, kappa = 1 - p/3
 *
 * dll_replica_word() holds the code at two points a chip, a quarter and
 * three quarters in, and interpolates linearly between them, so each chip
 * boundary where the code changes sign is a ramp half a chip wide: a
 * trapezoid, not the transmitted rectangle. Over chip phases spread
 * uniformly (any sample rate not locked to the chip rate), with p the
 * fraction of the code's boundaries that change sign, cyclically:
 *
 *   rho   = mean(replica * chip) = 1 - p/4    the signal term
 *   kappa = mean(replica^2)      = 1 - p/3    the noise term
 *
 * and the code-lock statistic, and every correlation through this
 * replica, gives up rho^2/kappa (#3134). The sweep here is M = 256
 * midpoints a chip, on edges every quarter chip: replica * chip is linear
 * between them, so rho is exact; replica^2 is quadratic on the ramps
 * (slope 4 a chip), so kappa reads low by p * 2/(3 M^2) = 1e-5 at most,
 * a tenth of the tolerance.
 *
 * The all-zero code is the control: no boundary changes sign, so the
 * replica IS the rectangle and both read 1. A replica that became
 * rectangular would pass it and fail the other two.
 * ---------------------------------------------------------------- */
static int
replica_terms_follow_transitions (void)
{
  enum
  {
    M = 256 /* phases a chip */
  };
  uint8_t code[1023];
  struct
  {
    const char *name;
    size_t      sf;
  } c[3] = { { "all zero (p = 0)", 31 },
             { "alternating (p = 1)", 64 },
             { "1023 random chips", 1023 } };
  for (int k = 0; k < 3; k++)
    {
      const size_t sf = c[k].sf;
      for (size_t i = 0; i < sf; i++)
        code[i] = k == 0 ? 0u : k == 1 ? (uint8_t)(i & 1u) : 0u;
      if (k == 2)
        make_code (code, sf, 3134u);
      size_t flips = 0;
      for (size_t i = 0; i < sf; i++)
        flips += code[i] != code[(i + 1) % sf];
      const double    p = (double)flips / (double)sf;
      dp_dll_state_t *d
          = dp_dll_create (code, sf, 2, 0.0, 0.01, 0.707, 0.5, 1);
      DP_CHECK (d != NULL);
      if (!d)
        return 1;
      /* A phase word w reads the grid whose point 0 is a quarter chip in
         (dll_tap_base() moves a tap there), so w is chip position
         x = w sf / 2^32 + 1/4, and the transmitted chip is code[floor x]. */
      double       rho = 0.0, kap = 0.0;
      const size_t n = (size_t)M * sf;
      for (size_t j = 0; j < n; j++)
        {
          const double   x = ((double)j + 0.5) / (double)M; /* chips */
          const uint32_t w
              = (uint32_t)llround ((x - 0.25) / (double)sf * 4294967296.0);
          const double r  = dll_replica_word (d, w);
          const size_t ci = (size_t)floor (x) % sf;
          rho += r * dll_chip_sign (code[ci]);
          kap += r * r;
        }
      rho /= (double)n;
      kap /= (double)n;
      printf ("  replica, %s: p %.4f, rho %.5f (1 - p/4 = %.5f), "
              "kappa %.5f (1 - p/3 = %.5f)\n",
              c[k].name, p, rho, 1.0 - p / 4.0, kap, 1.0 - p / 3.0);
      DP_CHECK_NEAR (rho, 1.0 - p / 4.0, 1e-4);
      DP_CHECK_NEAR (kap, 1.0 - p / 3.0, 1e-4);
      if (k == 2)
        DP_CHECK (p > 0.4 && p < 0.6); /* a code, not a corner */
      dp_dll_destroy (d);
    }
  return 0;
}

/* dp_dll_get_window_noise(): the offset tap's estimate on the window sum's
 * scale. Noise alone, of known power a sample (2, per-component sigma 1),
 * so a window sum over sf * sps samples carries sf * sps * 2. Three
 * geometries: one segment (a look an epoch, normalized by the period),
 * four segments (a look a partial), and four with the symbol-period aid
 * (a look sums aid_len partials). Each reads the truth within 10%: the
 * estimate's EMA, booted as a cumulative mean, has a relative sd of 2.2%
 * or less over these looks. The loop is held (bn 1e-9): on noise a
 * running code loop wanders in rate, and a partial then holds other than
 * its nominal samples (measured 0.71 to 0.83 of the truth at bn 0.002);
 * a tracking receiver's loop is locked. Sabotage: drop the division by the
 * look's partials -> the aid case reads aid_len times the truth, red; drop
 * seg_norm -> every case reads 1/seg_norm of it, red. */
static int
_test_window_noise_is_the_window_sums (void)
{
  enum
  {
    SFN = 63,
    SPN = 2,
    PER = 4000
  };
  uint8_t code[SFN];
  make_code (code, SFN, 4242u);
  const size_t     tsamps = (size_t)SFN * SPN;
  float _Complex  *x      = malloc ((size_t)PER * tsamps * sizeof *x);
  float _Complex  *out    = malloc ((size_t)PER * 8 * sizeof *out);
  dp_awgn_state_t *g      = dp_awgn_create (31337u, 1.0f);
  DP_REQUIRE (x && out && g);
  (void)dp_awgn_generate (g, (size_t)PER * tsamps, x, (size_t)PER * tsamps);
  const struct
  {
    size_t      seg;
    double      period;
    const char *name;
  } c[3] = { { 1, 0.0, "one segment" },
             { 4, 0.0, "four segments" },
             { 4, 8.0, "four segments, aid" } };
  for (int k = 0; k < 3; k++)
    {
      dp_dll_state_t *d
          = dp_dll_create (code, SFN, SPN, 0.0, 1e-9, 0.707, 0.5, c[k].seg);
      DP_REQUIRE (d != NULL);
      if (c[k].period > 0.0)
        DP_REQUIRE (dp_dll_set_symbol_period (d, c[k].period) == DP_OK);
      (void)dp_dll_steps (d, x, (size_t)PER * tsamps, out, (size_t)PER * 8);
      const double nw    = dp_dll_get_window_noise (d);
      const double truth = 2.0 * (double)tsamps;
      printf ("  window noise, %s: %.1f against %.1f (L %zu)\n", c[k].name, nw,
              truth, c[k].period > 0.0 ? d->aid_len : (size_t)1);
      DP_CHECK (fabs (nw / truth - 1.0) < 0.10);
      dp_dll_destroy (d);
    }
  dp_awgn_destroy (g);
  free (out);
  free (x);
  return 0;
}

/* The filter's contract (loop_filter_core.h) is a unit-gain actuator: one
 * unit of the integrator moves the code phase by one chip per update. The
 * coherent full-epoch loop applied the integrator as a code-rate ratio, so a
 * unit moved the phase by `sf` chips: an integral path `sf` times the
 * contract's (doppler#3234). Seeded directly, with no error to disturb it,
 * the integrator is steered into the NCO and the phase that increment moves
 * in one period is read back, per code length. Returns the failed checks. */
static int
the_integrator_moves_the_phase_one_chip_per_update (size_t sf)
{
  int          fails = 0;
  const size_t sps   = 4;
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 17u);
  dp_dll_state_t *d
      = dp_dll_create (code, sf, (double)sps, 0.0, 0.002, 0.707, 0.5, 1);
  DP_REQUIRE (code && d);
  const double integ
      = 0.01; /* chips per update, 66 or more LSB of phase_inc */
  const uint32_t nominal = nco_norm_freq_to_inc (d->inv_tsamps);
  d->lf.integ            = integ;
  dll_steer (d, 1.0, 1.0, 1.0); /* e = 0: the integrator alone steers */
  const double dinc  = (double)(int32_t)(d->code_nco.phase_inc - nominal)
                       / 4294967296.0; /* cycles per sample */
  const double chips = dinc * ((double)sf * (double)sps) * (double)sf;
  DP_CHECK_MSG (fabs (chips - integ) <= 0.02 * integ,
                "one unit of the integrator is one chip per update");
  if (fabs (chips - integ) > 0.02 * integ)
    DP_TEST_ERR ("  sf %zu: moved %.6f chips for integ %.6f (the old gain: "
                 "%.6f)\n",
                 sf, chips, integ, integ * (double)sf);
  /* code_rate is the ratio to the nominal rate: integ chips of sf */
  DP_CHECK_MSG (fabs (dp_dll_get_code_rate (d) - (1.0 + integ / (double)sf))
                    < 1e-12,
                "code_rate is 1 + integ / sf");
  dp_dll_destroy (d);
  free (code);
  return fails;
}

/* The gain table is restored verbatim from a blob, so one saved before the
 * integral path became a unit-gain actuator (version 15) would resume the loop
 * sf times too wide, bit-exactly: that blob is refused. Returns failed checks.
 */
static int
a_blob_from_before_the_unit_gain_integral_path_is_refused (void)
{
  int          fails = 0;
  const size_t sf    = 31;
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 5u);
  dp_dll_state_t *a = dp_dll_create (code, sf, 4.0, 0.0, 0.01, 0.707, 0.5, 1);
  dp_dll_state_t *b = dp_dll_create (code, sf, 4.0, 0.0, 0.01, 0.707, 0.5, 1);
  DP_REQUIRE (code && a && b);
  const size_t bytes = dp_dll_state_bytes (a);
  void        *blob  = malloc (bytes);
  DP_REQUIRE (blob);
  dp_dll_get_state (a, blob);
  DP_CHECK (dp_dll_set_state (b, blob) == DP_OK); /* the current version */
  ((dp_state_hdr_t *)blob)->version = 15u;
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_ERR_INVALID,
                "a version 15 blob carries the old gain table");
  free (blob);
  dp_dll_destroy (a);
  dp_dll_destroy (b);
  free (code);
  return fails;
}

/* The code NCO's increment is a 32-bit word and the conversion truncates, so
 * a command under one step (2^-32 cycles per sample) used to be dropped at
 * every steer and the loop read as frozen (doppler#3249). The remainder is
 * carried, so the MEAN increment over many steers is the commanded one. The
 * integrator is seeded so the command is `lsb` steps exactly (the gain table
 * says how many cycles per sample one unit of it is), the error is zero so
 * nothing else moves it, and the mean of the increments is compared with
 * the exact frequency the steer asked for. Without the carry the mean is the
 * truncated nominal, off by the fraction of a step the command and the
 * nominal rate together leave: 0.29 here. Returns the failed checks. */
static int
the_mean_increment_follows_a_command_under_one_step (size_t segments,
                                                     double lsb)
{
  int          fails = 0;
  const size_t sf    = 127;
  const double step  = 1.0 / 4294967296.0; /* one phase-word step, cycles */
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 21u);
  dp_dll_state_t *d
      = dp_dll_create (code, sf, 4.0, 0.0, 0.002, 0.707, 0.5, segments);
  DP_REQUIRE (code && d);
  d->lf.integ      = lsb * step / d->ctrl_i;
  const size_t n   = 4000;
  double       sum = 0.0;
  for (size_t k = 0; k < n; k++)
    {
      dll_steer (d, 1.0, 1.0, 1.0); /* e = 0: the integrator alone steers */
      sum += (double)d->code_nco.phase_inc;
    }
  /* the exact frequency asked for, in steps: nominal plus the command */
  const double want = (d->inv_tsamps + lsb * step) / step;
  const double got  = sum / (double)n;
  /* the carry holds the mean to a step over the number of steers */
  DP_CHECK_MSG (fabs (got - want) <= 2.0 / (double)n,
                "the mean increment is the commanded one");
  if (fabs (got - want) > 2.0 / (double)n)
    DP_TEST_ERR ("  segments %zu, command %+.3f step: mean %.4f, asked %.4f "
                 "(off %.4f steps; the tolerance is %.4f)\n",
                 segments, lsb, got, want, got - want, 2.0 / (double)n);
  dp_dll_destroy (d);
  free (code);
  return fails;
}

/* A held loop follows its held rate to one phase-word step of accumulated
 * phase, however long the hold. The held rate is the integrator's, plus the
 * carrier aid, here chosen so the commanded increment is half a step past a
 * whole word (the worst case for a truncation): converted once at the hold's
 * start the word is half a step short at every sample, which at the
 * certified point (sf 1023, 2 samples per chip) is 0.5 step x 2046 samples x
 * 1023 chips = 2.4e-4 chip per period, 1.5 chips over a 2 s hold's 6,285
 * periods. Converted at every period boundary through the carried remainder
 * (dll_set_inc) the accumulated difference stays within one step, 4.9e-4 chip,
 * at any length. `refreshes` calls dp_dll_set_rate_aid() that many times
 * before each period -- the receivers' carrier aids the code while it coasts
 * -- and `swing` alternates the aid between two commands 0.3 step either side,
 * so the held rate itself moves. Sabotaged: the held branch of dll_steer
 * returns without converting -- the drift reads 1.5 chips at 6,285 periods and
 * 0.024 at 100. */
static int
a_hold_follows_its_rate_to_a_step (size_t periods, int refreshes, int swing)
{
  int          fails = 0;
  const size_t sf    = 1023;
  const double step  = 1.0 / 4294967296.0; /* one phase-word step, cycles */
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 3u);
  dp_dll_state_t *d = dp_dll_create (code, sf, 2.0, 0.0, 0.002, 0.707, 0.5, 1);
  DP_REQUIRE (code && d);
  dp_dll_hold_here (d);
  dp_dll_set_coast (d, 1);
  const double f0      = d->inv_tsamps;
  const double target  = (floor (f0 / step) + 0.5) * step;
  double       applied = 0.0, asked = 0.0;
  for (size_t k = 0; k < periods; k++)
    {
      const double f = target + (swing ? (k & 1u ? 0.3 : -0.3) * step : 0.0);
      for (int r = 0; r < refreshes + 1; r++)
        dp_dll_set_rate_aid (d, f / f0 - 1.0);
      dll_steer (d, 1.0, 1.0, 1.0); /* one period boundary, held */
      applied += (double)d->code_nco.phase_inc;
      asked += d->inv_tsamps * (1.0 + d->rate_aid) / step;
    }
  const double steps = fabs (applied - asked); /* words, accumulated */
  const double chips = steps * step * (double)(sf * 2u) * (double)sf;
  DP_CHECK_MSG (steps <= 1.0 + 1e-6,
                "a held rate is followed to one phase-word step");
  DP_CHECK_MSG (chips < 1e-3, "and so to under a thousandth of a chip");
  printf ("  hold %zu periods, %d refreshes, swing %d: %.3f steps = %.2e "
          "chip\n",
          periods, refreshes, swing, steps, chips);
  dp_dll_destroy (d);
  free (code);
  return fails;
}

/* A blob taken mid-hold resumes bit for bit and passes the remainder's
 * range check: the carry keeps the remainder in [0, one step) in a hold too.
 */
static int
a_blob_taken_mid_hold_resumes (void)
{
  int          fails = 0;
  const size_t sf    = 63;
  const double step  = 1.0 / 4294967296.0;
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 9u);
  dp_dll_state_t *a = dp_dll_create (code, sf, 4.0, 0.0, 0.005, 0.707, 0.5, 1);
  dp_dll_state_t *b = dp_dll_create (code, sf, 4.0, 0.0, 0.005, 0.707, 0.5, 1);
  DP_REQUIRE (code && a && b);
  dp_dll_hold_here (a);
  dp_dll_set_coast (a, 1);
  dp_dll_set_rate_aid (a, 0.37 * step / a->inv_tsamps);
  for (int k = 0; k < 50; k++)
    dll_steer (a, 1.0, 1.0, 1.0);
  DP_CHECK_MSG (a->inc_resid >= 0.0 && a->inc_resid < step,
                "mid-hold the remainder is under one step");
  void *blob = malloc (dp_dll_state_bytes (a));
  DP_REQUIRE (blob);
  dp_dll_get_state (a, blob);
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_OK,
                "a blob taken mid-hold is accepted");
  for (int k = 0; k < 200; k++)
    {
      dll_steer (a, 1.0, 1.0, 1.0);
      dll_steer (b, 1.0, 1.0, 1.0);
      DP_CHECK (a->code_nco.phase_inc == b->code_nco.phase_inc);
      DP_CHECK (a->inc_resid == b->inc_resid);
    }
  free (blob);
  dp_dll_destroy (a);
  dp_dll_destroy (b);
  free (code);
  return fails;
}

/* The remainder's upper edge. A command f in [-2^-53, 0) has floor -1, and
 * under strict IEEE evaluation f - floor(f) rounds up to exactly 1.0, so the
 * remainder is the whole step 2^-32 (gcc, and any build that does not
 * reassociate). A compiler that reassociates under -ffast-math (clang's
 * -O3, which folds a literal command) computes f - (floor(f) + word)
 * instead, the exact remainder, step + f: one 1e-20 below the step for
 * f = -1e-20. Both are in [step - 2^-53, step], and that interval is what
 * every build must produce; the edge itself, exactly one step, the range
 * check must still accept, because an IEEE build writes it. So the pin
 * asserts (1) the produced remainder lies in the interval, (2) its blob is
 * accepted, restores the same remainder and, after one more conversion on
 * each object, leaves both with the same word and remainder, and (3) a
 * forged remainder of exactly one step is accepted and one ulp past it is
 * refused. Sabotaged: the range check's `<=` made `<` -- (3) goes red on
 * every compiler, (1) and (2) on a build that rounds to the edge.
 *
 * The command is fed twice, as a literal and as a volatile. The project
 * builds with -ffast-math, under which a literal can be folded and
 * reassociated (clang's -O3 did, and an `== step` on it failed on macOS and
 * Windows): a pin that asserts exact equality on a folded literal asserts
 * the compiler's evaluation order, not the object. The volatile is the
 * value the loop actually hands the conversion, and both must land in the
 * interval. */
static int
the_remainders_upper_edge_is_produced_and_accepted (void)
{
  int          fails = 0;
  const size_t sf    = 31;
  const double step  = 1.0 / 4294967296.0;
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 7u);
  dp_dll_state_t *a = dp_dll_create (code, sf, 4.0, 0.0, 0.01, 0.707, 0.5, 1);
  dp_dll_state_t *b = dp_dll_create (code, sf, 4.0, 0.0, 0.01, 0.707, 0.5, 1);
  DP_REQUIRE (code && a && b);
  a->inc_resid = 0.0;
  dll_set_inc (a, -1e-20); /* a literal: the compiler may fold it */
  DP_CHECK_MSG (
      a->inc_resid <= step && a->inc_resid >= step - ldexp (1.0, -53),
      "a command just under zero leaves one step, to 2^-53, to carry");
  b->inc_resid            = 0.0;
  volatile double runtime = -1e-20; /* a runtime value: it is not folded */
  dll_set_inc (b, runtime);
  DP_CHECK_MSG (b->inc_resid <= step
                    && b->inc_resid >= step - ldexp (1.0, -53),
                "and so does the same command as a runtime value");
  void *blob = malloc (dp_dll_state_bytes (a));
  DP_REQUIRE (blob);
  dp_dll_get_state (a, blob);
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_OK,
                "the remainder the object produces is accepted");
  DP_CHECK_MSG (b->inc_resid == a->inc_resid, "and restored");
  dll_set_inc (a, 3e-4);
  dll_set_inc (b, 3e-4);
  DP_CHECK_MSG (a->code_nco.phase_inc == b->code_nco.phase_inc
                    && a->inc_resid == b->inc_resid,
                "and the next conversion agrees on both objects");
  dp_dll_state_t *in
      = (dp_dll_state_t *)((char *)blob + sizeof (dp_state_hdr_t));
  in->inc_resid = step; /* what a build that rounds to the edge writes */
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_OK,
                "a remainder of exactly one step is accepted");
  in->inc_resid = nextafter (step, 1.0);
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_ERR_INVALID,
                "one ulp past the edge is refused");
  free (blob);
  dp_dll_destroy (a);
  dp_dll_destroy (b);
  free (code);
  return fails;
}

/* The output bound, dp_dll_steps_max_out(): the capacity one call needs,
 * segments x min(x_len, floor(2 x_len / P) + 1). A one-sample call that
 * completes a carried period at segments 4 emits all four partials, and what
 * a stream emits does not depend on how its calls are cut: fed in random
 * lengths, no call emits more than its bound (each is given the hard ceiling,
 * segments x len, so one that did would show), and the partials are the ones
 * a single call into an ample buffer makes, bit for bit. Sabotaged: the old
 * bound (the call's length, so one slot for one sample) -- the one-sample call
 * drops three partials, and every cut stream differs from the reference; a
 * headroom of 1 in place of 2 -- a cut call emits past its bound. A Debug
 * build asserts at the first refused emission instead, when the bound is
 * what is wrong (#3282).
 */
static int
a_one_sample_call_that_completes_a_carried_period_keeps_every_partial (void)
{
  int          fails = 0;
  const size_t sf = 31, segments = 4;
  const double sps  = 4.0;
  uint8_t     *code = malloc (sf);
  make_code (code, sf, 11u);
  const size_t    p   = (size_t)(sf * sps); /* 124 samples a period */
  float _Complex *x   = malloc (3 * p * sizeof *x);
  uint32_t        rng = 2463534242u;
  for (size_t i = 0; i < 3 * p; i++)
    {
      /* named locals: the order of two draws in one expression is the
         compiler's */
      const float re = (float)((int)(dp_xs32 (&rng) % 2001u) - 1000) / 1000.0f;
      const float im = (float)((int)(dp_xs32 (&rng) % 2001u) - 1000) / 1000.0f;
      x[i]           = re + I * im;
    }
  dp_dll_state_t *d
      = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segments);
  dp_dll_state_t *ref
      = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segments);
  DP_REQUIRE (code && x && d && ref);
  float _Complex got[8], want[8];

  /* P samples close nothing (the wrap is seen a sample after the phase
     passes it); the next one closes the carried period */
  size_t n0 = dp_dll_steps (d, x, p, got, dp_dll_steps_max_out (d, p));
  DP_CHECK_MSG (n0 == 0, "a period's samples close nothing yet");
  size_t cap = dp_dll_steps_max_out (d, 1);
  DP_CHECK_MSG (cap >= segments, "the bound for one sample covers an epoch");
  size_t n1 = dp_dll_steps (d, x + p, 1, got, cap);
  DP_CHECK_MSG (n1 == segments,
                "one sample emits all of the period's partials");
  size_t nr = dp_dll_steps (ref, x, p + 1, want, 8);
  DP_CHECK_MSG (nr == segments, "the reference makes the same period");
  DP_CHECK_MSG (memcmp (got, want, segments * sizeof *got) == 0,
                "and they are the reference's, bit for bit");
  /* a build without assertions keeps the documented stop: a capacity of one
     drops the rest, and writes nothing past it */
#ifdef NDEBUG
  {
    float _Complex canary[3] = { 7.0f, 7.0f, 7.0f };
    dp_dll_state_t *e
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segments);
    DP_REQUIRE (e);
    (void)dp_dll_steps (e, x, p, got, 8);
    size_t k = dp_dll_steps (e, x + p, 1, canary, 1);
    DP_CHECK_MSG (k == 1 && canary[1] == 7.0f && canary[2] == 7.0f,
                  "past its capacity a call stops and writes nothing more");
    dp_dll_destroy (e);
  }
#endif
  dp_dll_destroy (d);
  dp_dll_destroy (ref);
  free (x);
  free (code);
  return fails;
}

static int
random_call_lengths_never_exceed_the_bound (double sps, size_t segments,
                                            double aid)
{
  int          fails = 0;
  const size_t sf    = 31;
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 13u);
  const double    period = (double)sf * sps;
  const size_t    total  = (size_t)(24.0 * period);
  float _Complex *x      = malloc (total * sizeof *x);
  uint32_t        rng    = 88172645u;
  for (size_t i = 0; i < total; i++)
    {
      /* named locals: the order of two draws in one expression is the
         compiler's */
      const float re = (float)((int)(dp_xs32 (&rng) % 2001u) - 1000) / 1000.0f;
      const float im = (float)((int)(dp_xs32 (&rng) % 2001u) - 1000) / 1000.0f;
      x[i]           = re + I * im;
    }
  dp_dll_state_t *d
      = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segments);
  dp_dll_state_t *ref
      = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, segments);
  DP_REQUIRE (code && x && d && ref);
  dp_dll_set_rate_aid (d, aid);
  dp_dll_set_rate_aid (ref, aid);
  const size_t    refcap = dp_dll_steps_max_out (ref, total);
  float _Complex *want   = malloc (refcap * sizeof *want);
  /* Each cut call gets the hard ceiling, segments x len, as its max_out, so
     a call that emits more than its bound is not stopped at the bound and
     the check below can see it. */
  float _Complex *got = malloc (segments * total * sizeof *got);
  DP_REQUIRE (want && got);
  const size_t nwant = dp_dll_steps (ref, x, total, want, refcap);
  size_t       ngot = 0, pos = 0;
  while (pos < total)
    {
      size_t len = 1 + dp_xs32 (&rng) % (size_t)(3.0 * period);
      if (len > total - pos)
        len = total - pos;
      const size_t bound = dp_dll_steps_max_out (d, len);
      const size_t k
          = dp_dll_steps (d, x + pos, len, got + ngot, segments * len);
      if (k > bound)
        fails++;
      ngot += k;
      pos += len;
    }
  DP_CHECK_MSG (fails == 0, "a call never emits more than its bound");
  DP_CHECK_MSG (ngot == nwant && memcmp (got, want, nwant * sizeof *want) == 0,
                "cut at random, the partials are the one-call partials");
  if (ngot != nwant)
    DP_TEST_ERR ("  sps %.4f, segments %zu, aid %.2f: %zu partials against "
                 "%zu\n",
                 sps, segments, aid, ngot, nwant);
  dp_dll_destroy (d);
  dp_dll_destroy (ref);
  free (want);
  free (got);
  free (x);
  free (code);
  return fails;
}

/* A blob from before the remainder was carried (version 16) is refused, and
 * so is a carried remainder no conversion could have dropped: it is added to
 * the next steer's frequency, so a forged value is a forged rate. */
static int
the_carried_remainder_is_in_the_blob_and_checked (void)
{
  int          fails = 0;
  const size_t sf    = 31;
  uint8_t     *code  = malloc (sf);
  make_code (code, sf, 5u);
  dp_dll_state_t *a = dp_dll_create (code, sf, 4.0, 0.0, 0.01, 0.707, 0.5, 1);
  dp_dll_state_t *b = dp_dll_create (code, sf, 4.0, 0.0, 0.01, 0.707, 0.5, 1);
  DP_REQUIRE (code && a && b);
  a->lf.integ = 0.3 * (1.0 / 4294967296.0) / a->ctrl_i; /* 0.3 of a step */
  dll_steer (a, 1.0, 1.0, 1.0);
  DP_CHECK_MSG (a->inc_resid > 0.0, "the steer left a remainder to carry");
  const size_t bytes = dp_dll_state_bytes (a);
  void        *blob  = malloc (bytes);
  DP_REQUIRE (blob);
  dp_dll_get_state (a, blob);
  DP_CHECK (dp_dll_set_state (b, blob) == DP_OK);
  DP_CHECK_MSG (b->inc_resid == a->inc_resid,
                "the remainder resumes bit for bit");
  dp_dll_state_t *in
      = (dp_dll_state_t *)((char *)blob + sizeof (dp_state_hdr_t));
  const double keep = in->inc_resid;
  in->inc_resid     = 2.0 / 4294967296.0; /* more than one step */
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_ERR_INVALID,
                "a remainder over one step is refused");
  in->inc_resid = -1e-12;
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_ERR_INVALID,
                "a negative remainder is refused");
  in->inc_resid                     = keep;
  ((dp_state_hdr_t *)blob)->version = 16u;
  DP_CHECK_MSG (dp_dll_set_state (b, blob) == DP_ERR_INVALID,
                "a version 16 blob has no remainder");
  free (blob);
  dp_dll_destroy (a);
  dp_dll_destroy (b);
  free (code);
  return fails;
}

/* The Debug assert (#3282) holds the bound to its headroom, so a rate the
 * bound does not assume must not trip it. An aided Dll (segments 4, a symbol
 * period set, so aid_look() steers once a symbol) at a rate aid of 4 runs
 * its code at five times nominal, past the headroom of two: each period's
 * call emits more than the bound, and the drop is the price the header names.
 * Fed into buffers of exactly the bound, the call returns at most that many
 * and a Debug build does not abort. Sabotaged (the build of #3385's coverage
 * run): the re-read after aid_look()'s steer removed, the rate reads as the
 * nominal it was at entry, and a Debug build aborts on the first period. A
 * Release build compiles the check away, so the failing case is the Debug
 * one: the ASan and UBSan jobs. */
static int
a_rate_past_the_headroom_drops_without_aborting (void)
{
  int          fails = 0, full = 0;
  const size_t sf = 31, segments = 4;
  const double sps  = 2.0;
  uint8_t     *code = malloc (sf);
  make_code (code, sf, 17u);
  const size_t    n   = 62 * 6;
  float _Complex *x   = malloc (n * sizeof *x);
  uint32_t        rng = 2463534242u;
  for (size_t i = 0; i < n; i++)
    {
      const float re = (float)((int)(dp_xs32 (&rng) % 2001u) - 1000) / 1000.0f;
      const float im = (float)((int)(dp_xs32 (&rng) % 2001u) - 1000) / 1000.0f;
      x[i]           = re + I * im;
    }
  dp_dll_state_t *d
      = dp_dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, segments);
  DP_REQUIRE (code && x && d);
  DP_REQUIRE (dp_dll_set_symbol_period (d, 13.5) == DP_OK);
  dp_dll_set_rate_aid (d, 4.0);
  for (size_t pos = 0; pos < n; pos += 62)
    {
      const size_t    cap = dp_dll_steps_max_out (d, 62);
      float _Complex *out = malloc (cap * sizeof *out);
      DP_REQUIRE (out);
      /* a call that returns its whole capacity stopped there: the scene's
         premise, a drop past the headroom, actually happens */
      if (dp_dll_steps (d, x + pos, 62, out, cap) == cap)
        full++;
      free (out);
    }
  /* the aid reaches the NCO: the code runs past twice nominal, which holds
     in Release too */
  const double rate
      = (double)d->code_nco.phase_inc / 4294967296.0 * ((double)sf * sps);
  if (!(rate > 2.0))
    fails++;
  DP_CHECK_MSG (rate > 2.0, "the aid puts the code past twice nominal");
  DP_CHECK_MSG (full > 0, "past the headroom, a call stops at its capacity "
                          "(and a Debug build does not abort)");
  if (!full)
    fails++;
  dp_dll_destroy (d);
  free (x);
  free (code);
  return fails;
}

int
main (void)
{
  DP_CHECK (replica_terms_follow_transitions () == 0);
  DP_CHECK (_test_window_noise_is_the_window_sums () == 0);
  DP_CHECK (_test_window_sum_is_the_look_back () == 0);
  DP_CHECK (_test_block_correlate_is_the_per_sample_path () == 0);

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, NULL-code guard, init==create parity               *
   * ---------------------------------------------------------------- */
  {
    DP_CHECK (dp_dll_create (NULL, 0, 2, 0.0, 0.01, 0.707, 0.5, 1) == NULL);

    uint8_t code[31];
    make_code (code, 31, 1u);
    dp_dll_state_t *c = dp_dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5, 1);
    DP_CHECK (c != NULL);
    if (!c)
      return 1;
    /* The code phase predicted n samples ahead is the phase after n
       samples. Zero input steers nothing (a zero discriminator), so the
       rate holds through the updates the samples cross. */
    {
      const double now      = dp_dll_get_code_phase (c);
      const double ahead    = dp_dll_get_code_phase_ahead (c, 100);
      float _Complex z[100] = { 0 };
      float _Complex o[8];
      DP_CHECK (dp_dll_get_code_phase_ahead (c, 0) == now);
      (void)dp_dll_steps (c, z, 100, o, 8);
      DP_CHECK_NEAR (dp_dll_get_code_phase (c), ahead, 1e-9);
      DP_CHECK (fabs (ahead - now) > 1.0); /* it did move */
    }
    /* The lock rule reads back the verify counts set_lock_verify set,
       keeping the levels configure_lock derived. */
    {
      const dp_lockdet_rule_t r0 = dp_dll_get_lock_rule (c);
      DP_CHECK (dp_dll_set_lock_verify (c, 4, 7) == DP_OK);
      const dp_lockdet_rule_t r = dp_dll_get_lock_rule (c);
      DP_CHECK (r.n_up == 4 && r.n_down == 7);
      DP_CHECK (r.up_thresh == r0.up_thresh && r.up_thresh > 0.0);
      DP_CHECK (r.down_thresh == r0.down_thresh);
    }
    DP_CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    DP_CHECK (c->sf == 31 && c->sps == 2);
    DP_CHECK (dp_dll_get_code_rate (c) == 1.0);

    dp_dll_state_t v;
    dp_dll_init (&v, code, 31, 2, 0.0, 0.02, 0.707, 0.5);
    DP_CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    DP_CHECK (v.owns_code == 0);  /* init borrows */
    DP_CHECK (c->owns_code == 1); /* create copies */
    dp_dll_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 1b. dp_dll_init defends against a dirty caller stack                 *
   *                                                                  *
   * dp_dll_init() does an in-place init of a caller-owned (often stack) *
   * dp_dll_state_t, so every field it does not explicitly set starts as *
   * whatever garbage the caller's memory held. rate_aid (carrier-    *
   * aiding config, set only by dp_dll_set_rate_aid) was the one field   *
   * the zero-against-garbage block missed: on a clean stack it read  *
   * 0 (Linux), but a NaN there (0xFF-filled stack, seen on macOS)    *
   * made phase_inc = nco_norm_freq_to_inc(inv_tsamps*(1+NaN)+ctrl) cast   *
   * to 0, freezing the code NCO permanently (validate_dll_jitter     *
   * #82). Poison the whole struct, init, and assert the loop still   *
   * steers a live, wrapping NCO.                                     *
   * ---------------------------------------------------------------- */
  {
    uint8_t code[31];
    make_code (code, 31, 1u);
    dp_dll_state_t g;
    memset (&g, 0xFF, sizeof g); /* 0xFF doubles are NaN — the macOS case */
    dp_dll_init (&g, code, 31, 2, 0.0, 0.002, 0.707, 0.5);
    DP_CHECK (g.rate_aid == 0.0);         /* zeroed, not NaN */
    DP_CHECK (g.code_nco.phase_inc > 0u); /* NCO not frozen */
    uint32_t inc0 = g.code_nco.phase_inc;
    /* run a clean aligned epoch; the NCO must keep a sane (nonzero, ~nominal)
       phase_inc rather than collapsing to 0. */
    float _Complex sig[31 * 2];
    for (size_t ci = 0; ci < 31; ci++)
      {
        float vv    = (code[ci] & 1u) ? -1.0f : 1.0f;
        sig[ci * 2] = sig[ci * 2 + 1] = vv;
      }
    int wraps = 0;
    for (int ep = 0; ep < 4; ep++)
      for (size_t i = 0; i < 31 * 2; i++)
        if (dll_accumulate (&g, sig[i]))
          {
            dll_update (&g);
            g.acc_e = g.acc_p = g.acc_l = 0.0f;
            wraps++;
          }
    DP_CHECK (wraps >= 3); /* keeps wrapping ~once/epoch */
    DP_CHECK (g.code_nco.phase_inc
              > inc0 / 2u); /* still near nominal, not 0 */
  }

  /* ---------------------------------------------------------------- *
   * 2. On-time alignment — E ~ L, discriminator ~0, code_rate ~1     *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 400;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 7u);
    float _Complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t          n  = make_signal (rx, code, sf, sps, 0.0, nper, 3u, 0);

    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    size_t          k   = dp_dll_steps (d, rx, n, sym, nper);
    DP_CHECK (k >= nper - 2 && k <= nper); /* ~one prompt per period */
    DP_CHECK (fabs (dp_dll_get_last_error (d)) < 0.05); /* E ~ L on-time */
    DP_CHECK (fabs (dp_dll_get_code_rate (d) - 1.0) < 1e-3);
    dp_dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 3. Code Doppler — code_rate converges to the incoming chip rate  *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 1500;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    float _Complex *rx    = malloc (sf * sps * nper * sizeof (*rx));
    double          delta = 5e-4; /* incoming code runs 0.05% fast */
    size_t          n = make_signal (rx, code, sf, sps, delta, nper, 9u, 1);

    /* The loop must pull in a code Doppler of 5e-4: 0.0315 chips per period
       of ramp. A second-order loop's transient error to a rate step is about
       the rate over its natural frequency, wn = 8 zeta bn / (4 zeta^2 + 1);
       to stay inside the discriminator's linear range (about a chip) wn is at
       least 0.03 per period, so bn is 0.01 or more, and 0.05 gives margin. */
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.05, 0.707, 0.5, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    size_t          k   = dp_dll_steps (d, rx, n, sym, nper);
    /* the loop must speed its replica up to match the incoming rate */
    DP_CHECK (fabs (dp_dll_get_code_rate (d) - (1.0 + delta)) < 1e-4);
    /* sub-chip lock holds: the prompt despreads cleanly over the run tail
       (mean |Re prompt| well above 0; the code-phase tracking follows the
       sliding code phase without the integer-sample staircase). Threshold
       0.85, not the pre-redesign 0.9: the 2-samples/chip interpolated
       replica (docs/design/async-dsss-receiver.md §3.6) is a
       simpler point-sample model than the old dwell-width-aware exact
       matched-filter integral it replaced, and costs a small, expected
       amount of despread gain (observed ~0.87 here; ~0.88 in the Python
       prototype that validated this redesign) -- not a regression. */
    size_t lo = k / 2;
    double pm = 0.0;
    for (size_t j = lo; j < k; j++)
      pm += fabs (crealf (sym[j]));
    DP_CHECK (k > lo && pm / (double)(k - lo) > 0.85);
    dp_dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 4. Static phase offset is pulled in (discriminator decays)       *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 800;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 13u);
    float _Complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t          n  = make_signal (rx, code, sf, sps, 0.0, nper, 17u, 0);

    /* seed the replica 0.4 chips off — the loop must realign it */
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.4, 0.005, 0.707, 0.5, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    /* early discriminator (first few periods) should be non-trivial */
    dp_dll_steps (d, rx, sf * sps * 3, sym, 3);
    double early_err = fabs (dp_dll_get_last_error (d));
    dp_dll_steps (d, rx + sf * sps * 3, n - sf * sps * 3, sym, nper);
    double late_err = fabs (dp_dll_get_last_error (d));
    DP_CHECK (early_err > 0.05); /* started misaligned */
    DP_CHECK (late_err < 0.05);  /* pulled in */
    dp_dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 5. Reset reproducibility                                         *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 31, sps = 2, nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 21u);
    float _Complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t          n  = make_signal (rx, code, sf, sps, 3e-4, nper, 5u, 0);

    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    dp_dll_steps (d, rx, n, sym, nper);
    double r1 = dp_dll_get_code_rate (d), e1 = dp_dll_get_last_error (d);
    dp_dll_reset (d);
    dp_dll_steps (d, rx, n, sym, nper);
    double r2 = dp_dll_get_code_rate (d), e2 = dp_dll_get_last_error (d);
    DP_CHECK (r1 == r2 && e1 == e2);
    dp_dll_destroy (d);

    /* segments > 1: reset clears the output's level too (doppler#2495),
       so the same input after a reset gives a fresh Dll's output, bit for
       bit. A level carried over would scale the first periods by the
       old input's. */
    const size_t    segs = 4, cap = segs * (nper + 1);
    float _Complex *o1 = malloc (cap * sizeof *o1);
    float _Complex *o2 = malloc (cap * sizeof *o2);
    dp_dll_state_t *f
        = dp_dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, segs);
    dp_dll_state_t *g
        = dp_dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, segs);
    for (size_t i = 0; i < n; i++)
      rx[i] *= 8.0f; /* a level the fresh one never saw */
    (void)dp_dll_steps (f, rx, n, o1, cap);
    for (size_t i = 0; i < n; i++)
      rx[i] /= 8.0f;
    dp_dll_reset (f);
    size_t k1 = dp_dll_steps (f, rx, n, o1, cap);
    size_t k2 = dp_dll_steps (g, rx, n, o2, cap);
    DP_CHECK_MSG (k1 == k2 && k1 > 0 && memcmp (o1, o2, k1 * sizeof *o1) == 0,
                  "segments > 1: after reset, a fresh Dll's output");
    dp_dll_destroy (f);
    dp_dll_destroy (g);
    free (o1);
    free (o2);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6. segments > 1: sub-epoch partials recover an async symbol clock *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, K = 4, nsym = 2000;
    size_t       te   = sf * sps;
    double       dsym = 3e-3, tsym = (double)te * (1.0 + dsym);
    double       phi  = 0.37 * (double)te;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    size_t          N    = (size_t)(nsym * tsym) + 2 * te;
    float _Complex *rx   = malloc (N * sizeof (*rx));
    float _Complex *out  = malloc (N * sizeof (*out));
    int            *data = malloc ((nsym + 8) * sizeof (int));
    uint32_t        ds   = 7u;
    for (size_t i = 0; i < nsym + 6; i++)
      {
        data[i] = (dp_xs32 (&ds) & 1u) ? 1 : -1;
      }
    for (size_t nn = 0; nn < N; nn++)
      {
        long s = (long)floor (((double)nn - phi) / tsym);
        if (s < 0)
          s = 0;
        if (s >= (long)nsym + 6)
          s = nsym + 5;
        size_t ci = (nn / sps) % sf; /* code-aligned (no code Doppler) */
        float  cs = (code[ci] & 1u) ? -1.0f : 1.0f;
        rx[nn]    = (float)data[s] * cs;
      }
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    size_t np  = dp_dll_steps (d, rx, N, out, N);
    size_t nep = N / te;
    DP_CHECK (dp_dll_get_segments (d) == K);
    DP_CHECK (np >= (nep - 1) * K && np <= (nep + 1) * K);
    /* genie symbol despread on the partials (known timing) recovers the data
     */
    double *acc = calloc (nsym + 8, sizeof (double));
    for (size_t pp = 0; pp < np; pp++)
      {
        double t = (double)te * ((double)pp + 0.5) / (double)K;
        long   s = (long)floor ((t - phi) / tsym);
        if (s >= 0 && s < (long)nsym)
          acc[s] += creal (out[pp]);
      }
    long err = 0;
    for (size_t s = 2; s < nsym - 2; s++)
      if ((acc[s] >= 0 ? 1 : -1) != data[s])
        err++;
    DP_CHECK (err == 0); /* partials recover the asynchronous data */
    /* No code Doppler here (ci is code-aligned above): a well-behaved loop
       should settle near code_rate=1 with a small last_error, not pinned at
       DLL_DISC_CLAMP every epoch (a real bug -- the segments>1 discriminator
       once mixed a tsamps-normalised pp against raw-scale ep/lp, off by
       roughly tsamps^2, so it saturated on essentially every epoch). */
    DP_CHECK (fabs (dp_dll_get_code_rate (d) - 1.0) < 1e-3);
    DP_CHECK (fabs (dp_dll_get_last_error (d)) < 0.5);
    dp_dll_destroy (d);
    free (acc);
    free (rx);
    free (out);
    free (data);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6b. Symbol-period aid: the lock detector's looks become coherent  *
   *     over a symbol, and lock at a C/N0 where per-partial looks     *
   *     cannot (docs/design/async-dsss-receiver.md §3.7, and          *
   *     docs/design/async-dsss-receiver-measurements.md §12.3)        *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, K = 4, nsym = 1500;
    const size_t te   = sf * sps;
    const double P    = 7.24; /* partials per symbol: 1.81 epochs (5 Mcps) */
    const double tsym = P * (double)te / (double)K;
    const double phi  = 0.37 * (double)te; /* symbol clock phase, samples */
    /* Per-sample amplitude SNR chosen so ONE partial (te/K samples
       coherent) is ~-3 dB per look -- the default 20-look detector's
       statistic then sits under its threshold -- while a six-partial
       symbol window is ~+5 dB per look. */
    const double a    = sqrt (0.5 / (double)(te / K));
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    size_t          N   = (size_t)(nsym * tsym) + 2 * te;
    float _Complex *rx  = malloc (N * sizeof (*rx));
    float _Complex *out = malloc (N * sizeof (*out));
    make_async_signal (rx, N, code, sf, sps, 0.0, tsym, phi, a, 7u, 99u);
    /* Feed one epoch per call and count the epochs the flag was up. */
    size_t          nep = N / te;
    size_t          up0 = 0, up1 = 0;
    dp_dll_state_t *d0
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    dp_dll_state_t *d1
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    DP_CHECK (dp_dll_get_symbol_window (d1) == 0);
    DP_CHECK (dp_dll_set_symbol_period (d1, 1.5) == DP_ERR_INVALID);
    DP_CHECK (dp_dll_set_symbol_period (d1, P) == DP_OK);
    DP_CHECK (dp_dll_get_symbol_window (d1) == 6); /* floor(7.24) - 1 */
    /* Sized the way the header says: the window's coherent length in
       samples at this per-sample SNR. */
    int nl = dp_det_n_noncoh (a, (int)(6 * (te / K)), 0.99, 1e-3, 4000);
    DP_CHECK (nl >= 1 && nl < 100);
    DP_CHECK (dp_dll_configure_lock (d1, 1e-3, (size_t)(nl > 0 ? nl : 1), 0.0)
              == DP_OK);
    for (size_t e = 0; e < nep; e++)
      {
        dp_dll_steps (d0, rx + e * te, te, out, te);
        dp_dll_steps (d1, rx + e * te, te, out, te);
        if (e >= nep / 4) /* after the loops and references have settled */
          {
            up0 += (size_t)dp_dll_get_locked (d0);
            up1 += (size_t)dp_dll_get_locked (d1);
          }
      }
    double f0 = (double)up0 / (double)(nep - nep / 4);
    double f1 = (double)up1 / (double)(nep - nep / 4);
    printf ("  6b: per-partial looks up %.1f%%, symbol-aided up %.1f%% "
            "(N=%d, best phase %zu)\n",
            100.0 * f0, 100.0 * f1, nl, d1->aid_best);
    DP_CHECK (f0 < 0.5);  /* the default detector cannot hold this C/N0 */
    DP_CHECK (f1 > 0.95); /* the aided one does */
    /* The chosen hypothesis IS the symbol timing: its boundaries fall within
       one partial of the true ones (circular over the period). */
    double h_true = fmod (phi / ((double)te / (double)K), P);
    double dist   = fabs ((double)d1->aid_best - h_true);
    dist          = fmod (dist, P);
    if (dist > P - dist)
      dist = P - dist;
    DP_CHECK (dist <= 1.0);
    /* Both loops tracked the code regardless of the flag. */
    DP_CHECK (fabs (dp_dll_get_code_rate (d0) - 1.0) < 1e-3);
    DP_CHECK (fabs (dp_dll_get_code_rate (d1) - 1.0) < 1e-3);
    /* Serialization: the rings and hypotheses ride the blob; a blob from an
       instance without the aid is refused by one with it. */
    {
      dp_dll_state_t *b
          = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
      DP_CHECK (dp_dll_set_symbol_period (b, P) == DP_OK);
      DP_STATE_ROUNDTRIP_TEST (dp_dll, d1, b);
      size_t cb   = dp_dll_state_bytes (d0);
      void  *blob = malloc (cb);
      dp_dll_get_state (d0, blob);
      DP_CHECK (dp_dll_set_state (b, blob) == DP_ERR_INVALID);
      free (blob);
      dp_dll_destroy (b);
    }
    /* Verify counts: set, kept with the thresholds, and refused at 0. */
    {
      double up = d1->lock.up_thresh, down = d1->lock.down_thresh;
      DP_CHECK (dp_dll_set_lock_verify (d1, 2, 3) == DP_OK);
      DP_CHECK (d1->lock.n_up == 2 && d1->lock.n_down == 3);
      DP_CHECK (d1->lock.up_thresh == up && d1->lock.down_thresh == down);
      DP_CHECK (dp_dll_set_lock_verify (d1, 0, 3) == DP_ERR_INVALID);
    }
    /* Off again: per-partial looks, no window. */
    DP_CHECK (dp_dll_set_symbol_period (d1, 0.0) == DP_OK);
    DP_CHECK (dp_dll_get_symbol_window (d1) == 0);
    dp_dll_destroy (d0);
    dp_dll_destroy (d1);
    free (rx);
    free (out);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6b'. Symbol-period aid on NOISE: the detector's per-decision      *
   *      exceedance rate is its configured pfa, and it declares       *
   *      nothing (#1264)                                              *
   * ---------------------------------------------------------------- */
  {
    /* 6b's geometry and sizing, pure noise in. On noise the best timing
       hypothesis flips between neighbours whose windows share all but
       one partial; a decision whose n_looks were taken through
       overlapping windows read the same noise n times against a
       threshold sized for n independent looks -- measured 1.7e-2 per
       decision for a configured 1e-3, a false lock every one to two
       seconds, before a window overlapping the last look's stopped
       counting as a look. Sabotage: drop the aid_last_end guard in
       aid_look() -> red on both checks. */
    const size_t sf = 63, sps = 4, K = 4;
    const size_t te = sf * sps;
    const double P = 7.24, pfa = 1e-3;
    const double a = 0.16; /* per-sample amplitude: about three looks, the
                              receiver's own regime at 45 dB-Hz */
    uint8_t *code = malloc (sf);
    uint32_t cst  = 7u;
    for (size_t i = 0; i < sf; i++)
      code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 1u : 0u);
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    DP_CHECK (dp_dll_set_symbol_period (d, P) == DP_OK);
    int nl = dp_det_n_noncoh (a, (int)(6 * (te / K)), 0.99, pfa, 4000);
    DP_CHECK (nl >= 1);
    DP_CHECK (dp_dll_configure_lock (d, pfa, (size_t)nl, 0.0) == DP_OK);
    const size_t    n_ep = 40000; /* ~1.5e5 partials, thousands of looks */
    float _Complex *x    = malloc (te * sizeof *x);
    float _Complex *out  = malloc (te * sizeof *out);
    uint32_t        nst  = 0x9e3779b9u;
    size_t          dec = 0, exceed = 0, locks = 0;
    double          last = -1.0;
    int             prev = 0;
    for (size_t e = 0; e < n_ep; e++)
      {
        for (size_t i = 0; i < te; i++)
          x[i] = dp_cgauss (&nst);
        for (size_t q = 0; q < K; q++)
          {
            dp_dll_steps (d, x + q * (te / K), te / K, out, te);
            if (d->lock_stat != last && d->lock_stat > 0.0)
              {
                dec++;
                exceed += d->lock_stat > d->lock.up_thresh;
                last = d->lock_stat;
              }
            int lk = dp_dll_get_locked (d);
            locks += lk && !prev;
            prev = lk;
          }
      }
    printf ("  6b': aided detector on noise, N=%d: %zu decisions, %zu over "
            "the gate (%.2e per decision for pfa %.0e), %zu lock(s)\n",
            nl, dec, exceed, (double)exceed / (double)dec, pfa, locks);
    DP_CHECK (dec > 1000);
    /* Within a few times the configured pfa (the unaided detector's own
       realized rate is about twice it), and no false lock. */
    DP_CHECK ((double)exceed <= 4.0 * pfa * (double)dec);
    DP_CHECK (locks == 0);
    free (out);
    free (x);
    dp_dll_destroy (d);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6c. Symbol-period aid: the code loop steers on the aided window,  *
   *     once per symbol, with bn kept per epoch                       *
   *     (docs/design/async-dsss-receiver.md §3.7, and                 *
   *     docs/design/async-dsss-receiver-measurements.md §12.5)        *
   * ---------------------------------------------------------------- */
  {
    /* 6b's asynchronous data stream (7.24 partials per symbol at four
       partials per epoch), clean, with the incoming code running D0 fast:
       the code-rate step a type-2 loop nulls only by integrating it. */
    const size_t sf = 63, sps = 4, K = 4;
    const size_t te = sf * sps, nep = 1500, post = 100;
    const double P = 7.24, tsym = P * (double)te / (double)K;
    const double phi = 0.37 * (double)te;
    const double d0  = 1e-4; /* the incoming code runs 100 ppm fast:
                                what this loop (K0 = 1/sps chips per unit
                                of filter output) pulls in; 500 ppm slips
                                the code out of the lobe in either mode */
    const double bn   = 0.005;
    const size_t mid  = 300; /* inside the settling: 5/bn is 1000 epochs */
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    size_t          N   = (nep + post) * te;
    float _Complex *rx  = malloc (N * sizeof (*rx));
    float _Complex *out = malloc (te * sizeof (*out));
    make_async_signal (rx, N, code, sf, sps, d0, tsym, phi, 0.0, 7u, 0u);
    dp_dll_state_t *d = dp_dll_create (code, sf, sps, 0.0, bn, 0.707, 0.5, K);
    dp_dll_state_t *d0_
        = dp_dll_create (code, sf, sps, 0.0, bn, 0.707, 0.5, K);
    DP_CHECK (dp_dll_set_symbol_period (d, P) == DP_OK);
    /* The filter is re-timed to the symbol: P/K epochs per update. */
    DP_CHECK (fabs (d->lf.t - P / (double)K) < 1e-12);
    /* The gain pin: a code-rate step is nulled by any gain, but HOW FAST
       the integrator climbs is the gain -- so mid-transient the aided
       loop must read what the per-epoch loop reads. A filter left at its
       per-epoch gains while updating once per symbol climbs 1.8x slower
       and fails this; the two agree to six decimals when re-timed. */
    for (size_t e = 0; e < mid; e++)
      {
        dp_dll_steps (d, rx + e * te, te, out, te);
        dp_dll_steps (d0_, rx + e * te, te, out, te);
      }
    double r_mid  = dp_dll_get_code_rate (d),
           r_mid0 = dp_dll_get_code_rate (d0_);
    /* Mid-transient, not settled: the per-epoch loop is still well off
       the step (it overshoots to ~1.3x here at zeta 0.707). */
    DP_CHECK (fabs (r_mid0 - 1.0 - d0) > 0.1 * d0);
    DP_CHECK (fabs (r_mid - r_mid0) < 0.15 * d0);
    for (size_t e = mid; e < nep; e++)
      dp_dll_steps (d, rx + e * te, te, out, te);
    dp_dll_destroy (d0_);
    /* The aided loop pulled the code rate in: the discriminator on the
       window steered it (a loop that never steered would sit at 1.0). */
    double r_aided = dp_dll_get_code_rate (d);
    DP_CHECK (fabs (r_aided - (1.0 + d0)) < 4e-5);
    DP_CHECK (fabs (dp_dll_get_last_error (d)) < 0.5);
    /* Off again: the filter is back to one update per epoch, and the rate
       the loop had pulled in is CONTINUOUS across the switch -- after a
       few epochs (short against the 5/bn settling) it still reads the
       Doppler, not the Doppler scaled by the old interval. */
    DP_CHECK (dp_dll_set_symbol_period (d, 0.0) == DP_OK);
    DP_CHECK (d->lf.t == 1.0);
    for (size_t e = nep; e < nep + post; e++)
      dp_dll_steps (d, rx + e * te, te, out, te);
    double r_after = dp_dll_get_code_rate (d);
    printf ("  6c: code rate at %zu epochs per-epoch %.7f aided %.7f; "
            "settled %.7f; %zu epochs after the switch %.7f (incoming "
            "%.7f)\n",
            mid, r_mid0, r_mid, r_aided, post, r_after, 1.0 + d0);
    DP_CHECK (fabs (r_after - r_aided) < 4e-5);
    dp_dll_destroy (d);
    free (rx);
    free (out);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6d. segments > 1: the discriminator's sums are coherent over its *
   *     one-period window, so a residual carrier of one whole cycle  *
   *     per period nulls it, and half a cycle does not               *
   * ---------------------------------------------------------------- */
  {
    /* dp_dll_steps() promises: blind to the carrier's phase, not to its
       frequency; a residual df costs a window of T seconds sinc^2(df*T) of
       its power, and a whole 1/T cycle nulls it. Each partial spans a
       quarter of that cycle, so a loop that combined its partials
       non-coherently (sum|E_k|) would hold the code at one cycle too. A
       held code (no Doppler, constant data, seeded on the code) isolates
       the window: at 0 and half a cycle per period the loop stays on the
       code; at one whole cycle it walks off it. */
    const size_t sf = 63, sps = 4, K = 4, nper = 500;
    const size_t te   = sf * sps;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    float _Complex *clean = malloc (te * nper * sizeof *clean);
    float _Complex *rx    = malloc (te * nper * sizeof *rx);
    float _Complex *out   = malloc (te * nper * sizeof *out);
    size_t          n = make_signal (clean, code, sf, sps, 0.0, nper, 3u, 1);
    const double    cycles[3] = { 0.0, 0.5, 1.0 }; /* per code period */
    double          off[3];
    for (int c = 0; c < 3; c++)
      {
        for (size_t i = 0; i < n; i++)
          rx[i] = clean[i]
                  * (float _Complex)cexp (I * 2.0 * M_PI * cycles[c]
                                          * (double)i / (double)te);
        dp_dll_state_t *d
            = dp_dll_create (code, sf, sps, 0.0, 0.005, 0.707, 0.5, K);
        dp_dll_steps (d, rx, n, out, n);
        /* The incoming code is at phase 0 at every period boundary; fold
           the loop's phase to the nearest, signed. */
        double ph = dp_dll_get_code_phase (d);
        off[c]    = ph > 0.5 * (double)sf ? ph - (double)sf : ph;
        dp_dll_destroy (d);
      }
    printf ("  6d: code phase off after %zu periods, residual 0 / 0.5 / 1 "
            "cycle per period: %+.3f / %+.3f / %+.3f chips\n",
            nper, off[0], off[1], off[2]);
    DP_CHECK (fabs (off[0]) < 0.05); /* no residual: on the code */
    DP_CHECK (fabs (off[1]) < 0.05); /* half a cycle: -3.9 dB, still on */
    DP_CHECK (fabs (off[2]) > 0.5);  /* a whole cycle nulls the window */
    free (out);
    free (rx);
    free (clean);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6e. A period that is not a whole number of samples               *
   * ---------------------------------------------------------------- *
   * 11 MSa/s over 5 Mcps is 2.2 samples a chip. The partials close on the
   * code NCO's phase and the taps sit at each sample's dwell centre, so
   * the loop tracks there exactly as at an integer rate. The stimulus is
   * each sample's dwell-average of the chip waveform -- what a
   * band-limited front end delivers, and the model the taps assume; a
   * chip HELD from the dwell's start agrees with it only when no dwell
   * straddles a boundary, which is the integer case. After 3,000 periods
   * (415,800 samples, the next sample's dwell starting back at phase 0)
   * the loop reads phase 0 to well inside 0.01 chip (measured: -0.0011),
   * locked. Sabotage, red: truncate sps to an integer in
   * configure_geometry. */
  {
    const size_t sf = 63, K = 4, nper = 3000;
    const double sps  = 2.2;
    const size_t n    = (size_t)llround ((double)(sf * nper) * sps);
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 23u);
    float _Complex *rx  = malloc (n * sizeof *rx);
    float _Complex *out = malloc (n * sizeof *out);
    for (size_t i = 0; i < n; i++)
      {
        const double a = (double)i / sps, b = (double)(i + 1) / sps;
        const double ca = floor (a), cb = floor (b - 1e-12);
        const float  va = code[(size_t)ca % sf] ? -1.0f : 1.0f;
        const float  vb = code[(size_t)cb % sf] ? -1.0f : 1.0f;
        const float  fa = (float)((ca + 1.0 - a) * sps); /* share of chip a */
        rx[i]           = ca == cb ? va : fa * va + (1.0f - fa) * vb;
      }
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.005, 0.707, 0.5, K);
    DP_REQUIRE (d != NULL);
    dp_dll_steps (d, rx, n, out, n);
    const double ph  = dp_dll_get_code_phase (d);
    const double off = ph > 0.5 * (double)sf ? ph - (double)sf : ph;
    printf ("  6e: 2.2 samples a chip, %zu periods: code phase off %+.4f "
            "chips, locked %d\n",
            nper, off, dp_dll_get_locked (d));
    DP_CHECK (fabs (off) < 0.01);
    DP_CHECK (dp_dll_get_locked (d) == 1);
    dp_dll_destroy (d);
    free (out);
    free (rx);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 7. Always-on lock detector: locks on signal, not on noise        *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, K = 4, nper = 3000;
    size_t       te   = sf * sps;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    float _Complex *rx  = malloc (te * nper * sizeof (*rx));
    float _Complex *out = malloc (te * nper * sizeof (*out));

    /* signal present (const data, code-aligned): strong despread -> lock.
       The default config (pfa=1e-3, 20 looks, threshold ~8.567) is applied at
       create, so the detector works with no configure_lock call. */
    size_t          n = make_signal (rx, code, sf, sps, 0.0, nper, 9u, 1);
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    DP_CHECK (dp_dll_get_locked (d) == 0); /* fresh: unlocked */
    DP_CHECK (dp_dll_get_lock_stat (d) == 0.0);
    dp_dll_steps (d, rx, n, out, te * nper);
    DP_CHECK (dp_dll_get_locked (d) == 1);
    DP_CHECK (dp_dll_get_lock_stat (d) > 8.567); /* default CFAR threshold */
    dp_dll_destroy (d);

    /* noise only: prompt power matches the off-peak reference, so the
       statistic sits near sqrt(2*N) ~ 6.3 and stays below threshold. */
    uint32_t st = 4242u;
    for (size_t i = 0; i < te * nper; i++)
      rx[i] = dp_cgauss (&st);
    dp_dll_state_t *dn
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    dp_dll_steps (dn, rx, te * nper, out, te * nper);
    DP_CHECK (dp_dll_get_locked (dn) == 0);
    DP_CHECK (dp_dll_get_lock_stat (dn) < 8.567);
    DP_CHECK (dp_dll_get_lock_stat (dn)
              > 3.0); /* near sqrt(40), not degenerate */
    DP_CHECK (dp_dll_get_noise_est (dn) > 0.0);
    /* configure_lock retunes the threshold; an unreachable one never locks. */
    dp_dll_configure_lock_raw (dn, 1e9, 1e9, 20, 1.0 / 1024.0, 1, 1);
    DP_CHECK (dp_dll_get_lock_stat (dn)
              == 0.0); /* retune clears the statistic */
    dp_dll_steps (dn, rx, te * nper, out, te * nper);
    DP_CHECK (dp_dll_get_locked (dn) == 0);
    dp_dll_destroy (dn);

    /* Verify-counted declare: the default config (pfa=1e-3 -> n_up=2)
       needs two CONSECUTIVE above-threshold decisions. K=4 partials per
       period x 5 periods = 20 looks = exactly one decision window, so one
       window of strong signal leaves the loop unlocked with the verify
       run in flight; the second window declares. Both windows are drawn
       from ONE continuous 11-period signal (not the same 5 periods
       replayed) -- replaying the identical buffer for the second call
       would re-inject it right where the first call's phase left off,
       an artificial discontinuity at the seam that isn't a real data
       transition and measurably (not just cosmetically) degrades one
       epoch's power. A small margin beyond exactly 5 periods' worth of
       samples (half a partial-segment width, well under a quarter
       period) gives each window's last epoch room to complete its wrap
       even though the fixed-point loop's phase_inc drifts by a few PPM
       during early convergence -- exactly 5*tsamps samples is otherwise
       a zero-slack edge case, not a meaningful assertion. */
    size_t margin = sf * sps / 8;
    size_t nv     = sf * sps * 5 + margin;
    make_signal (rx, code, sf, sps, 0.0, 11, 9u, 1);
    dp_dll_state_t *dv
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    dp_dll_steps (dv, rx, nv, out, nv);
    DP_CHECK (dv->lock.cnt == 1);
    DP_CHECK (dp_dll_get_locked (dv) == 0);
    dp_dll_steps (dv, rx + nv, nv, out, nv);
    DP_CHECK (dp_dll_get_locked (dv) == 1);
    dp_dll_destroy (dv);
    free (rx);
    free (out);
    free (code);
  }

  /* serializable state — loop_filter child + correlators resume; the borrowed
   * code pointer is this instance's, preserved across set_state.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    dp_dll_state_t *a = dp_dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5, 1);
    dp_dll_state_t *b = dp_dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5, 1);
    DP_CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 80; i++)
      dll_accumulate (a, (float)(i % 7) - 3.0f + 0.5f * I);
    DP_STATE_ROUNDTRIP_TEST (dp_dll, a, b);
    DP_CHECK (b->chip_pos == a->chip_pos && b->acc_p == a->acc_p);
    DP_CHECK (b->lf.integ == a->lf.integ);            /* child resumed */
    DP_CHECK (b->code != NULL && b->code != a->code); /* code preserved */
    dp_dll_destroy (a);
    dp_dll_destroy (b);
  }

  /* lock config — pfa face is C-first now: the create-time default is the
   * precise detection-module threshold (no baked constant), the EMA alpha
   * comes from the det_ema_alpha estimator-SNR contract, and bad pfa is
   * rejected without touching the live config. */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    dp_dll_state_t *d = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 1);
    DP_CHECK (d != NULL);
    /* create-time default == the exact caller-path config; both lockdet
     * thresholds carry the CFAR eta (no level hysteresis by default) and
     * the declare verify count derives from the pfa:
     * det_verify_count(1e-3, 1e-6) = 2. */
    DP_CHECK (d->lock.up_thresh == dp_det_threshold_noncoherent (1e-3, 20));
    DP_CHECK (d->lock.down_thresh == d->lock.up_thresh);
    DP_CHECK (d->lock.n_up == 2 && d->lock.n_down == 2);
    DP_CHECK (fabs (d->lock_alpha - 1.0 / 1024.0) < 1e-15); /* auto floor */

    /* auto derivation follows 1/alpha = max(32*N, 1024) */
    DP_CHECK (dp_dll_configure_lock (d, 1e-3, 64, 0.0) == DP_OK);
    DP_CHECK (fabs (d->lock_alpha - 1.0 / 2048.0) < 1e-15);
    DP_CHECK (d->n_looks == 64);

    /* explicit reference SNR overrides the auto sizing */
    DP_CHECK (dp_dll_configure_lock (d, 1e-2, 20, 20.0) == DP_OK);
    DP_CHECK (fabs (d->lock_alpha - 2.0 / 101.0) < 1e-15);
    DP_CHECK (d->lock.up_thresh == dp_det_threshold_noncoherent (1e-2, 20));

    /* bad pfa: rejected whole, live config untouched */
    double thr = d->lock.up_thresh, alp = d->lock_alpha;
    DP_CHECK (dp_dll_configure_lock (d, 0.0, 20, 0.0) == DP_ERR_INVALID);
    DP_CHECK (dp_dll_configure_lock (d, 1.0, 20, 0.0) == DP_ERR_INVALID);
    DP_CHECK (dp_dll_configure_lock (d, -1.0, 20, 0.0) == DP_ERR_INVALID);
    DP_CHECK (d->lock.up_thresh == thr && d->lock_alpha == alp);

    /* n_looks = 0 clamps to 1 (auto floor still applies) */
    DP_CHECK (dp_dll_configure_lock (d, 1e-3, 0, 0.0) == DP_OK);
    DP_CHECK (d->n_looks == 1);
    DP_CHECK (fabs (d->lock_alpha - 1.0 / 1024.0) < 1e-15);
    dp_dll_destroy (d);
  }

  /* telemetry attach — seven records per code epoch in both the coherent
   * (segments == 1) and partial-correlation (segments > 1) loops; blobs
   * stay attachment-independent; a live attachment survives set_state. */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    enum
    {
      EP  = 62,
      NEP = 20,
      L   = EP * NEP
    };
    float _Complex rx[L], out[256];
    dp_tlm_rec_t recs[512];
    for (int i = 0; i < L; i++)
      rx[i] = (code[(size_t)(i / 2) % 31] & 1u) ? -1.0f : 1.0f;
    dp_tlm_t       *tlm = dp_tlm_create (4096);
    dp_dll_state_t *d = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 1);
    DP_CHECK (tlm != NULL && d != NULL);
    DP_CHECK (dp_dll_set_telemetry (d, tlm, "code", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.e") == d->tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.rate") == d->tlm.id_rate);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.lock") == d->tlm.id_lock);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.locked") == d->tlm.id_locked);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.nco") == d->tlm.id_nco);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.prompt_amp") == d->tlm.id_pamp);
    DP_CHECK (dp_tlm_probe_id (tlm, "code.offset_pwr") == d->tlm.id_opwr);

    /* An aid the input does not carry: the NCO's command (".nco") then
     * sits 1e-4 off the loop's own rate (".rate"), so a ".nco" that
     * reported the rate instead would show. */
    dp_dll_set_rate_aid (d, 1e-4);
    size_t k     = dp_dll_steps (d, rx, L, out, 256);
    size_t n_rec = dp_tlm_read (tlm, 512, recs, 512);
    /* e, rate, lock, locked, nco, prompt_amp, offset_pwr */
    DP_CHECK (k > 0 && n_rec == 7 * k);
    /* The final epoch's records mirror the tracked state (flush order per
     * epoch: e, rate, lock, locked, nco, prompt_amp, offset_pwr); ".nco"
     * is the commanded increment in chips per chip, and the last two are
     * the epoch's last look's |P| and |O|^2. */
    DP_CHECK (recs[n_rec - 6].value == (float)d->code_rate);
    DP_CHECK (recs[n_rec - 5].value == (float)d->lock_stat);
    DP_CHECK (recs[n_rec - 4].value == (float)dp_dll_get_locked (d));
    const double nco_cmd
        = nco_word_to_norm (d->code_nco.phase_inc) / d->inv_tsamps;
    DP_CHECK (recs[n_rec - 3].value == (float)nco_cmd);
    DP_CHECK (recs[n_rec - 2].value == (float)d->look_prompt_amp);
    DP_CHECK (recs[n_rec - 1].value == (float)d->look_offset_pwr);
    DP_CHECK_MSG (d->look_prompt_amp > 0.0 && d->look_offset_pwr >= 0.0,
                  "the look's terms are recorded");
    DP_CHECK_MSG (fabs (nco_cmd - d->code_rate) > 5e-5,
                  "the aid separates the command from the loop's rate");
    dp_dll_set_rate_aid (d, 0.0);

    /* segments > 1: the partial loop flushes once per epoch (an epoch is
     * `segments` emitted partials), through the same literal-tlm split. */
    dp_dll_state_t *s2 = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 2);
    DP_CHECK (s2 != NULL);
    DP_CHECK (dp_dll_set_telemetry (s2, tlm, "code2", 1) == DP_OK);
    size_t k2 = dp_dll_steps (s2, rx, L, out, 256);
    size_t n2 = dp_tlm_read (tlm, 512, recs, 512);
    DP_CHECK (k2 > 0 && n2 > 0 && n2 % 7 == 0);
    DP_CHECK (n2
              <= 7 * (k2 / 2 + 1)); /* one flush per epoch, not per partial */
    dp_dll_destroy (s2);

    /* Blobs zero the attachment (deterministic) and set_state into an
     * attached instance preserves that instance's live attachment. */
    size_t sb = dp_dll_state_bytes (d);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    dp_dll_get_state (d, b1);
    dp_dll_state_t *d3 = dp_dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 1);
    DP_CHECK (d3 != NULL);
    DP_CHECK (dp_dll_set_telemetry (d3, tlm, "code3", 2) == DP_OK);
    DP_CHECK (dp_dll_set_state (d3, b1) == DP_OK);
    DP_CHECK (d3->tlm.ctx == tlm);
    DP_CHECK (d3->tlm.id_e == dp_tlm_probe_id (tlm, "code3.e"));
    dp_dll_get_state (d3, b2);
    DP_CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    dp_dll_destroy (d3);

    /* Detach: probe sites revert to the single-branch cost. */
    DP_CHECK (dp_dll_set_telemetry (d, NULL, "code", 1) == DP_OK);
    DP_CHECK (d->tlm.ctx == NULL);
    (void)dp_dll_steps (d, rx, L, out, 256);
    DP_CHECK (dp_tlm_read (tlm, 512, recs, 512) == 0);

    /* A full probe table fails the attach whole. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    DP_CHECK (dp_dll_set_telemetry (d, tlm, "nope", 1) == DP_ERR_INVALID);
    DP_CHECK (d->tlm.ctx == NULL);
    dp_dll_destroy (d);
    dp_tlm_destroy (tlm);
  }

  /* ---------------------------------------------------------------- *
   * 8. Long-run false-lock regression: zero Doppler, zero carrier,   *
   *    segments=1 -- the code phase must never make a large jump     *
   *    once locked. This is the committed guard for a real bug: an   *
   *    earlier fixed-point-NCO redesign attempt (dwell-integrated    *
   *    replica + magnitude discriminator, direct phase kicks) passed  *
   *    every other test in this file yet still false-locked into a   *
   *    stable, wrong code phase over thousands of periods on select   *
   *    noise seeds -- with no test here to catch it. The 2x-          *
   *    oversampled replica + power-domain NELP discriminator (see     *
   *    dll_replica()/dll_update()) is what actually fixed it; this    *
   *    test is what would have caught the earlier attempt's failure   *
   *    immediately instead of needing a multi-session investigation. *
   * ---------------------------------------------------------------- */
  {
    const size_t    sf = 63, sps = 4, nper = 4188, tsamps = sf * sps;
    const uint32_t  seeds[] = { 42, 43, 44, 100 };
    const size_t    n_seeds = sizeof (seeds) / sizeof (seeds[0]);
    uint8_t        *code    = malloc (sf);
    float _Complex *rx      = malloc (tsamps * nper * sizeof (*rx));
    for (size_t si = 0; si < n_seeds; si++)
      {
        make_code (code, sf, 7u + seeds[si]);
        size_t n
            = make_signal (rx, code, sf, sps, 0.0, nper, 1000u + seeds[si], 1);
        dp_dll_state_t *d
            = dp_dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, 1);
        double prev_phase = -1.0;
        double max_jump   = 0.0;
        for (size_t p = 0; p < nper; p++)
          {
            for (size_t i = 0; i < tsamps; i++)
              dll_accumulate (d, rx[p * tsamps + i]);
            dll_update (d);
            d->acc_e = d->acc_p = d->acc_l = 0.0f;
            double cp                      = dp_dll_get_code_phase (d);
            if (prev_phase >= 0.0)
              {
                double jump = fabs (cp - prev_phase);
                if (jump > (double)sf / 2.0)
                  jump = (double)sf - jump;
                if (jump > max_jump)
                  max_jump = jump;
              }
            prev_phase = cp;
          }
        (void)n;
        DP_CHECK (max_jump < 5.0); /* no false-lock jump over the whole run */
        DP_CHECK (fabs (dp_dll_get_code_rate (d) - 1.0) < 1e-3);
        dp_dll_destroy (d);
      }
    free (rx);
    free (code);
  }

  /* dp_dll_lookback_segments(): ports despreader_coupled.py's
   * async_lookback_windows() -- the known reference value it derives at
   * this project's own validated point (tsamps=2046, max_error_db=0.5
   * -> windows=11). */
  {
    DP_CHECK (dp_dll_lookback_segments (2046, 0.5) == 11);
    /* tsamps==0 is a degenerate guard, not a real caller input. */
    DP_CHECK (dp_dll_lookback_segments (0, 0.5) == 1);
    /* Every returned segments count must evenly divide tsamps -- the
     * whole point of the divisor-snapping step. */
    size_t tsamps_probe[] = { 2046, 1024, 63 * 4, 31 * 2, 100 };
    for (size_t i = 0; i < sizeof (tsamps_probe) / sizeof (tsamps_probe[0]);
         i++)
      {
        size_t t = tsamps_probe[i];
        size_t s = dp_dll_lookback_segments (t, 0.5);
        DP_CHECK (s >= 1 && s <= t);
        DP_CHECK (t % s == 0);
      }
  }

  /* ---------------------------------------------------------------- *
   * Coast: the loop holds, the discriminator still reads             *
   * ---------------------------------------------------------------- */
  /* The header: coasting, "the discriminator is not filtered and
     phase_inc is not steered" -- so a holder coasting on another clock
     (a searcher's cell, a carrier aid) reads where the signal sits
     against the held phase, on `last_error` and the `.e` probe, and
     corrects on it. Seeded 0.15 chip off a clean signal and held from
     the start, the loop's rate and phase stay put and its discriminator
     reads the offset; the same loop run pulls in and its discriminator
     decays. Before this pin the coast returned before the discriminator
     was computed, and a coasting loop's probe read 0 for as long as it
     coasted (design section 12.22). */
  {
    /* 600 periods: 5/bn at 0.005 is 1000 epochs of settling for the
       running loop; the held one has nothing to settle. */
    const size_t sf = 63, sps = 4, nper = 600;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 23u);
    float _Complex *rx  = malloc (sf * sps * nper * sizeof (*rx));
    size_t          n   = make_signal (rx, code, sf, sps, 0.0, nper, 9u, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    const double    off = 0.15;
    dp_dll_state_t *held
        = dp_dll_create (code, sf, sps, off, 0.005, 0.707, 0.5, 1);
    dp_dll_state_t *run
        = dp_dll_create (code, sf, sps, off, 0.005, 0.707, 0.5, 1);
    DP_REQUIRE (held && run);
    dp_dll_hold_here (held);
    dp_dll_set_coast (held, 1);
    (void)dp_dll_steps (held, rx, n, sym, nper);
    (void)dp_dll_steps (run, rx, n, sym, nper);
    /* Held: the rate is the nominal it was held at, the phase advanced at
       it -- fed over samples per chip, plus the seed -- to a hundredth of
       a chip; the discriminator reads the offset, not zero. */
    DP_CHECK (dp_dll_get_code_rate (held) == 1.0);
    double want = fmod ((double)n / (double)sps + off, (double)sf);
    double got  = fmod (dp_dll_get_code_phase (held), (double)sf);
    double dph  = fabs (got - want);
    if (dph > sf / 2.0)
      dph = sf - dph;
    DP_CHECK_MSG (dph < 0.01, "coasting, the phase advances at the held rate");
    DP_CHECK_MSG (fabs (dp_dll_get_last_error (held)) > 0.05,
                  "coasting, the discriminator still reads the offset");
    /* Run: the loop pulled the offset in, so its read decayed. */
    DP_CHECK_MSG (fabs (dp_dll_get_last_error (run)) < 0.02,
                  "running, the same loop pulled in");
    /* The correction: the held loop put back on the signal reads a
       decayed discriminator over the next periods, and its phase is what
       it was told, folded into the period. */
    dp_dll_set_code_phase (held, (double)n / (double)sps + 2.0 * sf);
    DP_CHECK (fabs (fmod (dp_dll_get_code_phase (held), (double)sf)
                    - fmod ((double)n / (double)sps, (double)sf))
              < 1e-9);
    (void)dp_dll_steps (held, rx, 20 * sf * sps, sym, nper);
    DP_CHECK_MSG (fabs (dp_dll_get_last_error (held)) < 0.05,
                  "put back on the signal, the held loop's read decays");
    DP_CHECK (dp_dll_get_code_rate (held) == 1.0);
    dp_dll_destroy (run);
    dp_dll_destroy (held);
    free (sym);
    free (rx);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 8b. A put across the period wrap: nothing dumped, nothing        *
   *     skipped, on both correlation paths (#1287)                    *
   * ---------------------------------------------------------------- */
  /* dp_dll_set_code_phase() used to move only the NCO. The epoch in progress
     is keyed to the wrap, so a put back across it read every partial of the
     next period ready at once (slivers, and a garbage epoch at the wrap),
     and a put forward across it skipped the wrap and folded a second period
     into one epoch. Both puts below are a 1-chip correction that lands
     EXACTLY on the signal, so every epoch after the put is a clean
     full-period read and the counts are exact over the next 10 periods:
       back    -- 2 samples of tail, then 9 whole epochs (the bug: 10, the
                  first a garbage epoch at the wrap);
       forward -- the owed epoch closes at once, then 10 whole epochs (the
                  bug: 10, two periods folded into the first).
     Coasting, so only the put moves the phase. The blob taken right after
     the put resumes to the same epochs, so the arming is state.
     Sabotaged: drop the arming in dp_dll_set_code_phase (both counts read 10);
     drop the tail discard at the awaited wrap (the first epoch after a back
     put reads the tail's energy). */
  {
    /* 175 periods: ten before the put, ten after it, and the output level's
       settle after that (the header's DLL_OUT_LEVEL_PERIODS; see below). */
    const size_t sf = 63, sps = 4, nper = 175;
    const size_t te = sf * sps, cap = te * nper;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 29u);
    float _Complex *rx   = malloc (cap * sizeof (*rx));
    float _Complex *out  = malloc (cap * sizeof (*out));
    float _Complex *oute = malloc (cap * sizeof (*oute));
    (void)make_signal (rx, code, sf, sps, 0.0, nper, 5u, 1);
    const size_t segs[2] = { 1, 4 };
    for (size_t si = 0; si < 2; si++)
      for (int dir = -1; dir <= 1; dir += 2)
        {
          const size_t K = segs[si];
          /* back: the loop runs 1 chip AHEAD and is put back onto the
             signal just after its own wrap; forward: 1 chip BEHIND, put
             forward just before it. */
          const double    init = dir < 0 ? 1.0 : (double)sf - 1.0;
          const size_t    m    = dir < 0 ? 10 * te - 2 : 10 * te + 2;
          dp_dll_state_t *d
              = dp_dll_create (code, sf, sps, init, 0.005, 0.707, 0.5, K);
          dp_dll_state_t *e
              = dp_dll_create (code, sf, sps, init, 0.005, 0.707, 0.5, K);
          DP_REQUIRE (d && e);
          dp_dll_hold_here (d);
          dp_dll_set_coast (d, 1);
          size_t na = dp_dll_steps (d, rx, m, out, cap);
          DP_CHECK_MSG (na == 10 * K, "before the put: ten epochs");
          /* put onto the signal's own phase at sample m */
          dp_dll_set_code_phase (d,
                                 fmod ((double)m / (double)sps, (double)sf));
          void *blob = malloc (dp_dll_state_bytes (d));
          DP_REQUIRE (blob);
          dp_dll_get_state (d, blob);
          DP_CHECK (dp_dll_set_state (e, blob) == DP_OK);

          /* After the put, ONE SAMPLE A CALL: each epoch's partials then
             come out of a call of their own, divided by the output level
             read right after it (#2495), so |out| * level is the partial's
             own sum over seg_norm -- what this check read before the output
             had a level. The loop ran a chip off the code before the put,
             so its level was set on the code's sidelobes and is still
             climbing here; taking it out keeps the check exact. */
          double *lvl = malloc (cap * sizeof *lvl);
          DP_REQUIRE (lvl);
          size_t nb = 0;
          for (size_t i = 0; i < 10 * te; i++)
            {
              size_t k = dp_dll_steps (d, rx + m + i, 1, out + nb, cap - nb);
              for (size_t j = 0; j < k; j++)
                lvl[nb + j] = K > 1 && d->out_level > 0.0 ? d->out_level : 1.0;
              nb += k;
            }
          size_t want = dir < 0 ? 9 * K : 11 * K;
          DP_CHECK_MSG (nb == want, dir < 0
                                        ? "back across the wrap: an epoch "
                                          "closes only on a whole period"
                                        : "forward across the wrap: the "
                                          "owed epoch closes, none folded");
          /* Every read past the owed one spans a whole partial: no sliver.
             Compared PERIOD OVER PERIOD (out[i] against out[i + K]), not
             against the run's own min/max. At segments > 1 the partials of
             a PN code legitimately differ -- each spans a different quarter
             of the code, ~7% peak-to-peak here -- so "every read is the
             same size" is a property correct output does NOT have, and
             asserting it failed on a working put. One period later is the
             same quarter of the same code, so the comparison is exact; a
             sliver (a one-sample partial, ~1/63 of a full one at this
             geometry) cannot survive it. */
          /* Back: from the first read, which is what pins the TAIL
             DISCARD -- the 2-sample tail leaks only ~1.4% into that epoch,
             so a loose bound cannot see it and the steady state repeats
             EXACTLY, which is why the tolerance is 1% and not 10%.
             Forward: skip the owed epoch AND the one after it; that one is
             a legitimate ~2.8% transient as the loop settles onto the new
             phase, and tightening the bound over it would fail correct
             output. */
          size_t first = dir < 0 ? 0 : 2 * K;
          DP_CHECK_MSG (nb > first + K, "enough reads to compare periods");
          for (size_t i = first; i + K < nb; i++)
            {
              float a = cabsf (out[i]) * (float)lvl[i];
              float b = cabsf (out[i + K]) * (float)lvl[i + K];
              DP_CHECK_MSG (a > 0.99f * b && b > 0.99f * a,
                            "after the put every read spans its whole "
                            "partial");
            }
          size_t ne = dp_dll_steps (e, rx + m, 10 * te, oute, cap);
          DP_CHECK_MSG (ne == nb
                            && memcmp (oute, out, nb * sizeof (*out)) == 0,
                        "resumed from the blob taken after the put, the same "
                        "epochs");
          /* The level's settle (segments > 1): within 1% of where it ends
             ln 100 / -ln(1 - 1/N) periods after the put, N =
             DLL_OUT_LEVEL_PERIODS (146 at 32) -- the header's count, here
             from a level set on the code's sidelobes, the largest step a
             put makes -- and from there the OUTPUT itself, nothing taken
             out, repeats period over period to the same 1%. */
          if (K > 1)
            {
              const size_t settle = (size_t)ceil (
                  log (100.0) / -log1p (-1.0 / (double)DLL_OUT_LEVEL_PERIODS));
              const size_t pos = m + 10 * te;
              (void)dp_dll_steps (d, rx + pos, (settle - 10) * te, out, cap);
              const double at_settle = d->out_level;
              size_t       nt = dp_dll_steps (d, rx + pos + (settle - 10) * te,
                                              12 * te, out, cap);
              const double at_end = d->out_level;
              DP_CHECK_MSG (fabs (at_settle - at_end) < 0.01 * at_end,
                            "the output level is within 1% of where it "
                            "ends, the header's settle after the put");
              DP_CHECK_MSG (nt > K, "reads after the settle");
              for (size_t i = 0; i + K < nt; i++)
                {
                  float a = cabsf (out[i]), b = cabsf (out[i + K]);
                  DP_CHECK_MSG (a > 0.99f * b && b > 0.99f * a,
                                "after the settle the output itself repeats "
                                "period over period");
                }
            }
          free (lvl);
          free (blob);
          dp_dll_destroy (e);
          dp_dll_destroy (d);
        }
    free (oute);
    free (out);
    free (rx);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 9. The coasting read: every steer sums into dp_dll_take_error()     *
   *    on BOTH correlation paths (the one steer, doppler#1280)        *
   * ---------------------------------------------------------------- */
  /* A holder correcting a coasting loop once a block (design section
     12.22-12.24) reads the block MEAN of the discriminator, not the last
     epoch's probe. Every steer adds to a running sum; take returns the
     count and the sum and zeroes both. The full-epoch path steers once
     per epoch; the symbol-aided partial path once per symbol window.
     Sabotaged: accumulate on one path only (the other's count reads 0);
     do not zero on take (the second take repeats the first). */
  {
    const size_t sf = 63, sps = 4, nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 23u);
    float _Complex *rx  = malloc (sf * sps * nper * sizeof (*rx));
    size_t          n   = make_signal (rx, code, sf, sps, 0.0, nper, 9u, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    dp_dll_state_t *held
        = dp_dll_create (code, sf, sps, 0.15, 0.005, 0.707, 0.5, 1);
    DP_REQUIRE (held);
    dp_dll_hold_here (held);
    dp_dll_set_coast (held, 1);
    double sum = 1.0;
    DP_CHECK_MSG (dp_dll_take_error (held, &sum) == 0 && sum == 0.0,
                  "a fresh loop has nothing to take");
    (void)dp_dll_steps (held, rx, n, sym, nper);
    size_t cnt = dp_dll_take_error (held, &sum);
    DP_CHECK_MSG (cnt == nper,
                  "full-epoch path: one steer per epoch, coasting or not");
    DP_CHECK_MSG (
        fabs (sum / (double)cnt) > 0.05
            && fabs (sum / (double)cnt - dp_dll_get_last_error (held)) < 0.05,
        "the mean reads the held offset, as the last steer does");
    DP_CHECK_MSG (dp_dll_take_error (held, &sum) == 0 && sum == 0.0,
                  "taken: the sum starts again from zero");
    /* The running loop accumulates too: the same count. */
    dp_dll_state_t *run
        = dp_dll_create (code, sf, sps, 0.15, 0.005, 0.707, 0.5, 1);
    (void)dp_dll_steps (run, rx, n, sym, nper);
    {
      /* One steer per epoch. The seed offset of 0.15 chip delays this
         loop's period boundaries until it pulls in; at bn 0.005 that takes
         about 1000 periods, so within 300 the last boundary falls just past
         the end of the stream and the count is nper - 1. (It read nper only
         because the old loop's integral path pulled the offset in within a
         few periods, doppler#3234.) The held loop above never moves, so its
         boundaries are on time and its count is nper. */
      size_t got = dp_dll_take_error (run, &sum);
      DP_CHECK_MSG (got + 1 == nper,
                    "the running loop's seed offset is still unpulled at the "
                    "end of 300 periods (bn 0.005 settles in about 5/bn = "
                    "1000), so the last boundary falls past the stream");
    }
    /* The Python face: the mean, NaN once taken. */
    (void)dp_dll_steps (held, rx, sf * sps * 10, sym, nper);
    DP_CHECK (fabs (dp_dll_take_error_mean (held)) > 0.05);
    DP_CHECK (isnan (dp_dll_take_error_mean (held)));
    dp_dll_destroy (run);
    dp_dll_destroy (held);
    free (sym);
    free (rx);
    free (code);
  }
  {
    /* The symbol-aided partial path on 6b's asynchronous clean stream:
       once per symbol window, so fewer steers than epochs and more than
       one per two epochs (7.24 partials per symbol at four per epoch is
       1.81 epochs per symbol; the first looks settle the hypothesis). */
    const size_t sf = 63, sps = 4, K = 4, nsym = 400;
    const size_t te   = sf * sps;
    const double P    = 7.24;
    const double tsym = P * (double)te / (double)K;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    size_t          N   = (size_t)(nsym * tsym) + 2 * te;
    float _Complex *rx  = malloc (N * sizeof (*rx));
    float _Complex *out = malloc (N * sizeof (*out));
    make_async_signal (rx, N, code, sf, sps, 0.0, tsym, 0.37 * (double)te, 0.0,
                       7u, 99u);
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.1, 0.002, 0.707, 0.5, K);
    DP_REQUIRE (d && dp_dll_set_symbol_period (d, P) == DP_OK);
    dp_dll_hold_here (d);
    dp_dll_set_coast (d, 1);
    size_t nep = N / te;
    for (size_t e = 0; e < nep; e++)
      dp_dll_steps (d, rx + e * te, te, out, te);
    double sum;
    size_t cnt = dp_dll_take_error (d, &sum);
    DP_CHECK_MSG (cnt > nep / 2 && cnt < nep,
                  "symbol-aided path: one steer per symbol window");
    DP_CHECK_MSG (fabs (sum / (double)cnt) > 0.02,
                  "coasting, the windows' mean reads the held offset");
    /* A blob taken mid-interval carries the sum: the restored loop's
       take is the original's. */
    dp_dll_state_t *b
        = dp_dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    DP_REQUIRE (b && dp_dll_set_symbol_period (b, P) == DP_OK);
    for (size_t e = 0; e < 40; e++)
      dp_dll_steps (d, rx + e * te, te, out, te);
    DP_STATE_ROUNDTRIP_TEST (dp_dll, d, b);
    double sb, sd;
    size_t nb = dp_dll_take_error (b, &sb), nd = dp_dll_take_error (d, &sd);
    DP_CHECK_MSG (nb == nd && nb > 0 && sb == sd,
                  "the accumulator rides in the blob");
    dp_dll_destroy (b);
    dp_dll_destroy (d);
    free (out);
    free (rx);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 10. A held loop takes a new rate aid at its next boundary         *
   * ---------------------------------------------------------------- */
  /* A holder refreshing the Doppler it holds (the searcher-timed receiver's
     fold) needs the code rate to follow: the held rate, aid included, is
     converted at every period boundary, so the next one carries it. Held at
     aid 0 on a nominal-rate signal, told 2e-4, the phase over the next 200
     epochs advances the aid's extra chips (200 * sf * 2e-4 = 2.52 at sf 63)
     beyond nominal. Sabotaged: the held branch of dll_steer returns without
     converting -- the extra reads 0. */
  {
    const size_t sf = 63, sps = 4, nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 23u);
    float _Complex *rx  = malloc (sf * sps * nper * sizeof (*rx));
    size_t          n   = make_signal (rx, code, sf, sps, 0.0, nper, 9u, 1);
    float _Complex *sym = malloc (nper * sizeof (*sym));
    dp_dll_state_t *d
        = dp_dll_create (code, sf, sps, 0.0, 0.005, 0.707, 0.5, 1);
    DP_REQUIRE (d);
    dp_dll_hold_here (d);
    dp_dll_set_coast (d, 1);
    (void)dp_dll_steps (d, rx, 100 * sf * sps, sym, nper);
    const double aid = 2e-4;
    dp_dll_set_rate_aid (d, aid);
    const double p0 = dp_dll_get_code_phase (d);
    (void)dp_dll_steps (d, rx + 100 * sf * sps, 200 * sf * sps, sym, nper);
    double adv = dp_dll_get_code_phase (d) - p0;
    while (adv < 0.0)
      adv += (double)sf;
    /* The advance modulo the code: 200 epochs at nominal is 0 modulo sf,
       so what is left is the aid's extra, 2.52 chips. */
    adv               = fmod (adv, (double)sf);
    const double want = 200.0 * (double)sf * aid;
    DP_CHECK_MSG (
        fabs (adv - want) < 0.05,
        "held, a new rate aid moves the code rate from the next boundary");
    DP_CHECK (dp_dll_get_code_rate (d) == 1.0); /* the loop's own observable */
    (void)n;
    dp_dll_destroy (d);
    free (sym);
    free (rx);
    free (code);
  }

  /* A refused blob changes nothing (doppler#2092). */
  DP_CHECK (refused_blob_changes_nothing () == 0);

  /* A bad float is refused, never run (doppler#2103). */
  DP_CHECK (bad_floats_are_refused () == 0);

  /* A refused retune changes nothing; a forged config is refused (#2103). */
  DP_CHECK (refused_retune_changes_nothing () == 0);
  DP_CHECK (forged_state_is_refused_or_recomputed () == 0);
  DP_CHECK (replica_word_matches_its_definition () == 0);
  DP_CHECK (tap_words_track_their_inputs () == 0);
  DP_CHECK (hold_then_retune_resumes_exact () == 0);
  DP_CHECK (steer_telemetry_reports_the_steer_and_moves_nothing () == 0);
  DP_CHECK (coast_order_resumes_exact (0) == 0);
  DP_CHECK (coast_order_resumes_exact (1) == 0);
  DP_CHECK (coast_order_resumes_exact (2) == 0);
  DP_CHECK (coast_reports_the_held_rate (1) == 0);
  DP_CHECK (coast_reports_the_held_rate (4) == 0);

  /* segments > 1: circular error, a level free of the input's (#2495). */
  DP_CHECK (output_error_is_circular () == 0);
  DP_CHECK (output_level_is_input_level_free () == 0);

  /* dp_fmod_pos stays in [0, m) (doppler#2110). A negative remainder within
     half an ulp of zero rounded r + m up to exactly m, and dll_replica, which
     indexes the code with the fold of 2c - 0.5 over 2*sf, then read
     code[sf], one past the copy. The issue's worked case is sf = 31: the
     early tap at a spacing of 0.25 - 2^-55 from a dwell centre at chip 0
     gives -2^-54. The fold of such a sliver is 0, its equal on the circle;
     the ordinary negative fold and an exact multiple are unchanged. */
  /* Run-time inputs: a constant argument is folded at compile time, and
     the folded answer was right while the -O3 -ffast-math code was not.
     Both period shapes are pinned. With a CONSTANT period gcc rewrote
     `r + m < m` as `r < 0` and returned 62 (callers folding a phase over a
     fixed period); dll_replica's period, 2*sf, is a run-time value. */
  {
    volatile double sliver = -0x1p-54, tap = 2.0 * (0.25 - 0x1p-55) - 0.5;
    volatile double neg1 = -1.0, negm = -62.0, wrap = 124.5, nan_in = NAN;
    volatile double m = 62.0;
    DP_CHECK (dp_fmod_pos (sliver, 62.0) < 62.0);
    DP_CHECK (dp_fmod_pos (sliver, 62.0) == 0.0);
    DP_CHECK (dp_fmod_pos (tap, 62.0) < 62.0);
    DP_CHECK (isnan (dp_fmod_pos (nan_in, 62.0)));
    DP_CHECK (dp_fmod_pos (sliver, m) < 62.0);
    DP_CHECK (dp_fmod_pos (sliver, m) == 0.0);
    DP_CHECK (dp_fmod_pos (tap, m) < 62.0);
    DP_CHECK (dp_fmod_pos (neg1, m) == 61.0);
    DP_CHECK (dp_fmod_pos (negm, m) == 0.0);
    DP_CHECK (dp_fmod_pos (wrap, m) == 0.5);
    /* A NaN stays NaN, visible, rather than becoming the plausible phase
       0. */
    DP_CHECK (isnan (dp_fmod_pos (nan_in, m)));
  }

  /* The integral path is a unit-gain actuator at every code length. */
  DP_CHECK (the_integrator_moves_the_phase_one_chip_per_update (31) == 0);
  DP_CHECK (the_integrator_moves_the_phase_one_chip_per_update (127) == 0);

  DP_CHECK (a_blob_from_before_the_unit_gain_integral_path_is_refused () == 0);

  /* The conversion's remainder is carried: a command under one phase-word
     step moves the mean increment, at one segment and at partials, either
     sign (doppler#3249). */
  DP_CHECK (the_mean_increment_follows_a_command_under_one_step (1, 0.05)
            == 0);
  DP_CHECK (the_mean_increment_follows_a_command_under_one_step (1, -0.05)
            == 0);
  DP_CHECK (the_mean_increment_follows_a_command_under_one_step (4, 0.05)
            == 0);
  DP_CHECK (the_mean_increment_follows_a_command_under_one_step (4, -0.05)
            == 0);
  DP_CHECK (the_carried_remainder_is_in_the_blob_and_checked () == 0);
  DP_CHECK (a_hold_follows_its_rate_to_a_step (100, 0, 0) == 0);
  DP_CHECK (a_hold_follows_its_rate_to_a_step (6285, 0, 0) == 0);
  DP_CHECK (a_hold_follows_its_rate_to_a_step (6285, 5, 0) == 0);
  DP_CHECK (a_hold_follows_its_rate_to_a_step (6285, 5, 1) == 0);
  DP_CHECK (a_blob_taken_mid_hold_resumes () == 0);
  DP_CHECK (the_remainders_upper_edge_is_produced_and_accepted () == 0);
  DP_CHECK (
      a_one_sample_call_that_completes_a_carried_period_keeps_every_partial ()
      == 0);
  {
    /* segments 1, 4 and 8 at 2, 2.2 and 3.4219 samples a chip; then the code
       at 1.9 times nominal, inside the bound's headroom of two */
    static const double spss[] = { 2.0, 2.2, 3.4219 };
    static const size_t segs[] = { 1, 4, 8 };
    for (size_t i = 0; i < 3; i++)
      for (size_t j = 0; j < 3; j++)
        DP_CHECK (
            random_call_lengths_never_exceed_the_bound (spss[i], segs[j], 0.0)
            == 0);
    DP_CHECK (random_call_lengths_never_exceed_the_bound (2.2, 4, 0.9) == 0);
  }
  DP_CHECK (a_rate_past_the_headroom_drops_without_aborting () == 0);

  DP_TEST_END ("test_dll_core");
}
