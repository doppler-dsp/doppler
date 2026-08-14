/**
 * @file loop_filter_noise_bw.c
 * @brief Does `bn` deliver a loop whose noise bandwidth is `bn`?
 *
 * `loop_filter_init()` derives its gains by inverting the standard relation
 * between a second-order loop's natural frequency and its noise bandwidth,
 * and every consumer in the library sizes its settling and its jitter off
 * the answer. Nothing measured it. This does.
 *
 * ## Two independent routes to the same number
 *
 * The loop under test is the canonical one the definition assumes: a unit
 * detector, the filter, and an oscillator that integrates the control.
 *
 *     e[n] = w[n] - phi[n]
 *     c[n] = loop_filter_step (e[n])
 *     phi[n+1] = phi[n] + c[n]
 *
 * **Parseval, on the real code.** Drive that loop with a unit impulse and
 * `phi` *is* the closed-loop impulse response `h`. Then
 *
 *     sum h[n]^2  =  integral of |H(f)|^2 over (-1/2, 1/2]  =  2 * Bn
 *
 * so `Bn = 0.5 * sum h[n]^2`, exactly, with no RNG, no spectral grid and no
 * fitting — and it runs `loop_filter_step()` itself, not a model of it.
 *
 * **Spectral, on the derived transfer function.** With `u = exp(-j2*pi*f)`,
 * the same loop has
 *
 *     H(u) = u*[kp*(1-u) + ki] / [ (1-u)^2 + u*(kp*(1-u) + ki) ]
 *
 * integrated numerically over `f` in [0, 1/2].
 *
 * The two share no code and no arithmetic path. Agreement means the
 * implementation matches the difference equation AND the difference
 * equation has the bandwidth it claims; a disagreement localises which of
 * those two failed. A single route could not tell them apart — this is the
 * "consistency tests hide shared defects" rule applied before the fact.
 *
 * ## Units
 *
 * `bn` is normalized to cycles per SAMPLE and `t` is the update period in
 * samples, so a loop ticking once per update has a bandwidth of `bn * t`
 * cycles per update. That product is what both routes measure, which makes
 * this a test of the `t` scaling as well as of the gains.
 *
 * ## What the sweep found
 *
 * `bn` means what it says, and its error is a LAW rather than a scatter.
 * The delivered bandwidth is always slightly WIDE — never narrow, so a
 * caller sizing jitter off `bn` is conservative — by a fractional excess
 * that collapses onto the single group `bn*t`, with `t` dropping out
 * entirely:
 *
 *     Bn / (bn*t)  -  1  ~=  16*zeta^2 / (4*zeta^2 + 1)^2  *  (bn*t)
 *
 * which is `2*zeta*th / (4*zeta^2 + 1)` in the `th = wn*t` the gains are
 * built from. That coefficient is not fitted: it reproduces the measured
 * one to five decimals at every damping swept (1.000000 vs 1.0001 at
 * zeta 0.5, 0.888978 vs 0.8890 at 0.707, 0.640000 vs 0.6400 at 1.0,
 * 0.221453 vs 0.2215 at 2.0), which is why the gate can assert it.
 *
 * Solved for the budget a caller actually wants, at zeta = 0.707:
 * `bn*t <= 0.0112` holds the bandwidth within 1% and `bn*t <= 0.0552`
 * within 5%. Wider damping is more forgiving (1% at bn*t = 0.0450 for
 * zeta = 2.0), narrower less (0.0100 for zeta = 0.5).
 *
 * Usage:
 *   loop_filter_noise_bw           full sweep (bn x zeta x t), a table
 *   loop_filter_noise_bw --check   fast CI gate, asserting four things:
 *                                  (a) the two routes agree, (b) the
 *                                  deviation collapses onto bn*t, (c)
 *                                  bn*t <= 0.01 delivers within 1%, and
 *                                  (d) the excess obeys the closed form
 *                                  above at every damping
 */
#include "loop_filter/loop_filter_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TWOPI 6.283185307179586

/* Closed-loop noise bandwidth in cycles per update, by Parseval on the
 * impulse response of the REAL loop. Returns the truncation residual
 * through `tail` so a caller can tell a converged answer from a clipped
 * one — a silently truncated sum reads as a narrower loop. */
