#include "wfm_synth/wfm_synth_core.h"
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

int
main (void)
{
  int                _fails = 0;
  wfm_synth_state_t *obj
      = wfm_synth_create (0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)wfm_synth_step (obj);

  /* reset */
  wfm_synth_reset (obj);

  /* ── clean (snr >= WFM_SYNTH_SNR_CLEAN) generates no AWGN; baseband no LO ──
   */
  {
    /* clean tone with a freq offset: LO present, no AWGN */
    wfm_synth_state_t *c = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 1e5, 100.0,
                                             0, 1, 8, 7, 0, 0, 0.0);
    CHECK (c && c->awgn == NULL && c->lo != NULL);
    if (c)
      wfm_synth_destroy (c);

    /* noisy tone: AWGN present */
    wfm_synth_state_t *nz = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 1e5, 10.0,
                                              0, 1, 8, 7, 0, 0, 0.0);
    CHECK (nz && nz->awgn != NULL);
    if (nz)
      wfm_synth_destroy (nz);

    /* baseband (freq 0): no LO */
    wfm_synth_state_t *bb = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 0.0, 100.0,
                                              0, 1, 8, 7, 0, 0, 0.0);
    CHECK (bb && bb->lo == NULL && bb->awgn == NULL);
    if (bb)
      wfm_synth_destroy (bb);

    /* noise type always has AWGN, even at high snr */
    wfm_synth_state_t *ns = wfm_synth_create (WFM_SYNTH_NOISE, 1e6, 0.0, 100.0,
                                              0, 1, 8, 7, 0, 0, 0.0);
    CHECK (ns && ns->awgn != NULL);
    if (ns)
      wfm_synth_destroy (ns);
  }

  /* ── chirp (LFM): linear sweep, phase-continuous, byte-identical paths ────
   */
  {
    /* A clean chirp builds neither a static LO (it synthesises its own swept
     * carrier) nor AWGN; an up-chirp sweeps f_start→f_end over its span. */
    const double       fs = 1e6, f0 = 1e5, f1 = 3e5;
    const size_t       N  = 4096;
    wfm_synth_state_t *cu = wfm_synth_create (WFM_SYNTH_CHIRP, fs, f0, 100.0,
                                              0, 1, 8, 7, 0, 0, f1);
    CHECK (cu && cu->lo == NULL && cu->awgn == NULL);
    wfm_synth_set_chirp_span (cu, N);

    float complex *y = malloc (N * sizeof *y);
    CHECK (y != NULL);
    wfm_synth_steps (cu, y, N);

    /* unit magnitude everywhere (a pure FM tone has constant envelope) */
    CHECK (ALMOST_EQ (cabsf (y[0]), 1.0f, 1e-4f));
    CHECK (ALMOST_EQ (cabsf (y[N / 2]), 1.0f, 1e-4f));
    CHECK (ALMOST_EQ (cabsf (y[N - 1]), 1.0f, 1e-4f));

    /* instantaneous frequency rises: estimate it from the phase increment
     * (cycles/sample) at the start vs. the end of the sweep. */
    double w_lo = carg (y[1] * conjf (y[0])) / 6.283185307179586; /* ≈ f0/fs */
    double w_hi
        = carg (y[N - 1] * conjf (y[N - 2])) / 6.283185307179586; /* ≈ f1/fs */
    CHECK (ALMOST_EQ (w_lo, f0 / fs, 2e-3f));
    CHECK (ALMOST_EQ (w_hi, f1 / fs, 2e-3f));

    /* step() and steps() must agree bit-for-bit (the #67 lesson). */
    wfm_synth_state_t *cs = wfm_synth_create (WFM_SYNTH_CHIRP, fs, f0, 100.0,
                                              0, 1, 8, 7, 0, 0, f1);
    wfm_synth_set_chirp_span (cs, N);
    int step_match = 1;
    for (size_t i = 0; i < N; i++)
      if (wfm_synth_step (cs) != y[i])
        step_match = 0;
    CHECK (step_match);

    /* reset rewinds the sweep to sample 0 (reproducible). */
    float complex y0 = y[0];
    wfm_synth_reset (cu);
    CHECK (wfm_synth_step (cu) == y0);

    /* down-chirp: f_end < f_start sweeps the other way (high → low). */
    float complex *d = malloc (N * sizeof *d);
    CHECK (d != NULL);
    wfm_synth_state_t *cd = wfm_synth_create (WFM_SYNTH_CHIRP, fs, f1, 100.0,
                                              0, 1, 8, 7, 0, 0, f0);
    wfm_synth_set_chirp_span (cd, N);
    wfm_synth_steps (cd, d, N);
    double wd_lo = carg (d[1] * conjf (d[0])) / 6.283185307179586;
    double wd_hi = carg (d[N - 1] * conjf (d[N - 2])) / 6.283185307179586;
    CHECK (ALMOST_EQ (wd_lo, f1 / fs, 2e-3f)); /* starts high */
    CHECK (ALMOST_EQ (wd_hi, f0 / fs, 2e-3f)); /* ends low   */

    free (d);
    free (y);
    wfm_synth_destroy (cu);
    wfm_synth_destroy (cs);
    wfm_synth_destroy (cd);
  }

  wfm_synth_destroy (obj);
  if (_fails)
    {
      fprintf (stderr, "test_wfm_synth_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_wfm_synth_core PASSED\n");
  return 0;
}
