/**
 * @file test_dp_air.c
 * @brief dp_air_test.h's self-test: the scene's rate, level and truth.
 *
 * Every 415.1-B-1 measurement stands on this scene, so each of its claims
 * is checked against the shipped objects' own output:
 *
 *   1. the noise is the reference C/N0's, both rails counted;
 *   2. a clean emitter's power is the chip spectrum's fraction inside the
 *      front end's band (the sinc^2 integral, an equation, to within 0.15
 *      dB: the resampler's transition band is not a brickwall), and two
 *      emitters 10 dB apart in C/N0 are 10 dB apart in power;
 *   3. the truth's code and carrier phase despread the received stream:
 *      one code period against the truth's own chips and carrier gives a
 *      real, near-full correlation, and the same one chip off gives
 *      almost none -- at the 415.1 rate (2.69 samples a chip) on a ramp;
 *   4. the drawn frame start is honored: every symbol the truth places in
 *      the code-only window carries one sign, and the data symbols both.
 *
 * Sabotage (each red): the converter's delay left out of the truth, the
 * frame-start chips left out, the carrier's sign flipped, the emitter's
 * level not scaled to its C/N0.
 */
#include "dp_air_test.h"
#include "dp_dsss_test.h" /* dp_dsss_gold_1023 */
#include "dp_test.h"
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FS 11.0e6
#define SYM 3000.0

static double
mean_power (const float _Complex *x, size_t n)
{
  double s = 0.0;
  for (size_t i = 0; i < n; i++)
    s += (double)crealf (x[i]) * crealf (x[i])
         + (double)cimagf (x[i]) * cimagf (x[i]);
  return s / (double)n;
}

/* The fraction of a rectangular chip's power inside +-b, b in chip rates:
   the integral of sinc^2 from -b to b, by the midpoint rule. */
static double
in_band (double b)
{
  const int n = 200000;
  double    s = 0.0;
  for (int i = 0; i < n; i++)
    {
      const double f = -b + (2.0 * b) * ((double)i + 0.5) / (double)n;
      const double x = M_PI * f;
      s += f == 0.0 ? 1.0 : (sin (x) / x) * (sin (x) / x);
    }
  return s * (2.0 * b) / (double)n;
}

static dp_air_emitter_t
emitter (const uint8_t *code, double fc, double f0, double rate, double cn0,
         size_t frame_off, uint32_t seed)
{
  dp_air_emitter_t e = { 0 };
  e.code             = code;
  e.code_len         = 1023;
  e.chip_rate        = fc * 31.0 / 21216.0;
  e.sym_rate         = SYM;
  e.carrier_hz       = fc;
  e.f0_hz            = f0;
  e.rate_hz_s        = rate;
  e.cn0_dbhz         = cn0;
  e.code_only_sym    = 275;
  e.frame_sym        = 1000;
  e.frame_off        = frame_off;
  e.lag_s            = 0.0;
  e.seed             = seed;
  return e;
}