static double
bn_parseval (double bn, double zeta, double t, double *tail)
{
  loop_filter_state_t lf;
  memset (&lf, 0, sizeof lf);
  loop_filter_init (&lf, bn, zeta, t);

  /* The loop's time constant is ~1/(bn*t) updates; 400 of them puts the
   * tail far below double precision's reach for every cell swept here. */
  double bnt   = bn * t;
  size_t n_max = (size_t)(400.0 / (bnt > 1e-6 ? bnt : 1e-6));
  if (n_max < 1000)
    n_max = 1000;
  if (n_max > 40000000u)
    n_max = 40000000u;

  double phi = 0.0, sum2 = 0.0, half = 0.0;
  for (size_t n = 0; n < n_max; n++)
    {
      if (n == n_max / 2)
        half = sum2;
      sum2 += phi * phi;
      double e = ((n == 0) ? 1.0 : 0.0) - phi;
      phi += loop_filter_step (&lf, e);
    }

  *tail = (sum2 > 0.0) ? (sum2 - half) / sum2 : 0.0;
  return 0.5 * sum2;
}

/* The same quantity from the derived transfer function, integrated on a
 * uniform grid over [0, 1/2] by the trapezoid rule. */
static double
bn_spectral (double bn, double zeta, double t)
{
  loop_filter_state_t lf;
  memset (&lf, 0, sizeof lf);
  loop_filter_init (&lf, bn, zeta, t);

  const size_t M   = 1u << 20;
  const double df  = 0.5 / (double)M;
  double       acc = 0.0;

  for (size_t i = 0; i <= M; i++)
    {
      double         f  = 0.5 * (double)i / (double)M;
      double complex u  = cexp (-I * TWOPI * f);
      double complex fw = lf.kp * (1.0 - u) + lf.ki; /* the PI numerator  */
      double complex H  = u * fw / ((1.0 - u) * (1.0 - u) + u * fw);
      double         m2 = creal (H) * creal (H) + cimag (H) * cimag (H);
      acc += ((i == 0 || i == M) ? 0.5 : 1.0) * m2;
    }
  return acc * df;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;

  static const double bns[]
      = { 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2 };
  static const double zetas[] = { 0.5, 0.707, 1.0, 2.0 };
  static const double ts[]    = { 0.25, 1.0, 4.0 };

  /* The gate asserts the four things the sweep established, not a flat
   * tolerance band. A band wide enough to admit bn*t = 0.05 would be wide
   * enough to hide the law changing underneath it — demonstrated, not
   * assumed: a sabotage of `den`'s damping term moves the excess law by
   * 25-200% at every zeta while (c) trips at exactly one point, by 0.0002. */
  if (check)
    {
      /* (a) The two routes agree. They share no arithmetic, so this is what
         says the implementation matches its own difference equation. */
      static const double cells[][3] = { { 0.0005, 0.707, 1.0 },
                                         { 0.01, 0.707, 1.0 },
                                         { 0.05, 0.5, 1.0 },
                                         { 0.02, 1.0, 0.25 },
                                         { 0.002, 2.0, 4.0 } };
      for (size_t i = 0; i < sizeof cells / sizeof *cells; i++)
        {
          double tail = 0.0;
          double p
              = bn_parseval (cells[i][0], cells[i][1], cells[i][2], &tail);
          double s = bn_spectral (cells[i][0], cells[i][1], cells[i][2]);
          if (!(fabs (p - s) <= 1e-6 * s) || tail > 1e-12)
            {
              printf ("FAIL routes disagree at bn=%.4g zeta=%.3f t=%.2f: "
                      "parseval=%.9g spectral=%.9g tail=%.3g\n",
                      cells[i][0], cells[i][1], cells[i][2], p, s, tail);
              fail = 1;
            }
        }

      /* (b) The deviation is a function of the PRODUCT bn*t alone — `t`
         drops out. This is what pins the update-period scaling: a loop
         updating four times as often, at a quarter the bandwidth per
         update, is the same loop. */
      static const double pairs[][4] = {
        /* bn_a,  t_a,   bn_b,  t_b   — each pair has equal bn*t */
        { 0.02, 0.25, 0.005, 1.0 },
        { 0.2, 0.25, 0.05, 1.0 },
        { 0.05, 4.0, 0.2, 1.0 },
      };
      for (size_t i = 0; i < sizeof pairs / sizeof *pairs; i++)
        {
          double junk = 0.0;
          double ra   = bn_parseval (pairs[i][0], 0.707, pairs[i][1], &junk)
                        / (pairs[i][0] * pairs[i][1]);
          double rb   = bn_parseval (pairs[i][2], 0.707, pairs[i][3], &junk)
                        / (pairs[i][2] * pairs[i][3]);
          if (!(fabs (ra - rb) <= 1e-6 * rb))
            {
              printf ("FAIL bn*t collapse: (%.4g,%.2f)->%.9f vs "
                      "(%.4g,%.2f)->%.9f\n",
                      pairs[i][0], pairs[i][1], ra, pairs[i][2], pairs[i][3],
                      rb);
              fail = 1;
            }
        }

      /* (c) The caller-facing budget: at the damping every consumer uses,
         keeping bn*t <= 0.01 keeps the delivered bandwidth within 1% of the
         one asked for. Every shipped configuration sits inside this. The
         loop is always slightly WIDE, never narrow — a caller sizing noise
         off bn is therefore conservative, never optimistic. */
      static const double budget[] = { 0.0005, 0.001, 0.002, 0.005, 0.01 };
      for (size_t i = 0; i < sizeof budget / sizeof *budget; i++)
        {
          double tail = 0.0;
          double ratio
              = bn_parseval (budget[i], 0.707, 1.0, &tail) / budget[i];
          if (!(ratio >= 1.0 && ratio <= 1.01))
            {
              printf ("FAIL budget bn*t=%.4g ratio=%.6f (want [1, 1.01])\n",
                      budget[i], ratio);
              fail = 1;
            }
        }

      /* (d) The excess obeys the derived law, not merely "some small
         number". Asserting the closed form is what makes this a gate on the
         BEHAVIOUR rather than on a tolerance: a change that widened the
         loop by a different function of damping would sit comfortably
         inside any band loose enough to pass (c), and fails here. */
      static const double law_zetas[] = { 0.5, 0.707, 1.0, 2.0 };
      const double        bnt         = 1e-4; /* deep in the linear regime */
      for (size_t i = 0; i < sizeof law_zetas / sizeof *law_zetas; i++)
        {
          double z    = law_zetas[i];
          double junk = 0.0;
          double got  = (bn_parseval (bnt, z, 1.0, &junk) / bnt - 1.0) / bnt;
          double want
              = 16.0 * z * z / ((4.0 * z * z + 1.0) * (4.0 * z * z + 1.0));
          if (!(fabs (got - want) <= 1e-3 * want))
            {
              printf ("FAIL excess law at zeta=%.3f: measured %.6f, "
                      "16*z^2/(4*z^2+1)^2 = %.6f\n",
                      z, got, want);
              fail = 1;
            }
        }

      printf (fail ? "" : "loop_filter_noise_bw: OK\n");
      return fail;
    }

  printf ("# Closed-loop noise bandwidth vs the declared bn.\n");
  printf ("# parseval: 0.5*sum h[n]^2 on the real loop.  "
          "spectral: integral |H(f)|^2 df.\n");
  printf ("# ratio = measured / (bn*t); 1.000 means bn means what it "
          "says.\n\n");
  printf ("%8s %6s %6s %12s %12s %12s %8s %10s\n", "bn", "zeta", "t", "bn*t",
          "parseval", "spectral", "ratio", "tail");

  for (size_t k = 0; k < sizeof ts / sizeof *ts; k++)
    for (size_t j = 0; j < sizeof zetas / sizeof *zetas; j++)
      {
        for (size_t i = 0; i < sizeof bns / sizeof *bns; i++)
          {
            double bn = bns[i], zeta = zetas[j], t = ts[k];
            double tail = 0.0;
            double p    = bn_parseval (bn, zeta, t, &tail);
            double s    = bn_spectral (bn, zeta, t);
            printf ("%8.4g %6.3f %6.2f %12.6g %12.6g %12.6g %8.4f %10.2g\n",
                    bn, zeta, t, bn * t, p, s, p / (bn * t), tail);
          }
        printf ("\n");
      }
  return 0;
}
