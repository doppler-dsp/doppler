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

  /* ── bits: user pattern, mapping, cycling, step()==steps() ────────────────
   */
  {
    const uint8_t pat[6] = { 1, 0, 1, 1, 0, 0 };
    /* bpsk, sps=2 → 12 samples for one pass; build via steps() */
    wfm_synth_state_t *bs
        = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0, 0, 1, 2, 7, 0, 0);
    CHECK (bs && bs->lo == NULL && bs->awgn == NULL && bs->pn == NULL);
    CHECK (wfm_synth_set_bits (bs, pat, 6, 1) == 0); /* 1 = bpsk */
    float complex y[24];
    wfm_synth_steps (bs, y, 24); /* two passes (cycled) */
    /* bpsk: bit 1 -> -1, bit 0 -> +1; symbol centre at each sps-block */
    CHECK (ALMOST_EQ (crealf (y[0]), -1.0f, 1e-5f));  /* bit 1 */
    CHECK (ALMOST_EQ (crealf (y[2]), 1.0f, 1e-5f));   /* bit 0 */
    CHECK (ALMOST_EQ (crealf (y[12]), -1.0f, 1e-5f)); /* cycled: bit 1 again */
    int cyc = 1;
    for (int i = 0; i < 12; i++)
      if (y[i] != y[i + 12])
        cyc = 0;
    CHECK (cyc); /* the pattern repeats every 12 samples */

    /* step() must match steps() bit-for-bit */
    wfm_synth_state_t *bs2
        = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0, 0, 1, 2, 7, 0, 0);
    wfm_synth_set_bits (bs2, pat, 6, 1);
    int match = 1;
    for (int i = 0; i < 24; i++)
      if (wfm_synth_step (bs2) != y[i])
        match = 0;
    CHECK (match);

    /* reset rewinds the pattern */
    wfm_synth_reset (bs);
    CHECK (wfm_synth_step (bs) == y[0]);

    /* set_bits is a no-op on a non-bits synth, and rejects bad args */
    CHECK (wfm_synth_set_bits (obj, pat, 6, 1) == 0);  /* obj is a tone */
    CHECK (wfm_synth_set_bits (bs, pat, 6, 9) == -1);  /* bad modulation */
    CHECK (wfm_synth_set_bits (bs, NULL, 0, 1) == -1); /* empty */

    wfm_synth_destroy (bs);
    wfm_synth_destroy (bs2);
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