int
main (void)
{
  static uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const size_t    n = 1u << 20;
  float _Complex *x = malloc (n * sizeof *x);
  DP_REQUIRE (x != NULL);

  /* 1. Noise at the reference: 70.41 dB-Hz over 11 MSa/s is 0 dB a
     sample, so its power is 1.0 over both rails. */
  {
    dp_air_t a;
    DP_REQUIRE (dp_air_init (&a, NULL, 0, FS, 10.0 * log10 (FS), 5u, n));
    DP_REQUIRE (dp_air_next (&a, x, n));
    const double p = mean_power (x, n);
    printf ("  noise power at its reference: %.4f (1.0)\n", p);
    DP_CHECK (fabs (p - 1.0) < 0.01);
    dp_air_free (&a);
  }

  /* 2. A clean emitter at each 415.1 carrier: its power is what of the
     chip spectrum the front end's band keeps, +-fs/2 at 4 samples a chip
     decimated to fs. Two 10 dB apart are 10 dB apart. */
  for (int c = 0; c < 2; c++)
    {
      const double     fc = c ? 2.8e9 : 2.2e9;
      dp_air_emitter_t e  = emitter (code, fc, 0.0, 0.0, 50.0, 0, 3u);
      dp_air_t         a;
      DP_REQUIRE (dp_air_init (&a, &e, 1, FS, NAN, 0u, n));
      DP_REQUIRE (dp_air_next (&a, x, n));
      const double p    = mean_power (x + 4096, n - 4096);
      const double want = in_band (0.5 * FS / e.chip_rate);
      printf ("  emitter power at %.1f GHz: %.4f, in-band sinc^2 %.4f\n",
              fc / 1e9, p, want);
      /* The equation is a brickwall at fs/2; the resampler's transition
         band passes a little past it, so the power sits at or above it, and
         within 0.15 dB (measured 0.11 at 2.2 GHz, 0.04 at 2.8). */
      DP_CHECK (p >= want - 0.002);
      DP_CHECK (10.0 * log10 (p / want) < 0.15);
      dp_air_free (&a);

      dp_air_emitter_t two[2] = { e, e };
      two[1].cn0_dbhz         = 40.0;
      two[1].seed             = 9u;
      DP_REQUIRE (dp_air_init (&a, two, 2, FS, 50.0, 0u, n));
      DP_CHECK (
          fabs (20.0 * log10 ((double)a.amp[1] / (double)a.amp[0]) + 10.0)
          < 1e-6);
      DP_CHECK (fabs ((double)a.amp[0] - 1.0) < 1e-6);
      dp_air_free (&a);
    }

  /* 3 and 4. One clean emitter at 2.8 GHz (2.69 samples a chip), 700 Hz
     off and ramping at 400 Hz/s, its frame start drawn at symbol 618.
     Despread whole code periods against the truth, rendered at the
     default 4 samples a chip and at 2 (the interpolating front end). */
  for (unsigned spc = 4; spc >= 2; spc -= 2)
    {
      dp_air_emitter_t e = emitter (code, 2.8e9, 700.0, 400.0, 50.0, 618, 7u);
      e.spc              = spc;
      dp_air_t a;
      DP_REQUIRE (dp_air_init (&a, &e, 1, FS, NAN, 0u, n));
      DP_REQUIRE (dp_air_next (&a, x, n));
      const double spp     = FS / e.chip_rate * 1023.0; /* samples a period */
      size_t       periods = 0, real_ok = 0, full = 0, off_low = 0;
      long sym_sign[1000] = { 0 }; /* per frame position: +1/-1 seen, 0 none */
      size_t window_mixed = 0, data_pos = 0, data_neg = 0, balanced = 0;
      for (size_t p0 = 2000; p0 + (size_t)spp + 4 < n; p0 += (size_t)spp)
        {
          double _Complex on = 0, off = 0, early = 0, late = 0;
          double energy  = 0.0;
          long   sym0    = -2;
          int    one_sym = 1;
          for (size_t j = p0; j < p0 + (size_t)spp; j++)
            {
              const dp_air_truth_t t   = dp_air_truth (&a, 0, j);
              const double         ch  = t.code_chips;
              const size_t         c0  = (size_t)floor (ch);
              const size_t         c1  = (c0 + 1) % 1023;
              const double _Complex lo = cexp (
                  -2.0 * M_PI * I * (t.car_cycles - floor (t.car_cycles)));
              const double _Complex z = (double _Complex)x[j] * lo;
              on += z * (code[c0] ? -1.0 : 1.0);
              off += z * (code[c1] ? -1.0 : 1.0);
              /* Half a chip either side of the truth: on any band the
                 correlation peak is symmetric about the true phase. */
              const size_t ce = (size_t)floor (fmod (ch + 1022.5, 1023.0));
              const size_t cl = (size_t)floor (fmod (ch + 0.5, 1023.0));
              early += z * (code[ce] ? -1.0 : 1.0);
              late += z * (code[cl] ? -1.0 : 1.0);
              energy += cabs (z);
              if (sym0 == -2)
                sym0 = t.symbol;
              else if (t.symbol != sym0)
                one_sym = 0;
            }
          /* Only periods inside one symbol: a data edge would halve it. */
          if (!one_sym)
            continue;
          periods++;
          full += cabs (on) > 0.85 * energy;
          off_low += cabs (off) < 0.1 * energy;
          real_ok += fabs (cimag (on)) < 0.1 * fabs (creal (on));
          balanced += cabs (on) > cabs (early) && cabs (on) > cabs (late)
                      && fabs (cabs (early) - cabs (late)) < 0.1 * cabs (on);
          const long pos  = sym0 % 1000;
          const long sign = creal (on) > 0.0 ? 1 : -1;
          if (pos < 275)
            {
              if (sym_sign[pos] && sym_sign[pos] != sign)
                window_mixed++;
              sym_sign[pos] = sign;
              window_mixed += sym_sign[0] && sign != sym_sign[0];
            }
          else
            {
              data_pos += sign > 0;
              data_neg += sign < 0;
            }
        }
      printf ("  despread on the truth at %u samples a chip: %zu periods, "
              "full %zu, real %zu, balanced %zu, one chip off near zero "
              "%zu; code-only window mixed %zu, data +%zu/-%zu\n",
              spc, periods, full, real_ok, balanced, off_low, window_mixed,
              data_pos, data_neg);
      DP_REQUIRE (periods > 50);
      DP_CHECK (real_ok == periods);
      DP_CHECK (balanced == periods);
      DP_CHECK (full == periods);
      DP_CHECK (off_low == periods);
      DP_CHECK (window_mixed == 0);
      DP_CHECK (data_pos > 5 && data_neg > 5);
      dp_air_free (&a);
    }
  /* 6. What the front end does to the despread signal, clean (no noise):
     every whole symbol despread on the truth's chips and carrier, its mean
     (the correlation the band keeps) and its spread about the mean (self-
     noise, from aliasing or anything else the front end adds), at the
     default four samples a chip and at two. Measured, 2026-11-17: the
     self-noise ceiling is near 60 dB at both settings and carriers, so
     folded energy does not limit the receiver; at 2.8 GHz the band keeps
     less of the correlation at four samples a chip than at two (0.913
     against 0.945, 0.30 dB), what the paired pull-in run lost there. At
     2.2 GHz four keeps slightly more (0.953 against 0.945). */
  for (int fci = 0; fci < 2; fci++)
    {
      double mean_at[2] = { 0.0, 0.0 };
      for (unsigned spc = 4; spc >= 2; spc -= 2)
        {
          const double     fc = fci ? 2.8e9 : 2.2e9;
          dp_air_emitter_t e  = emitter (code, fc, 0.0, 0.0, 50.0, 300, 7u);
          e.spc               = spc;
          dp_air_t a;
          DP_REQUIRE (dp_air_init (&a, &e, 1, FS, NAN, 0u, n));
          DP_REQUIRE (dp_air_next (&a, x, n));
          double s1 = 0.0, s2 = 0.0, pw = 0.0;
          size_t ns = 0, cnt = 0;
          long   cur = -1;
          double acc = 0.0;
          for (size_t j = 4000; j < n; j++)
            {
              const dp_air_truth_t t   = dp_air_truth (&a, 0, j);
              const double _Complex lo = cexp (
                  -2.0 * M_PI * I * (t.car_cycles - floor (t.car_cycles)));
              const double v
                  = creal ((double _Complex)x[j] * lo)
                    * (code[(size_t)floor (t.code_chips)] ? -1.0 : 1.0);
              pw += (double)crealf (x[j]) * crealf (x[j])
                    + (double)cimagf (x[j]) * cimagf (x[j]);
              if (t.symbol != cur)
                {
                  if (cur >= 0 && cnt > 100)
                    {
                      const double m = fabs (acc) / (double)cnt;
                      s1 += m;
                      s2 += m * m;
                      ns++;
                    }
                  cur = t.symbol;
                  acc = 0.0;
                  cnt = 0;
                }
              acc += v;
              cnt++;
            }
          const double mean    = s1 / (double)ns;
          const double var     = s2 / (double)ns - mean * mean;
          const double ceil_db = 10.0 * log10 (mean * mean / var);
          printf ("  despread at %.1f GHz, %u samples a chip: %zu symbols, "
                  "mean %.4f, power %.4f, self-noise ceiling %.1f dB\n",
                  fc / 1e9, spc, ns, mean, pw / (double)(n - 4000), ceil_db);
          DP_REQUIRE (ns > 200);
          DP_CHECK (ceil_db > 45.0); /* no measurable self-noise */
          mean_at[spc == 4 ? 0 : 1] = mean;
          dp_air_free (&a);
        }
      if (fci)
        DP_CHECK (20.0 * log10 (mean_at[0] / mean_at[1]) < -0.2);
    }

  /* 5. An emitter given its payload: every whole-symbol half period
     despread on the truth carries the bit dp_air_symbol_bit() names for
     that symbol, window symbols and data alike, under one fixed sign
     convention -- the payload runs only through data symbols. */
  {
    const size_t nb   = 4000;
    uint8_t     *bits = malloc (nb);
    uint32_t     st   = 77u;
    DP_REQUIRE (bits != NULL);
    for (size_t i = 0; i < nb; i++)
      bits[i] = (uint8_t)(dp_xs32 (&st) & 1u);
    dp_air_emitter_t e = emitter (code, 2.2e9, -300.0, 0.0, 50.0, 940, 7u);
    e.bits             = bits;
    e.n_bits           = nb;
    dp_air_t a;
    DP_REQUIRE (dp_air_init (&a, &e, 1, FS, NAN, 0u, n));
    DP_REQUIRE (dp_air_next (&a, x, n));
    /* Half a code period at a time: a whole one (318 us) is nearly a
       symbol (333 us) and seldom fits inside one. */
    const double spp   = 0.5 * FS / e.chip_rate * 1023.0;
    size_t       agree = 0, disagree = 0, window = 0;
    for (size_t p0 = 2000; p0 + (size_t)spp + 4 < n; p0 += (size_t)spp)
      {
        double _Complex on = 0;
        long sym0          = -2;
        int  one           = 1;
        for (size_t j = p0; j < p0 + (size_t)spp; j++)
          {
            const dp_air_truth_t t   = dp_air_truth (&a, 0, j);
            const double _Complex lo = cexp (
                -2.0 * M_PI * I * (t.car_cycles - floor (t.car_cycles)));
            on += (double _Complex)x[j] * lo
                  * (code[(size_t)floor (t.code_chips)] ? -1.0 : 1.0);
            if (sym0 == -2)
              sym0 = t.symbol;
            else if (t.symbol != sym0)
              one = 0;
          }
        if (!one)
          continue;
        const int b = dp_air_symbol_bit (&a, 0, sym0);
        DP_REQUIRE (b >= 0);
        window += sym0 % 1000 < 275;
        /* bit 0 -> +1 under the despread above */
        if ((creal (on) > 0.0) == (b == 0))
          agree++;
        else
          disagree++;
      }
    printf ("  payload on the truth: %zu periods agree with the named bit, "
            "%zu disagree (%zu in the window)\n",
            agree, disagree, window);
    DP_CHECK (agree > 50 && disagree == 0 && window > 5);
    DP_CHECK (!dp_air_data_ended (&a)); /* the payload outlasted the run */
    dp_air_free (&a);
    free (bits);
  }
  free (x);
  DP_TEST_END ("test_dp_air");
}
