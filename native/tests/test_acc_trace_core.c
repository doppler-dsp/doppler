#include "doppler/acc_trace/acc_trace_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TOL 1e-5f

/* The per-bin mean of a long capture, against which the trace's is judged:
   Kahan-compensated, with the intermediates VOLATILE so that the library's
   -ffast-math cannot cancel the compensation away (t - s - y is not zero to
   a compiler that must reload t). Its error is a few ulps whatever the
   frame count, three orders below the bound it judges. */
typedef struct
{
  double s, c;
} kahan_t;

static void
kahan_add (kahan_t *k, double x)
{
  volatile double y = x - k->c;
  volatile double t = k->s + y;
  volatile double d = t - k->s;
  k->c              = d - y;
  k->s              = t;
}

/* The scalar statement of each mode's fold, branch for branch: what every
   vectorized form must equal bit for bit. */
static void
fold_ref (int mode, double alpha, double *acc, const float *p, size_t n,
          uint64_t count)
{
  for (size_t i = 0; i < n; i++)
    {
      const double v = (double)p[i];
      if (count == 0)
        acc[i] = v;
      else if (mode == ACC_TRACE_MEAN)
        acc[i] = acc[i] + v;
      else if (mode == ACC_TRACE_EXP)
        acc[i] = acc[i] + alpha * (v - acc[i]);
      else if (mode == ACC_TRACE_MAXHOLD)
        {
          if (v > acc[i])
            acc[i] = v;
        }
      else if (v < acc[i])
        acc[i] = v;
    }
}

