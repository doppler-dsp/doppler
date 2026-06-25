#include "loop_filter/loop_filter_core.h"
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

static int
almost (double a, double b, double tol)
{
  return fabs (a - b) <= tol;
}

int
main (void)
{
  int    _fails = 0;
  double bn = 0.02, zeta = 0.707, t = 1.0;

  loop_filter_state_t *lf = loop_filter_create (bn, zeta, t);
  CHECK (lf != NULL);
  if (!lf)
    return 1;

  /* Gains match the closed form. */
  double wn  = 8.0 * zeta * bn / (4.0 * zeta * zeta + 1.0);
  double th  = wn * t;
  double den = 4.0 + 4.0 * zeta * th + th * th;
  CHECK (almost (lf->kp, 8.0 * zeta * th / den, 1e-12));
  CHECK (almost (lf->ki, 4.0 * th * th / den, 1e-12));
  CHECK (lf->integ == 0.0);

  /* First update on a unit error: integ = ki, control = ki + kp. */
  double ctl = loop_filter_step (lf, 1.0);
  CHECK (almost (lf->integ, lf->ki, 1e-15));
  CHECK (almost (ctl, lf->ki + lf->kp, 1e-15));

  /* Integrator ramps linearly for a constant error. */
  for (int i = 0; i < 9; i++)
    (void)loop_filter_step (lf, 1.0);
  CHECK (almost (lf->integ, 10.0 * lf->ki, 1e-12));

  /* reset zeroes the integrator, keeps the gains. */
  double kp_before = lf->kp;
  loop_filter_reset (lf);
  CHECK (lf->integ == 0.0);
  CHECK (lf->kp == kp_before);

  /* configure recomputes gains but preserves the integrator. */
  loop_filter_step (lf, 2.0);
  double integ_before = lf->integ;
  loop_filter_configure (lf, 0.05, zeta, t);
  CHECK (lf->integ == integ_before);
  CHECK (lf->bn == 0.05);
  CHECK (lf->kp != kp_before);

  /* In-place init (the by-value embedding path used by trackers) matches the
   * heap path for the same parameters. */
  loop_filter_state_t emb;
  loop_filter_init (&emb, bn, zeta, t);
  emb.integ = 0.0;
  CHECK (almost (emb.kp, 8.0 * zeta * th / den, 1e-12));
  CHECK (almost (emb.ki, 4.0 * th * th / den, 1e-12));
  CHECK (almost (loop_filter_step (&emb, 1.0), emb.ki + emb.kp, 1e-15));

  loop_filter_destroy (lf);
  if (_fails)
    {
      fprintf (stderr, "test_loop_filter_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_loop_filter_core PASSED\n");
  return 0;
}
