/* Loop-bandwidth audit (doppler #1677): realised Bn and zeta of every loop
 * that designs its gains from bn, measured from the closed-loop step
 * response against ground truth, noise-free.
 *
 * Method.  A loop whose estimate starts (or is stepped) delta away from the
 * truth, with its integrator at the steady value, traces the error
 * e_k = delta*(1 - s_k), s the closed-loop step response.  Then
 *   Bn   = 1/2 * sum_k h_k^2,   h_k = s_k - s_{k-1}   (cycles per update)
 *   wn^2 = -delta / sum_k k*e_k                        (rad per update)
 * (the discrete counterparts of Bn = 1/2 int h^2 and int t e dt = -delta/wn^2
 * for a type-2 loop), and zeta solves Bn = wn (zeta + 1/(4 zeta)) / 2 on its
 * upper branch.  Kd is the open-loop discriminator slope at lock, measured
 * with a frozen loop (bn = 0) from a symmetric +-eps difference. */
#include "doppler/burst_despreader/burst_despreader_core.h"
#include "doppler/carrier_mpsk/carrier_mpsk_core.h"
#include "doppler/carrier_nda/carrier_nda_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "dp_rng_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NMAX 400000

static void
analyse (const char *name, double kd, double bn_cfg, double zeta_cfg,
         double *err, size_t n, double delta, double upd_per_bn_unit)
{
  /* upd_per_bn_unit: updates per unit of bn's time base (1 when bn is per
     update). Bn is reported in bn's own units. */
  /* Subtract the settled residual (NCO/LUT quantisation leaves a bias of
     order 1e-5 that a k-weighted sum would otherwise integrate), and sum
     over the first 3/10 of the run, which is ~12 loop constants. */
  double ess = 0.0;
  for (size_t k = n / 2; k < n; k++)
    ess += err[k];
  ess /= (double)(n - n / 2);
  delta -= ess;
  size_t nn  = n * 3 / 10;
  double sh2 = 0.0, skE = 0.0, prev = 0.0;
  for (size_t k = 0; k < nn; k++)
    {
      err[k] -= ess;
      double s = 1.0 - err[k] / delta;
      double h = s - prev;
      prev     = s;
      sh2 += h * h;
      skE += (double)k * err[k];
    }
  double bn_u = 0.5 * sh2;           /* cycles per update */
  double wn_u = sqrt (-delta / skE); /* rad per update    */
  double q    = 2.0 * bn_u / wn_u;
  /* two zetas give one Bn; an underdamped loop (zeta < 0.5) rings, so the
     number of sign changes in the error picks the branch */
  int zc = 0, last_sg = -1;
  for (size_t k = 1; k < nn; k++)
    if (fabs (err[k]) > 0.02 * fabs (delta))
      {
        int sg = err[k] > 0;
        if (last_sg >= 0 && sg != last_sg)
          zc++;
        last_sg = sg;
      }
  double z
      = q >= 1.0 ? 0.5 * (q + (zc > 3 ? -1 : 1) * sqrt (q * q - 1.0)) : NAN;
  double bn = bn_u * upd_per_bn_unit;
  printf ("%-22s Kd=%7.4f  bn_cfg=%.5f  Bn=%.6f (x%.3f)  zeta_cfg=%.3f  "
          "zeta=%.3f\n",
          name, kd, bn_cfg, bn, bn / bn_cfg, zeta_cfg, z);
}

static double err[NMAX];

static double
ph (uint32_t p)
{
  return (double)(int32_t)p * (6.283185307179586 / 4294967296.0);
}
static float complex xin[NMAX], yout[NMAX];

/* ---------------- carrier_nda ---------------- */
static double
nda_kd (int m)
{
  const double eps = 1e-3;
  double       e[2];
  for (int i = 0; i < 2; i++)
    {
      dp_carrier_nda_state_t *c
          = dp_carrier_nda_create (0.0, 0.707, 0.0, 8, 4, m);
      float complex v = cexpf (I * (float)(i ? eps : -eps));
      for (int k = 0; k < 64; k++)
        xin[k] = v;
      dp_carrier_nda_steps (c, xin, 64, yout, 64);
      e[i] = c->last_error;
      dp_carrier_nda_destroy (c);
    }
  return (e[1] - e[0]) / (2 * eps);
}

