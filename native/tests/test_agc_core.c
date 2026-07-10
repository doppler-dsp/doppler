#include "agc/agc_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
static inline int
_almost_eq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_almost_eq_c (float complex a, float complex b, float tol)
{
  return _almost_eq (crealf (a), crealf (b), tol)
         && _almost_eq (cimagf (a), cimagf (b), tol);
}
#define ALMOST_EQ(a, b, tol) _almost_eq ((float)(a), (float)(b), tol)
#define ALMOST_EQ_C(a, b, tol)                                                \
  _almost_eq_c ((float complex) (a), (float complex) (b), tol)

/* Feed n copies of a constant-magnitude sample; return the power of the
 * final output in dB.  Used to probe the converged loop state. */
static double
run_const (agc_state_t *agc, float complex x, size_t n)
{
  float complex y = 0.0f + 0.0f * I;
  for (size_t i = 0; i < n; i++)
    y = agc_step (agc, x);
  double p = (double)crealf (y) * crealf (y) + (double)cimagf (y) * cimagf (y);
  return 10.0 * log10 (p);
}

int
main (void)
{
  int _fails = 0;

  /* unit-magnitude direction: (0.6 + 0.8j) has |.| == 1, so scaling it
   * by A gives an input of magnitude A exercising both re and im. */
  const float complex dir = 0.6f + 0.8f * I;

  /* ---- lifecycle ---- */
  agc_state_t *obj = agc_create (0.0, 0.0025, 0.05);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* create seeds gain_db = 0 dB and p_avg = reference power = 10^0 = 1 */
  CHECK (ALMOST_EQ (obj->gain_db, 0.0, 1e-6));
  CHECK (ALMOST_EQ (obj->p_avg, 1.0, 1e-6));

  /* ---- convergence: a loud input is driven down to ref_db (0 dB) ---- */
  double out_db = run_const (obj, dir * 10.0f, 4000);
  CHECK (ALMOST_EQ (out_db, 0.0, 0.5));
  /* gain settles to -20 dB: 20*log10(10) of input attenuation */
  CHECK (ALMOST_EQ (obj->gain_db, -20.0, 0.5));
  agc_destroy (obj);

  /* ---- linear-in-dB: a quiet and a loud input settle to the same
          level within the same sample budget.  A level-dependent loop
          would leave one of them far from ref. ---- */
  agc_state_t *lo    = agc_create (0.0, 0.0025, 0.05);
  agc_state_t *hi    = agc_create (0.0, 0.0025, 0.05);
  double       lo_db = run_const (lo, dir * 0.01f, 4000);  /* -40 dB input */
  double       hi_db = run_const (hi, dir * 100.0f, 4000); /* +40 dB input */
  CHECK (ALMOST_EQ (lo_db, 0.0, 0.5));
  CHECK (ALMOST_EQ (hi_db, 0.0, 0.5));
  CHECK (ALMOST_EQ (lo_db, hi_db, 0.5));
  agc_destroy (lo);
  agc_destroy (hi);

  /* ---- non-zero reference: output converges to ref_db, not 0 dB ---- */
  agc_state_t *r    = agc_create (-6.0, 0.0025, 0.05);
  double       r_db = run_const (r, dir * 3.0f, 4000);
  CHECK (ALMOST_EQ (r_db, -6.0, 0.5));
  agc_destroy (r);

  /* ---- fast-math approximations agree with the exact functions ---- */
  CHECK (ALMOST_EQ (agc_exp10_ (0.0), 1.0, 1e-3));
  CHECK (ALMOST_EQ (agc_exp10_ (2.0), 100.0, 0.2));
  CHECK (ALMOST_EQ (agc_exp10_ (-1.5), 0.0316227766, 1e-3));
  CHECK (ALMOST_EQ (agc_log10_ (1.0), 0.0, 1e-3));
  CHECK (ALMOST_EQ (agc_log10_ (100.0), 2.0, 1e-2));
  CHECK (ALMOST_EQ (agc_log10_ (0.001), -3.0, 1e-2));

  /* ---- decimated steps() converges to the same gain as a per-sample
          step() loop: the block-rate control loop reaches the same
          steady state, just by a coarser path. ---- */
  {
    static float complex in[3000];
    static float complex blk[3000];
    agc_state_t         *a = agc_create (0.0, 0.005, 0.1);
    agc_state_t         *b = agc_create (0.0, 0.005, 0.1);
    for (size_t i = 0; i < 3000; i++)
      in[i] = dir * 4.0f;
    agc_steps (a, in, blk, 3000);
    for (size_t i = 0; i < 3000; i++)
      (void)agc_step (b, in[i]);
    CHECK (ALMOST_EQ (a->gain_db, b->gain_db, 0.3));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* ---- steps() supports in-place operation (output aliases input) ---- */
  {
    float complex buf[64], ref[64];
    agc_state_t  *a = agc_create (0.0, 0.005, 0.1);
    agc_state_t  *b = agc_create (0.0, 0.005, 0.1);
    for (size_t i = 0; i < 64; i++)
      buf[i] = dir * 5.0f;
    agc_steps (b, buf, ref, 64);
    agc_steps (a, buf, buf, 64);
    for (size_t i = 0; i < 64; i++)
      CHECK (ALMOST_EQ_C (buf[i], ref[i], 1e-6f));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* ---- decimation factor is configurable (8 / 16 / 32); every setting
          still converges the output to the reference ---- */
  {
    static float complex in[4000];
    static float complex blk[4000];
    size_t               decims[3] = { 8, 16, 32 };
    for (size_t i = 0; i < 4000; i++)
      in[i] = dir * 8.0f;
    for (int di = 0; di < 3; di++)
      {
        agc_state_t *a = agc_create (0.0, 0.002, 0.05);
        CHECK (a->decim == AGC_DECIM_DEFAULT); /* create() default */
        a->decim = decims[di];
        agc_steps (a, in, blk, 4000);
        double pw = (double)crealf (blk[3999]) * crealf (blk[3999])
                    + (double)cimagf (blk[3999]) * cimagf (blk[3999]);
        CHECK (ALMOST_EQ (10.0 * log10 (pw), 0.0, 0.5));
        agc_destroy (a);
      }
  }

  /* ---- reset restores post-create state; config is preserved ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    run_const (s, dir * 50.0f, 2000); /* perturb the loop */
    CHECK (fabs (s->gain_db) > 1.0);  /* loop has clearly moved */
    agc_reset (s);
    CHECK (ALMOST_EQ (s->gain_db, 0.0, 1e-6));
    CHECK (ALMOST_EQ (s->p_avg, 1.0, 1e-6));
    CHECK (ALMOST_EQ (s->ref_db, 0.0, 1e-6));
    CHECK (ALMOST_EQ (s->loop_bw, 0.0025, 1e-6));
    CHECK (ALMOST_EQ (s->alpha, 0.05, 1e-6));
    CHECK (ALMOST_EQ (s->clip_db, AGC_CLIP_DB_DEFAULT, 1e-6));
    agc_destroy (s);
  }

  /* ---- applied-gain telemetry: agc_get_applied_gain_db reports the gain
          the signal last saw.  At create it is unity (0 dB); at
          convergence it equals the commanded gain_db. ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    CHECK (ALMOST_EQ (agc_get_applied_gain_db (s), 0.0, 1e-6));
    run_const (s, dir * 10.0f, 4000);
    CHECK (ALMOST_EQ (agc_get_applied_gain_db (s), s->gain_db, 0.5));
    CHECK (ALMOST_EQ (agc_get_applied_gain_db (s), -20.0, 0.5));
    agc_destroy (s);
  }

  /* ---- output clip: square clip (I and Q independent), applied to the
          output only — it does not feed the detector ---- */
  {
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    CHECK (ALMOST_EQ (s->clip_db, AGC_CLIP_DB_DEFAULT, 1e-6)); /* default */
    s->clip_db = 6.0; /* L = 10^(6/20) ~ 1.995 */
    double L   = pow (10.0, 6.0 / 20.0);
    /* first step: gain is exactly unity, so output = clip(x).  re (5)
       exceeds L and clamps; im (1) is below L and is kept unchanged —
       proving the clip is square, not a circular magnitude limit. */
    float complex y = agc_step (s, 5.0f + 1.0f * I);
    CHECK (ALMOST_EQ (crealf (y), L, 0.02));
    CHECK (ALMOST_EQ (cimagf (y), 1.0, 1e-6));
    agc_destroy (s);
  }

  /* ---- clipping never perturbs the loop: the detector measures the
          unclipped signal, so gain_db evolves identically whether or
          not a clip is engaged ---- */
  {
    agc_state_t *a = agc_create (0.0, 0.0025, 0.05);
    agc_state_t *b = agc_create (0.0, 0.0025, 0.05);
    b->clip_db     = -3.0; /* aggressive clip on b only */
    for (size_t i = 0; i < 4000; i++)
      {
        (void)agc_step (a, dir * 10.0f);
        (void)agc_step (b, dir * 10.0f);
      }
    CHECK (ALMOST_EQ (a->gain_db, b->gain_db, 1e-9));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* ---- agc_steps() square-clips its block output too ---- */
  {
    static float complex in[256], out[256];
    for (size_t i = 0; i < 256; i++)
      in[i] = dir * 50.0f;
    agc_state_t *s = agc_create (0.0, 0.0025, 0.05);
    s->clip_db     = 0.0; /* L = 10^0 = 1.0 */
    agc_steps (s, in, out, 256);
    for (size_t i = 0; i < 256; i++)
      {
        CHECK (fabsf (crealf (out[i])) <= 1.0f + 1e-3f);
        CHECK (fabsf (cimagf (out[i])) <= 1.0f + 1e-3f);
      }
    agc_destroy (s);
  }

  agc_destroy (NULL); /* must be a no-op */

  /* serializable state — POD snapshot round-trips + rejects a bad envelope.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    agc_state_t *a = agc_create (0.0, 0.0025, 0.05);
    agc_state_t *b = agc_create (0.0, 0.0025, 0.05);
    CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 50; i++)
      (void)agc_step (a, 4.0f + 0.0f * I);
    DP_STATE_ROUNDTRIP_TEST (agc, a, b);
    CHECK (agc_get_applied_gain_db (b) == agc_get_applied_gain_db (a));
    agc_destroy (a);
    agc_destroy (b);
  }

  /* telemetry attach — records track the gain trajectory; blobs stay
   * deterministic (attachment zeroed); a live attachment survives
   * set_state; detach reverts to the no-op path. */
  {
    dp_tlm_t    *tlm = dp_tlm_create (256);
    agc_state_t *a   = agc_create (0.0, 0.0025, 0.05);
    CHECK (tlm != NULL && a != NULL);
    CHECK (agc_set_telemetry (a, tlm, "agc", 1) == DP_OK);
    CHECK (dp_tlm_lookup (tlm, "agc.gain_db") == a->tlm.id_gain);

    /* One record per gain update (default period 1 -> per sample); the
     * last record is the current integrator value exactly. */
    for (int i = 0; i < 32; i++)
      (void)agc_step (a, 0.5f + 0.0f * I);
    dp_tlm_rec_t recs[64];
    size_t       n = dp_tlm_read (tlm, recs, 64);
    CHECK (n == 32);
    CHECK (recs[n - 1].value == (float)a->gain_db);

    /* Blob determinism: an attached and a detached instance with the
     * same running state serialize byte-identically. */
    agc_state_t *d = agc_create (0.0, 0.0025, 0.05);
    CHECK (d != NULL);
    *d             = *a;
    d->tlm.ctx     = NULL;
    d->tlm.id_gain = 0;
    uint8_t blob_a[sizeof (dp_state_hdr_t) + sizeof (agc_state_t)];
    uint8_t blob_d[sizeof (blob_a)];
    CHECK (agc_state_bytes (a) == sizeof (blob_a));
    agc_get_state (a, blob_a);
    agc_get_state (d, blob_d);
    CHECK (memcmp (blob_a, blob_d, sizeof (blob_a)) == 0);

    /* Restore into an attached instance: running state comes from the
     * blob, the receiver's own live attachment survives. */
    dp_tlm_t    *tlm2 = dp_tlm_create (256);
    agc_state_t *b    = agc_create (0.0, 0.0025, 0.05);
    CHECK (tlm2 != NULL && b != NULL);
    CHECK (agc_set_telemetry (b, tlm2, "rx.agc", 1) == DP_OK);
    CHECK (agc_set_state (b, blob_a) == DP_OK);
    CHECK (b->gain_db == a->gain_db);
    CHECK (b->tlm.ctx == tlm2);

    /* Detach: emit sites revert to the single-branch no-op. */
    CHECK (agc_set_telemetry (a, NULL, "agc", 1) == DP_OK);
    CHECK (a->tlm.ctx == NULL);
    (void)agc_step (a, 0.5f + 0.0f * I);
    CHECK (dp_tlm_read (tlm, recs, 64) == 0);

    agc_destroy (d);
    agc_destroy (b);
    agc_destroy (a);
    dp_tlm_destroy (tlm2);
    dp_tlm_destroy (tlm);
  }

  if (_fails)
    {
      fprintf (stderr, "test_agc_core FAILED (%d)\n", _fails);
      return 1;
    }

  printf ("test_agc_core PASSED\n");
  return 0;
}
