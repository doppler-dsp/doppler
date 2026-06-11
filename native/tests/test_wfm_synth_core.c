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
      = wfm_synth_create (0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0, 0);
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
    wfm_synth_state_t *c
        = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 1e5, 100.0, 0, 1, 8, 7, 0, 0);
    CHECK (c && c->awgn == NULL && c->lo != NULL);
    if (c)
      wfm_synth_destroy (c);

    /* noisy tone: AWGN present */
    wfm_synth_state_t *nz
        = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 1e5, 10.0, 0, 1, 8, 7, 0, 0);
    CHECK (nz && nz->awgn != NULL);
    if (nz)
      wfm_synth_destroy (nz);

    /* baseband (freq 0): no LO */
    wfm_synth_state_t *bb
        = wfm_synth_create (WFM_SYNTH_TONE, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0, 0);
    CHECK (bb && bb->lo == NULL && bb->awgn == NULL);
    if (bb)
      wfm_synth_destroy (bb);

    /* noise type always has AWGN, even at high snr */
    wfm_synth_state_t *ns = wfm_synth_create (WFM_SYNTH_NOISE, 1e6, 0.0, 100.0,
                                              0, 1, 8, 7, 0, 0);
    CHECK (ns && ns->awgn != NULL);
    if (ns)
      wfm_synth_destroy (ns);
  }

  /* ── RRC pulse shaping: step()==steps(), shaping changes the output ────────
   */
  {
    /* a small symmetric low-pass FIR stands in for the RRC taps here */
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    wfm_synth_state_t *rs
        = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0, 0, 7, 4, 7, 0, 0);
    CHECK (rs && rs->fir == NULL);
    CHECK (wfm_synth_set_rrc (rs, taps, 5) == 0);
    CHECK (rs->fir != NULL);
    float complex y[256];
    wfm_synth_steps (rs, y, 256);

    /* step() must reproduce steps() bit-for-bit */
    wfm_synth_state_t *rs2
        = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0, 0, 7, 4, 7, 0, 0);
    wfm_synth_set_rrc (rs2, taps, 5);
    int match = 1;
    for (int i = 0; i < 256; i++)
      if (wfm_synth_step (rs2) != y[i])
        match = 0;
    CHECK (match);

    /* shaping changes the output vs the unshaped (rect) synth */
    wfm_synth_state_t *rect
        = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0, 0, 7, 4, 7, 0, 0);
    float complex r[256];
    wfm_synth_steps (rect, r, 256);
    int differs = 0;
    for (int i = 0; i < 256; i++)
      if (r[i] != y[i])
        differs = 1;
    CHECK (differs);

    /* set_rrc is a no-op on a non-modulated synth, and rejects bad args */
    CHECK (wfm_synth_set_rrc (obj, taps, 5) == 0); /* obj is a tone */
    CHECK (wfm_synth_set_rrc (rs, NULL, 0) == -1);

    wfm_synth_destroy (rs);
    wfm_synth_destroy (rs2);
    wfm_synth_destroy (rect);
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