static void
audit_nda (int m, double bn, double zeta)
{
  const double            delta = 0.02;
  size_t                  pre = 64, n = (size_t)(40.0 / bn);
  dp_carrier_nda_state_t *c = dp_carrier_nda_create (bn, zeta, 0.0, 8, 4, m);
  for (size_t k = 0; k < pre + n; k++)
    {
      float complex v = k < pre ? 1.0f : cexpf (I * (float)delta);
      dp_carrier_nda_steps (c, &v, 1, yout, 1);
      if (k >= pre)
        err[k - pre] = delta - ph (c->nco.phase);
    }
  char name[64];
  snprintf (name, sizeof name, "carrier_nda M=%d", m);
  analyse (name, nda_kd (m), bn, zeta, err, n, delta, 1.0);
  dp_carrier_nda_destroy (c);
}

/* ---------------- costas / carrier_mpsk ---------------- */
static void
audit_costas (double bn, double zeta)
{
  const double eps = 1e-3, delta = 0.02;
  double       e[2];
  for (int i = 0; i < 2; i++)
    {
      dp_costas_state_t *c = dp_costas_create (0.0, zeta, 0.0, 1, 0.0);
      float complex      v = cexpf (I * (float)(i ? eps : -eps));
      for (int k = 0; k < 8; k++)
        xin[k] = v;
      dp_costas_steps (c, xin, 8, yout, 8);
      e[i] = c->last_error;
      dp_costas_destroy (c);
    }
  double             kd  = (e[1] - e[0]) / (2 * eps);
  size_t             pre = 8, n = (size_t)(40.0 / bn);
  dp_costas_state_t *c = dp_costas_create (bn, zeta, 0.0, 1, 0.0);
  for (size_t k = 0; k < pre + n; k++)
    {
      float complex v = k < pre ? 1.0f : cexpf (I * (float)delta);
      dp_costas_steps (c, &v, 1, yout, 1);
      if (k >= pre)
        err[k - pre] = delta - ph (c->nco.phase);
    }
  analyse ("costas", kd, bn, zeta, err, n, delta, 1.0);
  dp_costas_destroy (c);
}

static void
audit_cmpsk (int m, double bn, double zeta)
{
  const double eps = 1e-3, delta = 0.02;
  double       e[2];
  for (int i = 0; i < 2; i++)
    {
      dp_carrier_mpsk_state_t *c
          = dp_carrier_mpsk_create (0.0, zeta, 0.0, 1, 0.0, m);
      float complex v
          = mpsk_constellation (0, m) * cexpf (I * (float)(i ? eps : -eps));
      for (int k = 0; k < 8; k++)
        xin[k] = v;
      dp_carrier_mpsk_steps (c, xin, 8, yout, 8);
      e[i] = c->last_error;
      dp_carrier_mpsk_destroy (c);
    }
  double                   kd  = (e[1] - e[0]) / (2 * eps);
  size_t                   pre = 8, n = (size_t)(40.0 / bn);
  dp_carrier_mpsk_state_t *c
      = dp_carrier_mpsk_create (bn, zeta, 0.0, 1, 0.0, m);
  float complex a0 = mpsk_constellation (0, m);
  for (size_t k = 0; k < pre + n; k++)
    {
      float complex v = a0 * (k < pre ? 1.0f : cexpf (I * (float)delta));
      dp_carrier_mpsk_steps (c, &v, 1, yout, 1);
      if (k >= pre)
        err[k - pre] = delta - ph (c->nco.phase);
    }
  char name[64];
  snprintf (name, sizeof name, "carrier_mpsk M=%d", m);
  analyse (name, kd, bn, zeta, err, n, delta, 1.0);
  dp_carrier_mpsk_destroy (c);
}

