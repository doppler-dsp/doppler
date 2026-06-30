#include "dp_state_test.h"
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

  /* ── RRC pulse shaping: step()==steps(), shaping changes the output ────────
   */
  {
    /* a small symmetric low-pass FIR stands in for the RRC taps here */
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    wfm_synth_state_t *rs = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0,
                                              0, 7, 4, 7, 0, 0, 0.0);
    CHECK (rs && rs->fir == NULL);
    CHECK (wfm_synth_set_rrc (rs, taps, 5) == 0);
    CHECK (rs->fir != NULL);
    float complex y[256];
    wfm_synth_steps (rs, y, 256);

    /* step() must reproduce steps() bit-for-bit */
    wfm_synth_state_t *rs2 = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0, 100.0,
                                               0, 7, 4, 7, 0, 0, 0.0);
    wfm_synth_set_rrc (rs2, taps, 5);
    int match = 1;
    for (int i = 0; i < 256; i++)
      if (wfm_synth_step (rs2) != y[i])
        match = 0;
    CHECK (match);

    /* shaping changes the output vs the unshaped (rect) synth */
    wfm_synth_state_t *rect = wfm_synth_create (WFM_SYNTH_QPSK, 1e6, 0.0,
                                                100.0, 0, 7, 4, 7, 0, 0, 0.0);
    float complex      r[256];
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

  /* ── bits: user pattern, mapping, cycling, step()==steps() ────────────────
   */
  {
    const uint8_t pat[6] = { 1, 0, 1, 1, 0, 0 };
    /* bpsk, sps=2 → 12 samples for one pass; build via steps() */
    wfm_synth_state_t *bs = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                              0, 1, 2, 7, 0, 0, 0.0);
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
    wfm_synth_state_t *bs2 = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                               0, 1, 2, 7, 0, 0, 0.0);
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

  /* ── bits + RRC: set_rrc shapes the bit stream, step()==steps() ───────────
   * Regression for the silent no-op where bits accepted RRC but emitted
   * rectangular pulses (set_rrc gated out bits; the bits paths ignored fir).
   */
  {
    const float        taps[5] = { 0.1f, 0.2f, 0.4f, 0.2f, 0.1f };
    const uint8_t      pat[6]  = { 1, 0, 1, 1, 0, 0 };
    wfm_synth_state_t *bs = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                              0, 1, 4, 7, 0, 0, 0.0);
    CHECK (wfm_synth_set_bits (bs, pat, 6, 1) == 0); /* bpsk */
    CHECK (wfm_synth_set_rrc (bs, taps, 5) == 0);    /* now accepted on bits */
    CHECK (bs->fir != NULL);
    float complex y[256];
    wfm_synth_steps (bs, y, 256);

    /* step() must reproduce steps() bit-for-bit (chunk-invariant FIR) */
    wfm_synth_state_t *bs2 = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0,
                                               0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_bits (bs2, pat, 6, 1);
    wfm_synth_set_rrc (bs2, taps, 5);
    int match = 1;
    for (int i = 0; i < 256; i++)
      if (wfm_synth_step (bs2) != y[i])
        match = 0;
    CHECK (match);

    /* shaping changes the output vs the unshaped (rect) bits synth */
    wfm_synth_state_t *rect = wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0,
                                                100.0, 0, 1, 4, 7, 0, 0, 0.0);
    wfm_synth_set_bits (rect, pat, 6, 1);
    float complex r[256];
    wfm_synth_steps (rect, r, 256);
    int differs = 0;
    for (int i = 0; i < 256; i++)
      if (r[i] != y[i])
        differs = 1;
    CHECK (differs);

    wfm_synth_destroy (bs);
    wfm_synth_destroy (bs2);
    wfm_synth_destroy (rect);
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
  /* serializable state — running scalars + present children
   * (presence-flagged). */
  {
    float complex      out[256];
    wfm_synth_state_t *a
        = wfm_synth_create (0, 1e6, 1e5, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
    wfm_synth_state_t *b
        = wfm_synth_create (0, 1e6, 1e5, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
    CHECK (a != NULL && b != NULL);
    wfm_synth_steps (a, out, 256);
    DP_STATE_ROUNDTRIP_TEST (wfm_synth, a, b);
    CHECK (b->sym_pos == a->sym_pos && b->chirp_n == a->chirp_n);
    CHECK (b->cur_re == a->cur_re && b->bit_idx == a->bit_idx);
    wfm_synth_destroy (a);
    wfm_synth_destroy (b);
  }

  printf ("test_wfm_synth_core PASSED\n");
  return 0;
}
