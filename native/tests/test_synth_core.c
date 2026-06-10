#include "synth/synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
  int            _fails = 0;
  synth_state_t *obj
      = synth_create (0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0, 0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)synth_step (obj);

  /* reset */
  synth_reset (obj);

  /* ── clean (snr >= SYNTH_SNR_CLEAN) generates no AWGN; baseband no LO ── */
  {
    /* clean tone with a freq offset: LO present, no AWGN */
    synth_state_t *c
        = synth_create (SYNTH_TONE, 1e6, 1e5, 100.0, 0, 1, 8, 7, 0, 0);
    CHECK (c && c->awgn == NULL && c->lo != NULL);
    if (c)
      synth_destroy (c);

    /* noisy tone: AWGN present */
    synth_state_t *nz
        = synth_create (SYNTH_TONE, 1e6, 1e5, 10.0, 0, 1, 8, 7, 0, 0);
    CHECK (nz && nz->awgn != NULL);
    if (nz)
      synth_destroy (nz);

    /* baseband (freq 0): no LO */
    synth_state_t *bb
        = synth_create (SYNTH_TONE, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0, 0);
    CHECK (bb && bb->lo == NULL && bb->awgn == NULL);
    if (bb)
      synth_destroy (bb);

    /* noise type always has AWGN, even at high snr */
    synth_state_t *ns
        = synth_create (SYNTH_NOISE, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0, 0);
    CHECK (ns && ns->awgn != NULL);
    if (ns)
      synth_destroy (ns);
  }

  synth_destroy (obj);

  /* ── step() loop is byte-identical to steps() ─────────────────────────
   * synth_step() must equal a synth_steps() block bit-for-bit; it delegates
   * to synth_steps() precisely so the two cannot drift. A hand-rolled scalar
   * `sym*carrier + noise` contracts to FMAs differently from the block path
   * under -ffast-math, and QPSK's irrational ±1/√2 leg made them diverge by
   * an ULP on arm64 (#67) — invisible on x86 (no baseline FMA), so this gate
   * matters most on the macOS C job. Cover every waveform, with the LO on
   * (freq offset) and both clean (snr=100, AWGN off) and noisy (snr=10) — the
   * #67 repro was the clean qpsk case. */
  {
    enum
    {
      N = 1024
    };
    static float complex a[N], b[N];
    const int            types[]
        = { SYNTH_TONE, SYNTH_NOISE, SYNTH_PN, SYNTH_BPSK, SYNTH_QPSK };
    const double snrs[] = { 100.0, 10.0 }; /* clean, noisy */
    const int    spss[] = { 1, 4 };
    for (size_t t = 0; t < sizeof (types) / sizeof (types[0]); t++)
      for (size_t q = 0; q < sizeof (snrs) / sizeof (snrs[0]); q++)
        for (size_t p = 0; p < sizeof (spss) / sizeof (spss[0]); p++)
          {
            int    ty = types[t];
            double sn = snrs[q];
            int    sp = spss[p];
            /* identical config → identical PN/LO/AWGN evolution */
            synth_state_t *sa
                = synth_create (ty, 1e6, 1e5, sn, 0, 7, sp, 7, 0, 0);
            synth_state_t *sb
                = synth_create (ty, 1e6, 1e5, sn, 0, 7, sp, 7, 0, 0);
            CHECK (sa && sb);
            if (sa && sb)
              {
                for (int i = 0; i < N; i++)
                  a[i] = synth_step (sa);
                synth_steps (sb, b, N);
                CHECK (memcmp (a, b, sizeof (a)) == 0);
              }
            if (sa)
              synth_destroy (sa);
            if (sb)
              synth_destroy (sb);
          }
  }

  if (_fails)
    {
      fprintf (stderr, "test_synth_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_synth_core PASSED\n");
  return 0;
}