int
main (void)
{

  /* ── lifecycle + invalid args ───────────────────────────────────────── */
  {
    DP_CHECK (dp_acc_trace_create (0, 0, 0.1) == NULL); /* n == 0 */
    /* n doubles past what a size_t can count in bytes: refused before
       calloc, whose overflow ASan and TSan report as an error. */
    DP_CHECK (dp_acc_trace_create (SIZE_MAX / sizeof (double) + 1, 0, 0.1)
              == NULL);
    DP_CHECK (dp_acc_trace_create (8, -1, 0.1) == NULL); /* bad mode */
    DP_CHECK (dp_acc_trace_create (8, 4, 0.1) == NULL);  /* bad mode */
    dp_acc_trace_destroy (NULL);                         /* must not crash */

    dp_acc_trace_state_t *obj = dp_acc_trace_create (8, ACC_TRACE_MEAN, 0.1);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->n == 8);
    DP_CHECK (obj->count == 0);
    DP_CHECK (dp_acc_trace_value_max_out (obj) == 8);

    /* value before any frame → 0 (None in Python). */
    float out[8];
    DP_CHECK (dp_acc_trace_value (obj, 8, out, 8) == 0);
    dp_acc_trace_destroy (obj);
  }

  /* ── MEAN: average of two frames, per bin ───────────────────────────── */
  {
    dp_acc_trace_state_t *obj  = dp_acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    float                 a[4] = { 1, 3, 5, 7 };
    float                 b[4] = { 3, 5, 7, 9 };
    dp_acc_trace_accumulate (obj, a, 4);
    DP_CHECK (obj->count == 1);
    dp_acc_trace_accumulate (obj, b, 4);
    DP_CHECK (obj->count == 2);
    float out[4];
    DP_CHECK (dp_acc_trace_value (obj, 4, out, 4) == 4);
    const float want[4] = { 2, 4, 6, 8 };
    for (int i = 0; i < 4; i++)
      DP_CHECK (fabsf (out[i] - want[i]) < TOL);

    /* reset clears the running trace and counter. */
    dp_acc_trace_reset (obj);
    DP_CHECK (obj->count == 0);
    DP_CHECK (dp_acc_trace_value (obj, 4, out, 4) == 0);
    dp_acc_trace_destroy (obj);
  }

  /* ── MEAN is order-independent and stable over three frames ─────────── */
  {
    dp_acc_trace_state_t *obj   = dp_acc_trace_create (2, ACC_TRACE_MEAN, 0.1);
    float                 f0[2] = { 0, 30 };
    float                 f1[2] = { 6, 60 };
    float                 f2[2] = { 9, 90 };
    dp_acc_trace_accumulate (obj, f0, 2);
    dp_acc_trace_accumulate (obj, f1, 2);
    dp_acc_trace_accumulate (obj, f2, 2);
    float out[2];
    dp_acc_trace_value (obj, 2, out, 2);
    DP_CHECK (fabsf (out[0] - 5.0f) < TOL);  /* (0+6+9)/3    */
    DP_CHECK (fabsf (out[1] - 60.0f) < TOL); /* (30+60+90)/3 */
    dp_acc_trace_destroy (obj);
  }

  /* ── EXP: seed then single update with alpha = 0.5 ──────────────────── */
  {
    dp_acc_trace_state_t *obj  = dp_acc_trace_create (2, ACC_TRACE_EXP, 0.5);
    float                 s[2] = { 10, 20 };
    float                 u[2] = { 2, 4 };
    dp_acc_trace_accumulate (obj, s, 2); /* seeds acc = s */
    dp_acc_trace_accumulate (obj, u, 2); /* 0.5*u + 0.5*s */
    float out[2];
    dp_acc_trace_value (obj, 2, out, 2);
    DP_CHECK (fabsf (out[0] - 6.0f) < TOL);  /* 0.5*2 + 0.5*10 */
    DP_CHECK (fabsf (out[1] - 12.0f) < TOL); /* 0.5*4 + 0.5*20 */
    dp_acc_trace_destroy (obj);
  }

  /* ── EXP alpha lies in (0, 1], at create and through the setter ────────
   * Outside it the EMA is not an average: 0 never leaves the first frame,
   * -0.5 extrapolates away from the data (a power trace goes negative), 1.5
   * saturates to pass-through in dp_ema_step, NaN poisons every bin
   * (#1911 (c)).  Each refusal sits beside its precondition: the nearest
   * valid values are accepted, and the three modes that never read alpha
   * accept every value, at create and through the setter. */
  {
    const double bad[]  = { 0.0, -0.5, 1.5, NAN };
    const double good[] = { 1.0, 0.25, 1e-9 };
    const int    no_read[]
        = { ACC_TRACE_MEAN, ACC_TRACE_MAXHOLD, ACC_TRACE_MINHOLD };
    const size_t n_bad     = sizeof bad / sizeof bad[0];
    const size_t n_good    = sizeof good / sizeof good[0];
    const size_t n_no_read = sizeof no_read / sizeof no_read[0];

    for (size_t i = 0; i < n_bad; i++)
      {
        DP_CHECK (dp_acc_trace_create (4, ACC_TRACE_EXP, bad[i]) == NULL);
        for (size_t k = 0; k < n_no_read; k++)
          {
            dp_acc_trace_state_t *m
                = dp_acc_trace_create (4, no_read[k], bad[i]);
            DP_CHECK (m != NULL);
            if (m)
              DP_CHECK (dp_acc_trace_set_alpha (m, bad[i]) == DP_OK);
            dp_acc_trace_destroy (m);
          }
      }
    for (size_t i = 0; i < n_good; i++)
      {
        dp_acc_trace_state_t *e
            = dp_acc_trace_create (4, ACC_TRACE_EXP, good[i]);
        DP_CHECK (e != NULL);
        dp_acc_trace_destroy (e);
      }

    /* The setter applies the same rule: a refused alpha leaves the one the
     * trace had, and the trace still averages with it. */
    dp_acc_trace_state_t *e = dp_acc_trace_create (2, ACC_TRACE_EXP, 0.5);
    DP_REQUIRE (e != NULL);
    for (size_t i = 0; i < n_bad; i++)
      {
        DP_CHECK (dp_acc_trace_set_alpha (e, bad[i]) == DP_ERR_INVALID);
        DP_CHECK (e->alpha == 0.5);
      }
    float s[2] = { 10, 20 };
    float u[2] = { 2, 4 };
    float out[2];
    dp_acc_trace_accumulate (e, s, 2);
    dp_acc_trace_accumulate (e, u, 2);
    dp_acc_trace_value (e, 2, out, 2);
    DP_CHECK (fabsf (out[0] - 6.0f) < TOL); /* 0.5*2 + 0.5*10, not -0.5's */
    DP_CHECK (fabsf (out[1] - 12.0f) < TOL);
    for (size_t i = 0; i < n_good; i++)
      {
        DP_CHECK (dp_acc_trace_set_alpha (e, good[i]) == DP_OK);
        DP_CHECK (e->alpha == good[i]);
      }
    dp_acc_trace_destroy (e);
  }

  /* ── MAXHOLD / MINHOLD per bin ──────────────────────────────────────── */
  {
    dp_acc_trace_state_t *mx = dp_acc_trace_create (3, ACC_TRACE_MAXHOLD, 0.1);
    dp_acc_trace_state_t *mn = dp_acc_trace_create (3, ACC_TRACE_MINHOLD, 0.1);
    float                 p0[3] = { 1, 5, 2 };
    float                 p1[3] = { 4, 3, 6 };
    dp_acc_trace_accumulate (mx, p0, 3);
    dp_acc_trace_accumulate (mx, p1, 3);
    dp_acc_trace_accumulate (mn, p0, 3);
    dp_acc_trace_accumulate (mn, p1, 3);
    float omx[3], omn[3];
    dp_acc_trace_value (mx, 3, omx, 3);
    dp_acc_trace_value (mn, 3, omn, 3);
    const float wmx[3] = { 4, 5, 6 };
    const float wmn[3] = { 1, 3, 2 };
    for (int i = 0; i < 3; i++)
      {
        DP_CHECK (fabsf (omx[i] - wmx[i]) < TOL);
        DP_CHECK (fabsf (omn[i] - wmn[i]) < TOL);
      }
    dp_acc_trace_destroy (mx);
    dp_acc_trace_destroy (mn);
  }

  /* ── short frame is ignored ─────────────────────────────────────────── */
  {
    dp_acc_trace_state_t *obj = dp_acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    float                 shrt[2] = { 1, 2 };
    dp_acc_trace_accumulate (obj, shrt, 2); /* p_len < n → no-op */
    DP_CHECK (obj->count == 0);
    dp_acc_trace_destroy (obj);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    dp_acc_trace_state_t *a    = dp_acc_trace_create (8, 0, 0.1);
    const float           p[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    float                 out[8];
    DP_CHECK (a != NULL);
    dp_acc_trace_accumulate (a, p, 8);

    for (int i = 0; i < 8; i++)
      out[i] = 42.0f;
    DP_CHECK (dp_acc_trace_value (a, 8, out, 3) == 3);
    for (int i = 3; i < 8; i++)
      DP_CHECK (out[i] == 42.0f); /* tail untouched */

    /* Zero capacity emits nothing. */
    for (int i = 0; i < 8; i++)
      out[i] = 42.0f;
    DP_CHECK (dp_acc_trace_value (a, 8, out, 0) == 0);
    for (int i = 0; i < 8; i++)
      DP_CHECK (out[i] == 42.0f);
    dp_acc_trace_destroy (a);
  }

  /* serializable state — field-wise trace + count round-trips + rejects. */
  {
    dp_acc_trace_state_t *a = dp_acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    dp_acc_trace_state_t *b = dp_acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    DP_CHECK (a != NULL && b != NULL);
    const float f1[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float f2[4] = { 0.5f, -1.0f, 2.0f, 0.0f };
    dp_acc_trace_accumulate (a, f1, 4);
    dp_acc_trace_accumulate (a, f2, 4);
    DP_STATE_ROUNDTRIP_TEST (dp_acc_trace, a, b);
    DP_CHECK (b->count == a->count);
    DP_CHECK (memcmp (b->acc, a->acc, a->n * sizeof (double)) == 0);
    dp_acc_trace_destroy (a);
    dp_acc_trace_destroy (b);
  }

  /* ── a runtime alpha travels in the blob (#2000) ─────────────────────────
   * alpha can change after create (dp_acc_trace_set_alpha), so a resume that
   * took it from create() went on averaging with the old one: one frame
   * later the uninterrupted trace read 2.0 and the resumed one 1.2.  The
   * blob carries it now, behind the header, the u32 mode and the u64 count,
   * and set_state applies the setter's own rule to it: an alpha the setter
   * would refuse is refused, and the state is left as it was. */
  {
    const float           f1[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float           f2[4] = { 9.0f, 7.0f, 5.0f, 3.0f };
    dp_acc_trace_state_t *a     = dp_acc_trace_create (4, ACC_TRACE_EXP, 0.1);
    dp_acc_trace_state_t *b     = dp_acc_trace_create (4, ACC_TRACE_EXP, 0.1);
    DP_REQUIRE (a && b);
    dp_acc_trace_accumulate (a, f1, 4);
    DP_CHECK (dp_acc_trace_set_alpha (a, 0.5) == DP_OK);
    const size_t   nb   = dp_acc_trace_state_bytes (a);
    unsigned char *blob = malloc (nb);
    DP_REQUIRE (blob != NULL);
    dp_acc_trace_get_state (a, blob);
    DP_CHECK (dp_acc_trace_set_state (b, blob) == DP_OK);
    DP_CHECK (b->alpha == 0.5);
    dp_acc_trace_accumulate (a, f2, 4);
    dp_acc_trace_accumulate (b, f2, 4);
    DP_CHECK (memcmp (b->acc, a->acc, a->n * sizeof (double)) == 0);

    /* The setter's rule, at the blob: each value it refuses is refused here,
     * and a refused blob leaves the state untouched.  b folds one more frame
     * first, so its count differs from the blob's and a count written before
     * the check would show. */
    dp_acc_trace_accumulate (b, f1, 4);
    const size_t at
        = sizeof (dp_state_hdr_t) + sizeof (uint32_t) + sizeof (uint64_t);
    unsigned char *before = malloc (nb), *after = malloc (nb);
    DP_REQUIRE (before && after);
    const double bad[] = { 0.0, -0.5, 1.5, NAN };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
      {
        dp_acc_trace_get_state (a, blob);
        memcpy (blob + at, &bad[i], sizeof bad[i]);
        dp_acc_trace_get_state (b, before);
        DP_CHECK (dp_acc_trace_set_state (b, blob) == DP_ERR_INVALID);
        dp_acc_trace_get_state (b, after);
        DP_CHECK (memcmp (before, after, nb) == 0);
      }

    /* Precondition: mean never reads alpha, so as at the setter, any value
     * restores there. */
    dp_acc_trace_state_t *m = dp_acc_trace_create (4, ACC_TRACE_MEAN, 0.1);
    DP_REQUIRE (m != NULL);
    dp_acc_trace_get_state (m, blob);
    memcpy (blob + at, &bad[1], sizeof bad[1]);
    DP_CHECK (dp_acc_trace_set_state (m, blob) == DP_OK);
    DP_CHECK (m->alpha == -0.5);

    free (blob);
    free (before);
    free (after);
    dp_acc_trace_destroy (a);
    dp_acc_trace_destroy (b);
    dp_acc_trace_destroy (m);
  }

  /* ── mode is a reject key ─────────────────────────────────────────────────
   * A blob from a trace in another mode is another configuration.  A mean
   * trace's blob restored into an exp instance came back OK, with an alpha
   * mean mode never checked, and the mean trace went on as an EMA.  Every
   * pair of modes that differ is refused, with the target left as it was;
   * every pair that agrees restores. */
  {
    const float f1[4]    = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float f2[4]    = { 9.0f, 7.0f, 5.0f, 3.0f };
    const int   modes[4] = { ACC_TRACE_MEAN, ACC_TRACE_EXP, ACC_TRACE_MAXHOLD,
                             ACC_TRACE_MINHOLD };
    for (size_t i = 0; i < 4; i++)
      for (size_t j = 0; j < 4; j++)
        {
          dp_acc_trace_state_t *src = dp_acc_trace_create (4, modes[i], 0.25);
          dp_acc_trace_state_t *dst = dp_acc_trace_create (4, modes[j], 0.5);
          DP_REQUIRE (src && dst);
          dp_acc_trace_accumulate (src, f1, 4);
          dp_acc_trace_accumulate (dst, f2, 4);
          const size_t   nb     = dp_acc_trace_state_bytes (src);
          unsigned char *blob   = malloc (nb);
          unsigned char *before = malloc (nb), *after = malloc (nb);
          DP_REQUIRE (blob && before && after);
          dp_acc_trace_get_state (src, blob);
          dp_acc_trace_get_state (dst, before);
          const int rc = dp_acc_trace_set_state (dst, blob);
          if (i == j)
            DP_CHECK (rc == DP_OK);
          else
            {
              DP_CHECK (rc == DP_ERR_INVALID);
              dp_acc_trace_get_state (dst, after);
              DP_CHECK (memcmp (before, after, nb) == 0);
            }
          free (blob);
          free (before);
          free (after);
          dp_acc_trace_destroy (src);
          dp_acc_trace_destroy (dst);
        }
  }

  /* ── mean mode holds the per-bin SUM, and value() divides (#2094) ───────
   * The trace is the sum the header says, not the mean: acc[i] is the sum of
   * the frames, bit for bit, and value() is (float)(acc / count). */
  {
    dp_acc_trace_state_t *a = dp_acc_trace_create (3, ACC_TRACE_MEAN, 0.1);
    DP_REQUIRE (a != NULL);
    const float f[3][3] = { { 1, 2, 4 }, { 0.5f, 8, 16 }, { 3, 0.25f, 1 } };
    for (int k = 0; k < 3; k++)
      dp_acc_trace_accumulate (a, f[k], 3);
    float out[3];
    DP_REQUIRE (dp_acc_trace_value (a, 3, out, 3) == 3);
    for (int i = 0; i < 3; i++)
      {
        const double sum = (double)f[0][i] + f[1][i] + f[2][i];
        DP_CHECK (a->acc[i] == sum);
        DP_CHECK (out[i] == (float)(sum / 3.0));
      }
    dp_acc_trace_destroy (a);
  }

  /* ── the mean over a long capture: the sum's measured error (#2094) ─────
   * 10^6 frames of exponential power (mean 1: a noise-only PSD bin) in 32
   * bins. The mean the trace holds, acc / count in double, against the
   * compensated reference above. In this build (x86-64-v2, -ffast-math) the
   * worst bin is 1.8e-15; the bound is 1e-14. The Welford update the sum
   * replaced measured 8.5e-14 on the same data shape, and float with
   * compensation 2.9e-8 (docs/design/spectrogram-measurements.md, "The
   * trace's mean is a sum"). A return to Welford is refused by the section
   * above: the trace would no longer be the sum. */
  {
    enum
    {
      NB     = 32,
      FRAMES = 1000000
    };
    dp_acc_trace_state_t *a = dp_acc_trace_create (NB, ACC_TRACE_MEAN, 0.1);
    DP_REQUIRE (a != NULL);
    static kahan_t ref[NB];
    float          p[NB];
    uint32_t       rng = 0x2094u;
    for (long k = 0; k < FRAMES; k++)
      {
        for (int i = 0; i < NB; i++)
          {
            /* dp_uni is (0, 1), never 0, so the log is finite */
            p[i] = (float)-log (dp_uni (&rng));
            kahan_add (&ref[i], (double)p[i]);
          }
        dp_acc_trace_accumulate (a, p, NB);
      }
    double worst = 0.0;
    for (int i = 0; i < NB; i++)
      {
        const double want = ref[i].s / FRAMES;
        const double got  = a->acc[i] / (double)a->count;
        const double e    = fabs (got - want) / want;
        worst             = e > worst ? e : worst;
      }
    DP_CHECK (a->count == FRAMES);
    DP_CHECK_NEAR (worst, 0.0, 1e-14);
    dp_acc_trace_destroy (a);
  }

  /* ── every mode equals its scalar statement, at any length (#2094) ───────
   * The folds are written to vectorize, so the length that is not a whole
   * number of vectors, and the frames that do and do not move a hold, must
   * still give the scalar fold's trace bit for bit. Lengths 1 to 67 cover
   * every remainder of every width a shipped build uses (2, 4, 8 doubles). */
  {
    static const int modes[4] = { ACC_TRACE_MEAN, ACC_TRACE_EXP,
                                  ACC_TRACE_MAXHOLD, ACC_TRACE_MINHOLD };
    uint32_t         rng      = 0xF01Du;
    int              same     = 1;
    for (int m = 0; m < 4; m++)
      for (size_t n = 1; n <= 67; n++)
        {
          dp_acc_trace_state_t *a = dp_acc_trace_create (n, modes[m], 0.3);
          double               *r = (double *)calloc (n, sizeof *r);
          float                *p = (float *)malloc (n * sizeof *p);
          DP_REQUIRE (a && r && p);
          for (uint64_t k = 0; k < 9; k++)
            {
              for (size_t i = 0; i < n; i++)
                p[i] = (float)dp_gauss (&rng);
              fold_ref (modes[m], 0.3, r, p, n, k);
              dp_acc_trace_accumulate (a, p, n);
            }
          same &= memcmp (a->acc, r, n * sizeof *r) == 0;
          free (p);
          free (r);
          dp_acc_trace_destroy (a);
        }
    DP_CHECK (same);
  }

  /* ── the first frame seeds every mode; a NaN never replaces a hold ──────
   * One frame, then value(), returns that frame bit for bit in every mode.
   * Then a frame with a NaN in bin 1: maxhold and minhold keep what bin 1
   * held (the compare is false, so the select keeps the trace), and the
   * other bins still move. The NaN half rests on the declared
   * -fno-finite-math-only (CMakeLists.txt:146). It cannot see that flag
   * removed itself (GCC's blend keeps the trace either way, measured on
   * #2105), so test_fp_policy.c is the canary that does. */
  {
    static const int modes[4] = { ACC_TRACE_MEAN, ACC_TRACE_EXP,
                                  ACC_TRACE_MAXHOLD, ACC_TRACE_MINHOLD };
    const float      seed[3]  = { 0.75f, -2.5f, 3.125f };
    for (int m = 0; m < 4; m++)
      {
        dp_acc_trace_state_t *a = dp_acc_trace_create (3, modes[m], 0.5);
        DP_REQUIRE (a != NULL);
        float out[3];
        dp_acc_trace_accumulate (a, seed, 3);
        DP_CHECK (dp_acc_trace_value (a, 3, out, 3) == 3);
        DP_CHECK (memcmp (out, seed, sizeof out) == 0);
        dp_acc_trace_destroy (a);
      }
    const float nan_frame[3] = { 9.0f, NAN, -9.0f };
    for (int m = 2; m < 4; m++)
      {
        dp_acc_trace_state_t *a = dp_acc_trace_create (3, modes[m], 0.5);
        DP_REQUIRE (a != NULL);
        float out[3];
        dp_acc_trace_accumulate (a, seed, 3);
        dp_acc_trace_accumulate (a, nan_frame, 3);
        DP_CHECK (dp_acc_trace_value (a, 3, out, 3) == 3);
        DP_CHECK (out[1] == seed[1]); /* the NaN did not replace it */
        DP_CHECK (out[m == 2 ? 0 : 2] == nan_frame[m == 2 ? 0 : 2]);
        dp_acc_trace_destroy (a);
      }
  }

  /* ── the restore checks what the sum rests on (#2094) ──────────────────
   * The count divides every mean reading, so a forged blob that pairs a
   * count of 0 with a trace is refused, in every mode: reset() leaves a
   * trace of +0.0 bits, so a stray value and a -0.0 are both forged. The
   * refusal leaves the trace as it was. The positive control: a fresh
   * trace's own blob (count 0, all +0.0) restores. And nothing accumulate
   * can reach is refused: an Inf or NaN trace's blob restores as taken. */
  {
    static const int modes[4] = { ACC_TRACE_MEAN, ACC_TRACE_EXP,
                                  ACC_TRACE_MAXHOLD, ACC_TRACE_MINHOLD };
    for (int m = 0; m < 4; m++)
      {
        dp_acc_trace_state_t *a = dp_acc_trace_create (4, modes[m], 0.5);
        dp_acc_trace_state_t *b = dp_acc_trace_create (4, modes[m], 0.5);
        DP_REQUIRE (a && b);
        const size_t   nb    = dp_acc_trace_state_bytes (a);
        unsigned char *fresh = malloc (nb), *lie = malloc (nb);
        unsigned char *held = malloc (nb), *after = malloc (nb);
        DP_REQUIRE (fresh && lie && held && after);
        dp_acc_trace_get_state (a, fresh); /* count 0, trace +0.0 */
        DP_CHECK (dp_acc_trace_set_state (b, fresh) == DP_OK);
        const float f[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        dp_acc_trace_accumulate (b, f, 4); /* b holds a trace to keep */
        dp_acc_trace_get_state (b, held);
        const size_t        at = sizeof (dp_state_hdr_t) + sizeof (uint32_t)
                                 + sizeof (uint64_t) + sizeof (double);
        static const double forged[] = { 1.0, -0.0, 1e-300 };
        for (size_t k = 0; k < 3; k++)
          {
            memcpy (lie, fresh, nb);
            memcpy (lie + at + 2 * sizeof (double), &forged[k],
                    sizeof (double));
            DP_CHECK (dp_acc_trace_set_state (b, lie) == DP_ERR_INVALID);
            dp_acc_trace_get_state (b, after);
            DP_CHECK (memcmp (after, held, nb) == 0);
          }
        free (fresh);
        free (lie);
        free (held);
        free (after);
        dp_acc_trace_destroy (a);
        dp_acc_trace_destroy (b);
      }

    /* reachable non-finite traces round-trip: +Inf, and Inf - Inf = NaN */
    dp_acc_trace_state_t *a = dp_acc_trace_create (2, ACC_TRACE_MEAN, 0.1);
    dp_acc_trace_state_t *b = dp_acc_trace_create (2, ACC_TRACE_MEAN, 0.1);
    DP_REQUIRE (a && b);
    const float inf2[2] = { INFINITY, INFINITY };
    const float mix[2]  = { 1.0f, -INFINITY };
    dp_acc_trace_accumulate (a, inf2, 2);
    dp_acc_trace_accumulate (a, mix, 2); /* bin 0 +Inf, bin 1 NaN */
    DP_CHECK (isinf (a->acc[0]) && a->acc[0] > 0 && isnan (a->acc[1]));
    DP_STATE_ROUNDTRIP_TEST (dp_acc_trace, a, b);
    dp_acc_trace_destroy (a);
    dp_acc_trace_destroy (b);
  }

  /* ── an Inf frame stays +Inf in a mean trace (#2094) ────────────────────
   * The sum of +Inf and a finite frame is +Inf, so the mean reads +Inf.
   * The Welford update it replaced read NaN (Inf - Inf in its correction).
   * Stated in the changelog as a behaviour change. */
  {
    dp_acc_trace_state_t *a = dp_acc_trace_create (1, ACC_TRACE_MEAN, 0.1);
    DP_REQUIRE (a != NULL);
    const float inf = INFINITY, one = 1.0f;
    float       out;
    dp_acc_trace_accumulate (a, &inf, 1);
    dp_acc_trace_accumulate (a, &one, 1);
    dp_acc_trace_accumulate (a, &one, 1);
    DP_CHECK (dp_acc_trace_value (a, 1, &out, 1) == 1);
    DP_CHECK (isinf (out) && out > 0.0f);
    dp_acc_trace_destroy (a);
  }

  /* ── a NaN never replaces a hold, on the packed path too (#2094) ───────
   * n = 67 runs the vectorized body and its remainder in every shipped
   * build (widths 2, 4, 8 doubles), so NaNs go in the first lane, lanes on
   * both sides of a vector boundary, and the last bin. Each keeps what it
   * held; every other bin still moves. This rests on the declared
   * -fno-finite-math-only (CMakeLists.txt:146), held by test_fp_policy.c. */
  {
    enum
    {
      N = 67
    };
    static const size_t at[] = { 0, 1, 7, 8, 15, 16, 31, 32, 63, 64, 66 };
    for (int mode = ACC_TRACE_MAXHOLD; mode <= ACC_TRACE_MINHOLD; mode++)
      {
        dp_acc_trace_state_t *a = dp_acc_trace_create (N, mode, 0.5);
        DP_REQUIRE (a != NULL);
        float seed[N], nan_frame[N], out[N];
        for (int i = 0; i < N; i++)
          {
            seed[i]      = (float)i;
            nan_frame[i] = mode == ACC_TRACE_MAXHOLD ? 1000.0f : -1000.0f;
          }
        for (size_t k = 0; k < sizeof at / sizeof *at; k++)
          nan_frame[at[k]] = NAN;
        dp_acc_trace_accumulate (a, seed, N);
        dp_acc_trace_accumulate (a, nan_frame, N);
        DP_REQUIRE (dp_acc_trace_value (a, N, out, N) == N);
        int kept = 1, moved = 1;
        for (int i = 0; i < N; i++)
          {
            int is_nan_bin = 0;
            for (size_t k = 0; k < sizeof at / sizeof *at; k++)
              is_nan_bin |= (size_t)i == at[k];
            if (is_nan_bin)
              kept &= out[i] == seed[i];
            else
              moved &= out[i] == nan_frame[i];
          }
        DP_CHECK (kept);
        DP_CHECK (moved);
        dp_acc_trace_destroy (a);
      }
  }

  /* ── a blob of another layout version is refused ─────────────────────────
   * Rewriting only the header's version leaves every other byte valid, so
   * this fails if and only if the version is consulted.  Version 1 is the
   * layout v0.65.0 shipped, without mode or alpha; version 2 carried them
   * with a mean trace holding the mean, which version 3's sum would misread
   * by a factor of the count (#2094).  Both are named, not derived from
   * ACC_TRACE_STATE_VERSION, so putting the constant back turns this red.
   * The next version is refused too, so the check is not >=. */
  {
    dp_acc_trace_state_t *t = dp_acc_trace_create (4, ACC_TRACE_EXP, 0.25);
    DP_REQUIRE (t != NULL);
    const size_t   nb   = dp_acc_trace_state_bytes (t);
    unsigned char *blob = malloc (nb);
    DP_REQUIRE (blob != NULL);
    dp_acc_trace_get_state (t, blob);
    DP_CHECK (dp_acc_trace_set_state (t, blob) == DP_OK);
    dp_state_hdr_t hdr;
    memcpy (&hdr, blob, sizeof hdr);
    hdr.version = 1u;
    memcpy (blob, &hdr, sizeof hdr);
    DP_CHECK (dp_acc_trace_set_state (t, blob) == DP_ERR_INVALID);
    hdr.version = 2u;
    memcpy (blob, &hdr, sizeof hdr);
    DP_CHECK (dp_acc_trace_set_state (t, blob) == DP_ERR_INVALID);
    hdr.version = (uint16_t)(ACC_TRACE_STATE_VERSION + 1u);
    memcpy (blob, &hdr, sizeof hdr);
    DP_CHECK (dp_acc_trace_set_state (t, blob) == DP_ERR_INVALID);
    free (blob);
    dp_acc_trace_destroy (t);
  }

  DP_TEST_END ("test_acc_trace_core");
}