/* ---------------- burst_despreader ---------------- */
#define BSF 31
static void
audit_burst (int which, double bn)
{
  uint8_t  bc[BSF];
  uint32_t st = 0x777u;
  for (int i = 0; i < BSF; i++)
    bc[i] = (dp_xs32 (&st) & 1u) ? 1u : 0u;
  const int    sps = 4;
  const double delta
      = which ? 0.02 : (getenv ("BD") ? atof (getenv ("BD")) : 0.3);
  size_t                       nsym = (size_t)(40.0 / bn);
  dp_burst_despreader_state_t *b    = dp_burst_despreader_create (
      bc, BSF, BSF, sps, 0.0, which ? 0.0 : delta, which ? bn : 1e-12,
      which ? 1e-12 : bn);
  size_t        got = 0, n = 0;
  float complex o;
  double        e0 = NAN;
  while (got < nsym)
    {
      size_t        chip = (n / sps) % BSF;
      float complex v    = (bc[chip] & 1u) ? -1.0f : 1.0f;
      if (which)
        v *= cexpf (I * (float)delta);
      size_t k = dp_burst_despreader_steps (b, &v, 1, &o, 1);
      n++;
      if (!k)
        continue;
      double ev;
      if (which)
        ev = delta - b->car_phase;
      else
        {
          double tru = fmod ((double)n / sps, (double)BSF);
          ev         = b->chip_pos - tru;
          if (ev > BSF / 2.0)
            ev -= BSF;
          if (ev < -BSF / 2.0)
            ev += BSF;
        }
      if (isnan (e0))
        e0 = which ? delta : ev;
      err[got++] = ev;
      if (getenv ("DBG") && got < 12)
        printf ("  burst %zu n=%zu ev=%.6f chip_pos=%.6f\n", got, n, ev,
                b->chip_pos);
    }
  analyse (which ? "burst_desp carrier" : "burst_desp code", NAN, bn, 0.707,
           err, nsym, which ? delta : e0, 1.0);
  dp_burst_despreader_destroy (b);
}

/* ---------------- dll ---------------- */
#define SF 127
static uint8_t       code[SF];
static float complex chipsig[SF * 64];

static void
make_code (int sps)
{
  uint32_t st = 0x12345u;
  for (size_t i = 0; i < SF; i++)
    code[i] = (dp_xs32 (&st) & 1u) ? 1u : 0u;
  for (size_t c = 0; c < SF; c++)
    for (int i = 0; i < sps; i++)
      chipsig[c * (size_t)sps + (size_t)i] = (code[c] & 1u) ? -1.0f : 1.0f;
}

static void
audit_dll (int sps, double spacing, double bn, double zeta)
{
  make_code (sps);
  const double tau = 0.02;
  double       e[2];
  for (int s = 0; s < 2; s++)
    {
      dp_dll_state_t d;
      dp_dll_init (&d, code, SF, (size_t)sps, s ? +tau : -tau, 1e-12, zeta,
                   spacing);
      for (int p = 0; p < 4; p++)
        {
          for (size_t i = 0; i < (size_t)SF * sps; i++)
            dll_accumulate (&d, chipsig[i]);
          dll_update (&d);
          d.acc_e = d.acc_p = d.acc_l = 0.0f;
        }
      e[s] = dp_dll_get_last_error (&d);
    }
  double kd = (e[0] - e[1]) / (2.0 * tau);

  /* closed loop from an initial code-phase offset delta (chips) */
  const double   delta = 0.05;
  dp_dll_state_t d;
  dp_dll_init (&d, code, SF, (size_t)sps, delta, bn, zeta, spacing);
  size_t    n = (size_t)(40.0 / bn), dumps = 0;
  long long samp = 0;
  double    ph0  = NAN;
  while (dumps < n)
    for (size_t i = 0; i < (size_t)SF * sps && dumps < n; i++, samp++)
      {
        if (!dll_accumulate (&d, chipsig[i]))
          continue;
        /* error BEFORE the update: the phase this epoch was dumped at */
        double loop_phase = d.chip_pos + (double)(dumps + 1) * SF;
        double true_phase = (double)(samp + 1) / sps;
        double ev         = loop_phase - true_phase;
        if (isnan (ph0))
          ph0 = ev;
        err[dumps++] = ev;
        dll_update (&d);
        d.acc_e = d.acc_p = d.acc_l = 0.0f;
      }
  /* the measured offset sets delta: the steady value is 0 */
  char name[64];
  snprintf (name, sizeof name, "dll sps=%d d=%.2f", sps, spacing);
  if (getenv ("DBG"))
    for (size_t k = 0; k < 40; k++)
      printf ("  %zu %.6e\n", k, err[k]);
  analyse (name, kd, bn, zeta, err, n, ph0, 1.0);
}

int
main (void)
{
  const double z = 0.707;
  for (int i = 0; i < 3; i++)
    {
      double bn = (double[]){ 0.0025, 0.005, 0.01 }[i];
      printf ("--- bn = %g per update ---\n", bn);
      audit_nda (2, bn, z);
      audit_nda (4, bn, z);
      audit_nda (8, bn, z);
      audit_costas (bn, z);
      audit_cmpsk (4, bn, z);
      audit_cmpsk (8, bn, z);
      audit_burst (1, bn);
      audit_burst (0, bn);
      audit_dll (8, 0.5, bn, z);
      audit_dll (2, 0.5, bn, z);
      audit_dll (8, 0.25, bn, z);
    }
  return 0;
}
