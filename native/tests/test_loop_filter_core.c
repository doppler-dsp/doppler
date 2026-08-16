/**
 * @file test_loop_filter_core.c
 * @brief Unit tests for the second-order PI loop filter.
 *
 * The filter is embedded by value in seven objects (costas, carrier_mpsk,
 * carrier_nda, dll, symsync, ratesync, burst_despreader), so a claim that
 * holds here holds for every tracking loop in the library — and one that is
 * merely assumed here is assumed by all of them.
 *
 * Sections:
 *   1. Lifecycle: create/init parity, the calloc-zeroed integrator,
 *      destroy(NULL)
 *   2. The gains are the canonical form — checked against an INDEPENDENTLY
 *      written parameterisation, not a transcription of the implementation
 *   3. init stores bn, zeta and t
 *   4. The PI recurrence, and the linear ramp under a constant error
 *   5. init does NOT touch integ — the contract all seven embedders depend on
 *   6. reset zeroes the integrator and keeps BOTH gains
 *   7. configure retunes without disturbing the integrator
 *   8. steps() == repeated step(), with the integrator carried across calls
 *   9. steps() with out aliasing x
 *  10. The declared domain's edge, and what lies outside it
 *  11. Serialized state: round-trip, envelope reject, and what a restore
 *      into a differently-configured instance does
 *
 * See docs/design/loop-filter.md for the reasoning these assume.
 */
#include "dp_test.h"
#include "loop_filter/loop_filter_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* The canonical discrete PI gains, written from Rice, "Digital
 * Communications: A Discrete-Time Approach", App. C, with Kd*K0 = 1.
 *
 * This is deliberately NOT loop_filter_init()'s expression. That one derives
 * wn by inverting the analog noise-bandwidth relation and carries
 * th = wn*t; this one carries theta = th/2 and a denominator scaled by 4.
 * The two are algebraically identical, so a correct implementation matches
 * to machine precision — but a sign or factor error in one does not
 * reproduce in the other, which is the whole reason for writing it twice.
 * Re-typing the implementation's own formula beside it would prove only
 * that the file had been copied correctly. */
static void
rice_gains (double bn, double zeta, double t, double *kp, double *ki)
{
  double theta = 4.0 * zeta * bn * t / (4.0 * zeta * zeta + 1.0);
  double den   = 1.0 + 2.0 * zeta * theta + theta * theta;
  *kp          = 4.0 * zeta * theta / den;
  *ki          = 4.0 * theta * theta / den;
}

/**
 * @brief loop_filter_wn() against this file's INDEPENDENT derivation.
 *
 * `rice_gains` above derives `theta = 4*zeta*bn*t / (4*zeta^2 + 1)` from the
 * published form, deliberately without re-typing the implementation's
 * expression — and `theta` is exactly `wn*t/2`. So tying the new public
 * `loop_filter_wn()` to it checks the primitive against a derivation that was
 * already independent, rather than against a copy of itself.
 *
 * The alternative — asserting `loop_filter_wn(bn, zeta) == 8*zeta*bn /
 * (4*zeta^2 + 1)` — would be re-typing the implementation beside itself and
 * would prove only that the file had been copied correctly, which is the trap
 * this test file's own header calls out.
 */
static void
check_wn (double bn, double zeta, double t)
{
  double kp, ki, theta;
  rice_gains (bn, zeta, t, &kp, &ki);
  theta = 4.0 * zeta * bn * t / (4.0 * zeta * zeta + 1.0);
  DP_CHECK_NEAR (loop_filter_wn (bn, zeta) * t / 2.0, theta, 1e-12);
}

int
main (void)
{
  const double bn = 0.02, zeta = 0.707, t = 1.0;

  /* ------------------------------------------------------------------ *
   * 1. Lifecycle: create/init parity, calloc-zeroed integ, destroy(NULL)
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t *lf = loop_filter_create (bn, zeta, t);
    DP_CHECK (lf != NULL);
    if (!lf)
      return 1;

    /* create() is calloc + init(), so the integrator starts at zero on the
       heap path. Section 5 covers the path where it does not. */
    DP_CHECK (lf->integ == 0.0);

    loop_filter_state_t emb;
    memset (&emb, 0, sizeof emb);
    loop_filter_init (&emb, bn, zeta, t);
    DP_CHECK (emb.kp == lf->kp);
    DP_CHECK (emb.ki == lf->ki);
    DP_CHECK (emb.bn == lf->bn);
    DP_CHECK (emb.zeta == lf->zeta);
    DP_CHECK (emb.t == lf->t);
    DP_CHECK (emb.integ == lf->integ);

    loop_filter_destroy (lf);

    /* "@param state May be NULL" — the claim is that this is a no-op, and
       until now nothing ran it. */
    loop_filter_destroy (NULL);

    /* gh-740: create() is the UNTRUSTED boundary — a Python caller's
       arbitrary doubles arrive through it — and rejects every domain
       violation. init() is deliberately left unguarded (§10): it is the
       by-value path whose seven embedders all validate upstream, and the
       asymmetry is the decision, not an oversight. */
    loop_filter_state_t *edge = loop_filter_create (0.0, zeta, t);
    DP_CHECK (edge != NULL); /* bn = 0 is IN the domain — see §10 */
    loop_filter_destroy (edge);

    DP_CHECK (loop_filter_create (-1e-9, zeta, t) == NULL);
    DP_CHECK (loop_filter_create (bn, zeta, 0.0) == NULL);
    DP_CHECK (loop_filter_create (bn, zeta, -1.0) == NULL);
    DP_CHECK (loop_filter_create (bn, 0.0, t) == NULL);
    DP_CHECK (loop_filter_create (bn, -1.0, t) == NULL);
    /* Non-finite in any argument: t = INFINITY used to give NaN gains, and
       a NaN gain poisons every later update permanently. */
    DP_CHECK (loop_filter_create (INFINITY, zeta, t) == NULL);
    DP_CHECK (loop_filter_create (bn, INFINITY, t) == NULL);
    DP_CHECK (loop_filter_create (bn, zeta, INFINITY) == NULL);
    DP_CHECK (loop_filter_create (NAN, zeta, t) == NULL);
    DP_CHECK (loop_filter_create (bn, NAN, t) == NULL);
    DP_CHECK (loop_filter_create (bn, zeta, NAN) == NULL);
  }

  /* ------------------------------------------------------------------ *
   * 2. The gains are the canonical form (independent parameterisation)
   * ------------------------------------------------------------------ */
  {
    static const double bns[]   = { 0.001, 0.005, 0.01, 0.02, 0.05, 0.2 };
    static const double zetas[] = { 0.5, 0.707, 0.9 };
    static const double ts[]    = { 0.125, 0.5, 1.0, 4.0 };

    for (size_t i = 0; i < sizeof bns / sizeof *bns; i++)
      for (size_t j = 0; j < sizeof zetas / sizeof *zetas; j++)
        for (size_t k = 0; k < sizeof ts / sizeof *ts; k++)
          {
            double want_kp, want_ki;
            rice_gains (bns[i], zetas[j], ts[k], &want_kp, &want_ki);

            loop_filter_state_t s;
            memset (&s, 0, sizeof s);
            loop_filter_init (&s, bns[i], zetas[j], ts[k]);

            /* Relative tolerance: the two forms differ by a factor of 4 top
               and bottom, so they are not bit-identical even when exact. */
            DP_CHECK_NEAR (s.kp, want_kp, 1e-12 * fabs (want_kp) + 1e-15);
            DP_CHECK_NEAR (s.ki, want_ki, 1e-12 * fabs (want_ki) + 1e-15);

            /* Both gains are positive across the whole declared domain —
               the sign convention every embedder's steer depends on. */
            DP_CHECK (s.kp > 0.0 && s.ki > 0.0);
          }
  }

  /* ------------------------------------------------------------------ *
   * 3. init stores bn, zeta and t
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t s;
    memset (&s, 0, sizeof s);
    loop_filter_init (&s, 0.031, 0.61, 2.5);
    DP_CHECK (s.bn == 0.031);
    DP_CHECK (s.zeta == 0.61);
    DP_CHECK (s.t == 2.5);
  }

  /* ------------------------------------------------------------------ *
   * 4. The PI recurrence, and the ramp under a constant error
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t *lf = loop_filter_create (bn, zeta, t);

    /* First update on a unit error: integ = ki, control = ki + kp. */
    double ctl = loop_filter_step (lf, 1.0);
    DP_CHECK_NEAR (lf->integ, lf->ki, 1e-15);
    DP_CHECK_NEAR (ctl, lf->ki + lf->kp, 1e-15);

    /* The integrator ramps LINEARLY for a constant error, which is what
       makes it a rate estimate rather than a smoothed error. */
    for (int i = 0; i < 9; i++)
      (void)loop_filter_step (lf, 1.0);
    DP_CHECK_NEAR (lf->integ, 10.0 * lf->ki, 1e-12);

    /* The proportional term is instantaneous: with the integrator held, the
       control tracks the error's sign and scale with no memory at all. */
    lf->integ  = 0.0;
    double neg = loop_filter_step (lf, -1.0);
    DP_CHECK_NEAR (neg, -(lf->ki + lf->kp), 1e-15);

    loop_filter_destroy (lf);
  }

  /* ------------------------------------------------------------------ *
   * 5. init does NOT touch integ — the embedding contract
   *
   * Every one of the seven embedders relies on this, and two of them
   * (costas, carrier_mpsk) rely on it POSITIVELY: they seed the integrator
   * to a known carrier offset and would lose it if init zeroed. It is also
   * what makes loop_filter_init() usable as a retune (section 7).
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t s;
    memset (&s, 0, sizeof s);
    loop_filter_init (&s, bn, zeta, t);

    const double sentinel = -3.25;
    s.integ               = sentinel;
    double kp_before      = s.kp;

    loop_filter_init (&s, 0.05, zeta, t); /* a real retune */

    DP_CHECK (s.integ == sentinel); /* untouched, bit-exact */
    DP_CHECK (s.kp != kp_before);   /* and the gains really did move */
  }

  /* ------------------------------------------------------------------ *
   * 6. reset zeroes the integrator and keeps BOTH gains
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t *lf = loop_filter_create (bn, zeta, t);
    for (int i = 0; i < 10; i++)
      (void)loop_filter_step (lf, 1.0);
    DP_CHECK (lf->integ != 0.0);

    double kp_before = lf->kp, ki_before = lf->ki;
    loop_filter_reset (lf);

    DP_CHECK (lf->integ == 0.0);
    DP_CHECK (lf->kp == kp_before);
    DP_CHECK (lf->ki == ki_before); /* ki was never checked before */
    DP_CHECK (lf->bn == bn && lf->zeta == zeta && lf->t == t);

    loop_filter_destroy (lf);
  }

  /* ------------------------------------------------------------------ *
   * 7. configure retunes without disturbing the integrator
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t *lf = loop_filter_create (bn, zeta, t);
    (void)loop_filter_step (lf, 2.0);

    double integ_before = lf->integ, kp_before = lf->kp;
    loop_filter_configure (lf, 0.05, zeta, t);

    DP_CHECK (lf->integ == integ_before); /* the estimate survives */
    DP_CHECK (lf->bn == 0.05);
    DP_CHECK (lf->kp != kp_before);

    /* configure and init are the same operation; a caller with an embedded
       state uses init and must get the identical result. */
    loop_filter_state_t viaInit;
    memset (&viaInit, 0, sizeof viaInit);
    loop_filter_init (&viaInit, 0.05, zeta, t);
    DP_CHECK (viaInit.kp == lf->kp && viaInit.ki == lf->ki);

    loop_filter_destroy (lf);
  }

  /* ------------------------------------------------------------------ *
   * 8. steps() == repeated step(), integrator carried across calls
   *
   * The block entry point had no C-level caller and no C-level test; its
   * only caller in the tree is the Python binding.
   * ------------------------------------------------------------------ */
  {
    enum
    {
      N = 64
    };
    double x[N], got[N];

    /* A varied, sign-changing error — a constant one would pass even if the
       loop dropped or reordered elements. */
    for (int i = 0; i < N; i++)
      x[i] = sin (0.37 * i) + 0.25 * cos (1.9 * i);

    loop_filter_state_t *blk = loop_filter_create (bn, zeta, t);
    loop_filter_state_t *ref = loop_filter_create (bn, zeta, t);

    /* Split the block in two so the carry ACROSS calls is exercised, not
       just the carry within one. */
    loop_filter_steps (blk, x, got, 20);
    loop_filter_steps (blk, x + 20, got + 20, (size_t)(N - 20));

    for (int i = 0; i < N; i++)
      {
        double want = loop_filter_step (ref, x[i]);
        /* Same inline function, but inlined at two different call sites, so
           an FMA contraction can differ by an ULP on arm64 — the same
           allowance section 11 makes. A real ordering bug is O(1). */
        DP_CHECK_NEAR (got[i], want, 1e-12);
      }

    /* The integrator itself must agree, not merely the outputs. */
    DP_CHECK_NEAR (blk->integ, ref->integ, 1e-12);

    /* n == 0 is a no-op, not a read of x. */
    double before = blk->integ;
    loop_filter_steps (blk, NULL, NULL, 0);
    DP_CHECK (blk->integ == before);

    loop_filter_destroy (blk);
    loop_filter_destroy (ref);
  }

  /* ------------------------------------------------------------------ *
   * 9. steps() with out aliasing x
   *
   * "@param out Control-value array (length >= n; may alias @p x)". A
   * two-pass implementation would satisfy section 8 and fail here, which is
   * why this is its own section rather than a line in that one.
   * ------------------------------------------------------------------ */
  {
    enum
    {
      N = 48
    };
    double x[N], sep[N], both[N];

    for (int i = 0; i < N; i++)
      x[i] = sin (0.11 * i) - 0.4;

    loop_filter_state_t *a = loop_filter_create (bn, zeta, t);
    loop_filter_state_t *b = loop_filter_create (bn, zeta, t);

    loop_filter_steps (a, x, sep, N); /* separate buffers */

    memcpy (both, x, sizeof both);
    loop_filter_steps (b, both, both, N); /* fully aliased */

    for (int i = 0; i < N; i++)
      DP_CHECK_NEAR (both[i], sep[i], 1e-12);

    loop_filter_destroy (a);
    loop_filter_destroy (b);
  }

  /* ------------------------------------------------------------------ *
   * 10. The declared domain's edge, and what lies outside it
   *
   * The header declares bn >= 0 and t > 0. create() enforces them (§1);
   * init() deliberately does not, because it is the by-value path and its
   * seven embedders validate upstream — guarding an internal guarantee is
   * error handling this project does not write. gh-740 is where that split
   * was decided; this section is what pins the unguarded half.
   * ------------------------------------------------------------------ */
  {
    /* bn = 0 is IN the declared domain, and it means a frozen loop: both
       gains are exactly zero, so the control is whatever the integrator
       already held and no error can ever change it. A caller can rely on
       this — it is how a loop is held open. */
    loop_filter_state_t z;
    memset (&z, 0, sizeof z);
    loop_filter_init (&z, 0.0, zeta, t);
    DP_CHECK (z.kp == 0.0 && z.ki == 0.0);

    z.integ = 0.75;
    for (int i = 0; i < 100; i++)
      DP_CHECK (loop_filter_step (&z, 1.0) == 0.75);
    DP_CHECK (z.integ == 0.75);

    /* Outside the domain the object does not complain. These assert only
       that the gains stay FINITE — deliberately weaker than the values,
       so that adding the validation the header implies does not turn this
       section red. What the values ARE is recorded in the certification
       report, and the missing validation is filed as an issue. */
    loop_filter_state_t bad;

    memset (&bad, 0, sizeof bad);
    loop_filter_init (&bad, bn, zeta, 0.0); /* t = 0, declared t > 0 */
    DP_CHECK (isfinite (bad.kp) && isfinite (bad.ki));

    memset (&bad, 0, sizeof bad);
    loop_filter_init (&bad, -0.01, zeta, t); /* bn < 0, declared bn >= 0 */
    DP_CHECK (isfinite (bad.kp) && isfinite (bad.ki));
  }

  /* ------------------------------------------------------------------ *
   * 11. Serialized state
   * ------------------------------------------------------------------ */
  {
    loop_filter_state_t *a = loop_filter_create (0.01, 0.707, 1.0);
    for (int i = 0; i < 30; i++)
      (void)loop_filter_step (a, 0.1);

    unsigned char blob[64];
    DP_CHECK (loop_filter_state_bytes (a) <= sizeof blob);
    loop_filter_get_state (a, blob);

    double refv = 0.0;
    for (int i = 0; i < 10; i++)
      refv = loop_filter_step (a, 0.1); /* reference continuation */

    loop_filter_state_t *b = loop_filter_create (0.01, 0.707, 1.0);
    DP_CHECK (loop_filter_set_state (b, blob) == DP_OK);

    /* The envelope rejects a clobbered blob rather than reinterpreting it,
       and leaves the target untouched when it does. */
    double integ_after_restore = b->integ;
    blob[0] ^= (unsigned char)0xFF;
    DP_CHECK (loop_filter_set_state (b, blob) == DP_ERR_INVALID);
    DP_CHECK (b->integ == integ_after_restore);
    blob[0] ^= (unsigned char)0xFF;

    double gotv = 0.0;
    for (int i = 0; i < 10; i++)
      gotv = loop_filter_step (b, 0.1);
    /* The restored integrator is bit-identical (whole-struct snapshot); the
     * two continuation loops can still differ by an FMA-contraction ULP on
     * arm64 (clang fuses `integ + kp*x` differently per call site), so compare
     * with a tolerance — a real restore bug would be O(1), far above this. */
    DP_CHECK_NEAR (refv, gotv, 1e-12);
    loop_filter_destroy (b);

    /* A restore into a DIFFERENTLY-configured instance: the snapshot is the
       whole struct, so the blob's configuration wins and the target is
       silently retuned to the source's bandwidth. That is the documented
       POD design, and it is a caller-visible consequence worth pinning —
       a restore is not only a memory transfer. */
    loop_filter_state_t *c = loop_filter_create (0.05, 0.5, 4.0);
    DP_CHECK (c->bn == 0.05);
    DP_CHECK (loop_filter_set_state (c, blob) == DP_OK);
    DP_CHECK (c->bn == 0.01);
    DP_CHECK (c->zeta == 0.707);
    DP_CHECK (c->t == 1.0);
    DP_CHECK (c->kp == a->kp && c->ki == a->ki);
    loop_filter_destroy (c);

    loop_filter_destroy (a);
  }

  /* The natural frequency, now public because every closed form about this
     loop is written in it (loop_filter_core.h). */
  check_wn (bn, zeta, t);
  check_wn (0.005, 0.707, 1.0);
  check_wn (0.05, 1.0, 4.0);

  /* The number the header quotes: wn = 1.8857*bn at zeta = 0.707. Callers
     size ramp tolerances against it, so it is pinned as a value and not only
     as a relationship. */
  DP_CHECK_NEAR (loop_filter_wn (0.005, 0.707), 1.8857 * 0.005, 1e-6);

  /* Linear in bn, which is what makes a symbol-rate-normalised bn give a
     per-symbol wn — the property every ramp law here depends on. */
  DP_CHECK_NEAR (loop_filter_wn (0.02, 0.707),
                 4.0 * loop_filter_wn (0.005, 0.707), 1e-12);

  /* And it is the SAME number loop_filter_init() used: theta = wn*t/2 drives
     the gains, so a wn that drifted from the gains would break this. */
  {
    loop_filter_state_t s;
    double              kp, ki;
    loop_filter_init (&s, bn, zeta, t);
    rice_gains (bn, zeta, t, &kp, &ki);
    DP_CHECK_NEAR (s.kp, kp, 1e-12);
    DP_CHECK_NEAR (s.ki, ki, 1e-12);
  }

  DP_TEST_END ("test_loop_filter_core");
}
