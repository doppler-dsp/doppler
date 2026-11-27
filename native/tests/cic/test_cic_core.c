/* test_cic_core.c — CIC decimation filter unit tests.
 *
 * CIC is fixed at N=4 stages, M=1, power-of-two R.
 *
 * Covers:
 *   - Invalid constructor arguments → NULL
 *   - Output sample count: n_in/R for any block size multiple of R
 *   - DC response: settled output = 1.0 for ±1.0 DC input, real and complex
 *   - Zero input: output is exactly 0+0j throughout
 *   - Reset: second run with same input produces byte-identical output
 *   - Reconfigure: output count and DC response correct after R change
 *   - dp_cic_destroy(NULL): no crash
 *   - Alias rejection: stopband tone ≥ 20 dB below passband reference
 */
#include "doppler/cic/cic_core.h"
#include "doppler/dp_complex.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Feed n_in copies of `sample` through obj and return the last output.
 * `out` must hold at least ceil(n_in / R) elements.
 * Caller guarantees n_in is large enough that the filter has settled.
 */
static float _Complex dc_last (dp_cic_state_t *obj, float _Complex sample,
                               float _Complex *out, size_t n_in, size_t cap)
{
  float _Complex *in
      = (float _Complex *)malloc (n_in * sizeof (float _Complex));
  for (size_t i = 0; i < n_in; i++)
    in[i] = sample;
  size_t n = dp_cic_decimate (obj, in, n_in, out, cap);
  free (in);
  return out[n - 1];
}

/* The CIC as its definition, with no modular arithmetic. The sample is
   encoded as dp_cic_decimate() encodes it for this filter's word; the four
   boxcars are sliding sums of signed 64-bit values, which the word's width
   keeps inside an int64 at every stage (a boxcar of R words of `bits` bits
   four deep is under 2^(bits - 1) * R^4 <= 2^63 in size), so nothing here
   wraps; the midpoint goes on, the sum is floor-divided by R^4, and the
   sample is decoded. */
static void
cic_model (const dp_cic_state_t *c, const float _Complex *x, size_t n,
           float _Complex *y)
{
  const uint32_t R = c->R;
  int64_t       *v = malloc (n * sizeof *v), *w = malloc (n * sizeof *w);
  for (int part = 0; part < 2; part++)
    {
      for (size_t i = 0; i < n; i++)
        {
          float s = (part ? cimagf (x[i]) : crealf (x[i])) * c->in_scale;
          if (!(s <= CIC_WORD_TOP (c->bits)))
            s = CIC_WORD_TOP (c->bits);
          if (s < CIC_WORD_BOTTOM (c->bits))
            s = CIC_WORD_BOTTOM (c->bits);
          v[i] = (int64_t)s;
        }
      for (int o = 0; o < CIC_N; o++)
        {
          int64_t run = 0;
          for (size_t i = 0; i < n; i++)
            {
              if (i >= R)
                run -= v[i - R];
              run += v[i];
              w[i] = run;
            }
          memcpy (v, w, n * sizeof *v);
        }
      for (size_t k = 0; (k + 1) * R <= n; k++)
        {
          /* v + 2^(bits-1) * R^4 is never negative: as an unsigned sum
             (a negative int64 converts modulo 2^64, C99 6.3.1.3p2) it is
             the true value, and the shift floors it */
          const uint64_t u
              = (uint64_t)v[(k + 1) * R - 1] + (c->offset << c->shift);
          const float f
              = (float)((int64_t)(u >> c->shift) - (int64_t)c->offset)
                * c->out_scale;
          if (part)
            y[k] = CMPLXF (crealf (y[k]), f);
          else
            y[k] = CMPLXF (f, 0.0f);
        }
    }
  free (v);
  free (w);
}

/* The filter this one was before it had a precision: a 16-bit sample at
   full scale 2.0 with the offset-binary midpoint carried through the
   boxcars, written out with its own literals because it is a record of
   that filter and not a use of this one's constants. Each boxcar is the
   difference of a running sum, kept unsigned: the sum outgrows 64 bits at
   large R (signed, that is undefined behavior, UBSan's catch), and the
   difference of two wrapped sums is exact, since every boxcar fits. */
static void
cic_legacy (const float _Complex *x, size_t n, uint32_t R, float _Complex *y)
{
  int64_t      *v    = malloc (n * sizeof *v);
  uint64_t     *c    = malloc ((n + 1) * sizeof *c);
  const int64_t gain = (int64_t)R * R * R * R;
  for (int part = 0; part < 2; part++)
    {
      for (size_t i = 0; i < n; i++)
        {
          float s = (part ? cimagf (x[i]) : crealf (x[i])) * 16384.0f;
          if (s > 32767.0f)
            s = 32767.0f;
          if (s < -32768.0f)
            s = -32768.0f;
          v[i] = (int64_t)(int16_t)s + 32768;
        }
      for (int o = 0; o < 4; o++)
        {
          c[0] = 0;
          for (size_t i = 0; i < n; i++)
            c[i + 1] = c[i] + (uint64_t)v[i];
          for (size_t i = 0; i < n; i++)
            v[i] = (int64_t)(c[i + 1] - c[i + 1 >= R ? i + 1 - R : 0]);
        }
      for (size_t k = 0; (k + 1) * R <= n; k++)
        {
          float f = ((float)(v[(k + 1) * R - 1] / gain) - 32768.0f)
                    * (2.0f / 32768.0f);
          if (part)
            y[k] = CMPLXF (crealf (y[k]), f);
          else
            y[k] = CMPLXF (f, 0.0f);
        }
    }
  free (v);
  free (c);
}

static const uint32_t PRECISIONS[]
    = { CIC_PRECISION_16, CIC_PRECISION_32, CIC_PRECISION_64 };
enum
{
  N_PREC = 3
};

/* The word, its two ends, and the offset that goes around the pipeline.
     Sabotages, each shown red: the offset left out of the decode; added
   without the pipeline's gain; left on at the input as well; the NaN-safe
   comparison written as `s > hi`; one stage of gain too few in the shift;
   a 32-bit sum in the decode; CIC_PRECISION_64 given a 32-bit word; a
   word one bit wider than the register has left; the headroom left out of
   the scale; the top of the word one float too high. */
static void
test_the_word (void)
{
  /* What a filter is built with, and what each precision is. */
  {
    dp_cic_state_t *c = dp_cic_create (64);
    DP_CHECK (c != NULL);
    if (c)
      {
        const uint32_t room = CIC_REG_BITS - c->shift;
        DP_CHECK (dp_cic_get_precision (c) == CIC_PRECISION_DEFAULT);
        DP_CHECK (CIC_PRECISION_DEFAULT == CIC_PRECISION_64);
        DP_CHECK (dp_cic_get_bits (c) == room);
        DP_CHECK (dp_cic_get_headroom_bits (c) == CIC_HEADROOM_BITS_WIDE);
        DP_CHECK (dp_cic_get_full_scale (c) == (double)CIC_PAPR_WIDE);
        DP_CHECK (dp_cic_get_full_scale (c) == 4.0); /* the stated ratio */
        DP_CHECK (dp_cic_get_step (c)
                  == dp_cic_get_full_scale (c) / ldexp (1.0, (int)room - 1));
        DP_CHECK (dp_cic_set_precision (c, CIC_PRECISION_32) == DP_OK);
        DP_CHECK (dp_cic_get_bits (c) == CIC_PRECISION_32);
        DP_CHECK (dp_cic_get_full_scale (c) == (double)CIC_PAPR_WIDE);
        DP_CHECK (dp_cic_set_precision (c, CIC_PRECISION_16) == DP_OK);
        DP_CHECK (dp_cic_get_bits (c) == CIC_PRECISION_16);
        DP_CHECK (dp_cic_get_headroom_bits (c) == CIC_HEADROOM_BITS_16);
        DP_CHECK (dp_cic_get_full_scale (c) == (double)CIC_PAPR_16);
        DP_CHECK (dp_cic_get_full_scale (c) == 2.0);
        DP_CHECK (dp_cic_get_step (c) == 2.0 / 32768.0);
        /* the headroom is its own setting, and a precision puts it back */
        DP_CHECK (dp_cic_set_headroom_bits (c, 4) == DP_OK);
        DP_CHECK (dp_cic_get_full_scale (c) == 16.0);
        DP_CHECK (dp_cic_get_step (c) == 16.0 / 32768.0);
        DP_CHECK (dp_cic_set_precision (c, CIC_PRECISION_64) == DP_OK);
        DP_CHECK (dp_cic_get_headroom_bits (c) == CIC_HEADROOM_BITS_WIDE);
        /* refusals change nothing */
        dp_cic_state_t before = *c;
        DP_CHECK (dp_cic_set_precision (c, 0) == DP_ERR_INVALID);
        DP_CHECK (dp_cic_set_precision (c, 8) == DP_ERR_INVALID);
        DP_CHECK (dp_cic_set_precision (c, 33) == DP_ERR_INVALID);
        DP_CHECK (dp_cic_set_headroom_bits (c, CIC_HEADROOM_BITS_MAX + 1u)
                  == DP_ERR_INVALID);
        DP_CHECK (memcmp (&before, c, sizeof before) == 0);
        DP_CHECK (dp_cic_set_headroom_bits (c, CIC_HEADROOM_BITS_MAX)
                  == DP_OK);
        DP_CHECK (dp_cic_set_headroom_bits (c, 0) == DP_OK);
        DP_CHECK (dp_cic_get_full_scale (c) == 1.0);
      }
    dp_cic_destroy (c);
    /* past R = 256 the register has under 32 bits left, and the two wide
       precisions are one word */
    for (uint32_t R = 2; R <= CIC_R_MAX; R <<= 1)
      {
        dp_cic_state_t *w = dp_cic_create (R);
        DP_CHECK (w != NULL);
        if (!w)
          continue;
        const uint32_t room = CIC_REG_BITS - w->shift;
        DP_CHECK (room > CIC_PRECISION_16); /* what CIC_R_MAX is for */
        DP_CHECK (dp_cic_get_bits (w) == room);
        DP_CHECK (dp_cic_set_precision (w, CIC_PRECISION_32) == DP_OK);
        DP_CHECK (dp_cic_get_bits (w)
                  == (room < CIC_PRECISION_32 ? room : CIC_PRECISION_32));
        DP_CHECK ((R > 256) == (room < CIC_PRECISION_32));
        dp_cic_destroy (w);
      }
  }

  /* A setting resets the filter and travels in the blob: a twin built
     with nothing set resumes a configured stream bit for bit. */
  {
    const uint32_t  R = 16;
    const size_t    n = (size_t)R * 40u, no = n / R;
    dp_cic_state_t *a = dp_cic_create (R), *b = dp_cic_create (R);
    float _Complex *x  = malloc (n * sizeof *x);
    float _Complex *ya = malloc (no * sizeof *ya),
                   *yb = malloc (no * sizeof *yb);
    DP_CHECK (a && b && x && ya && yb);
    if (a && b && x && ya && yb)
      {
        uint32_t st = 5u;
        for (size_t i = 0; i < n; i++)
          {
            const double u = dp_uni (&st) - 0.5;
            const double w = dp_uni (&st) - 0.5;
            x[i]           = CMPLXF ((float)(20.0 * u), (float)w);
          }
        (void)dp_cic_decimate (a, x, n, ya, no);
        DP_CHECK (a->clipped && a->integ_re[0] != 0);
        DP_CHECK (dp_cic_set_precision (a, CIC_PRECISION_32) == DP_OK);
        DP_CHECK (!a->clipped && a->integ_re[0] == 0 && a->phase == 0);
        DP_CHECK (dp_cic_set_headroom_bits (a, 5) == DP_OK);
        (void)dp_cic_decimate (a, x, n / 2, ya, no);
        DP_CHECK (!a->clipped); /* 10 of full scale 32 */
        size_t nb   = dp_cic_state_bytes (a);
        char  *blob = malloc (nb);
        DP_CHECK (blob != NULL);
        if (blob)
          {
            dp_cic_get_state (a, blob);
            DP_CHECK (dp_cic_set_state (b, blob) == DP_OK);
            DP_CHECK (dp_cic_get_precision (b) == CIC_PRECISION_32
                      && dp_cic_get_headroom_bits (b) == 5);
            size_t ka = dp_cic_decimate (a, x + n / 2, n / 2, ya, no);
            size_t kb = dp_cic_decimate (b, x + n / 2, n / 2, yb, no);
            DP_CHECK (ka == kb && memcmp (ya, yb, ka * sizeof *ya) == 0);
            /* a precision or a headroom no filter has is refused */
            dp_cic_state_t bad = *a, keep = *b;
            bad.precision = 17;
            dp_cic_get_state (&bad, blob);
            DP_CHECK (dp_cic_set_state (b, blob) == DP_ERR_INVALID);
            bad               = *a;
            bad.headroom_bits = CIC_HEADROOM_BITS_MAX + 1u;
            dp_cic_get_state (&bad, blob);
            DP_CHECK (dp_cic_set_state (b, blob) == DP_ERR_INVALID);
            DP_CHECK (memcmp (&keep, b, sizeof keep) == 0);
          }
        free (blob);
      }
    dp_cic_destroy (a);
    dp_cic_destroy (b);
    free (x);
    free (ya);
    free (yb);
  }

  /* Silence in is exact zeros out, from the first output after create. */
  static const uint32_t rs[] = { 2, 16, 64, CIC_R_MAX };
  for (size_t pi = 0; pi < N_PREC; pi++)
    for (size_t ri = 0; ri < sizeof rs / sizeof *rs; ri++)
      {
        const uint32_t  R = rs[ri];
        const size_t    n = (size_t)R * 8u;
        dp_cic_state_t *c = dp_cic_create (R);
        float _Complex *x = calloc (n, sizeof *x), *y = malloc (8 * sizeof *y);
        DP_CHECK (c != NULL && x != NULL && y != NULL);
        if (c && x && y)
          {
            DP_CHECK (dp_cic_set_precision (c, PRECISIONS[pi]) == DP_OK);
            DP_CHECK (dp_cic_decimate (c, x, n, y, 8) == 8);
            for (size_t k = 0; k < 8; k++)
              DP_CHECK (crealf (y[k]) == 0.0f && cimagf (y[k]) == 0.0f);
            for (int i = 0; i < CIC_N; i++)
              DP_CHECK (c->integ_re[i] == 0 && c->comb_im[i] == 0);
          }
        dp_cic_destroy (c);
        free (x);
        free (y);
      }

  /* Every output is the definition's, bit for bit, at each precision; and
     at CIC_PRECISION_16 it is, past the fill, the output of the filter
     this one was. Tones from 1/512 of the output rate to the input's
     Nyquist and noise, at unit amplitude, just under full scale, at it,
     and clipping. */
  static const uint32_t rm[]   = { 2, 16, 64, 512, CIC_R_MAX };
  static const float    frac[] = { 0.25f, 0.9995f, 1.0f, 1.3f };
  static const double   cyc[]  = { 1.0 / 512.0,
                                   0.05,
                                   0.25,
                                   0.49,
                                   0.5,
                                   1.0,
                                   -1.0 /* the input's Nyquist, less a bit */,
                                   0.0 /* noise */ };
  size_t                rows = 0, legacy_rows = 0, legacy_fill = 0;
  double                worst16 = 0.0, worst64 = 0.0;
  for (size_t pi = 0; pi < N_PREC; pi++)
    for (size_t ri = 0; ri < sizeof rm / sizeof *rm; ri++)
      for (size_t ai = 0; ai < sizeof frac / sizeof *frac; ai++)
        for (size_t ci = 0; ci < sizeof cyc / sizeof *cyc; ci++)
          {
            const uint32_t  R  = rm[ri];
            const size_t    n  = (size_t)R * (R > 64 ? 24u : 400u);
            const size_t    no = n / R;
            dp_cic_state_t *c  = dp_cic_create (R);
            float _Complex *x  = malloc (n * sizeof *x);
            float _Complex *y  = malloc (no * sizeof *y);
            float _Complex *m  = malloc (no * sizeof *m);
            DP_CHECK (c && x && y && m);
            if (c && x && y && m)
              {
                DP_CHECK (dp_cic_set_precision (c, PRECISIONS[pi]) == DP_OK);
                const double fs  = dp_cic_get_full_scale (c);
                const double amp = (double)frac[ai] * fs;
                uint32_t     st
                    = 0xC1Cu + (uint32_t)(pi * 1000 + ri * 100 + ai * 10 + ci);
                double f = cyc[ci] < 0.0 ? 0.5 - 0.01 / R : cyc[ci] / R;
                for (size_t i = 0; i < n; i++)
                  if (cyc[ci] == 0.0)
                    {
                      const double a = 2.0 * dp_uni (&st) - 1.0;
                      const double b = 2.0 * dp_uni (&st) - 1.0;
                      x[i] = CMPLXF ((float)(amp * a), (float)(amp * b));
                    }
                  else
                    x[i] = CMPLXF (
                        (float)(amp * cos (2.0 * M_PI * f * (double)i)),
                        (float)(amp * sin (2.0 * M_PI * f * (double)i)));
                DP_CHECK (dp_cic_decimate (c, x, n, y, no) == no);
                cic_model (c, x, n, m);
                DP_CHECK (memcmp (y, m, no * sizeof *y) == 0);
                /* under the bound nothing clips, over it something does;
                   at it, whether a sample lands on the top step is the
                   stream's doing */
                if (frac[ai] < 1.0f)
                  DP_CHECK (!c->clipped);
                if (frac[ai] > 1.0f)
                  DP_CHECK (c->clipped);
                if (PRECISIONS[pi] == CIC_PRECISION_16)
                  {
                    cic_legacy (x, n, R, m);
                    /* past the first CIC_N - 1, which are the fill */
                    DP_CHECK (memcmp (y + (CIC_N - 1), m + (CIC_N - 1),
                                      (no - (CIC_N - 1)) * sizeof *y)
                              == 0);
                    legacy_rows++;
                    legacy_fill += memcmp (y, m, (CIC_N - 1) * sizeof *y) != 0;
                  }
                /* How far the output is from the boxcars' exact sum of the
                   float input, on the slow tone well inside the range:
                   the word's doing. */
                if (ci == 0 && ai == 0 && R == 64)
                  {
                    double worst = 0.0;
                    for (size_t k = CIC_N; k < no; k++)
                      {
                        /* a tone through four boxcars of R is the tone
                           scaled by (sin (pi f R) / (R sin (pi f)))^4 and
                           centered CIC_N (R - 1) / 2 behind the newest */
                        const double g
                            = pow (sin (M_PI * f * R) / (R * sin (M_PI * f)),
                                   (double)CIC_N);
                        const double t
                            = (double)((k + 1) * R - 1)
                              - 0.5 * (double)CIC_N * (double)(R - 1);
                        const double d
                            = fabs ((double)crealf (y[k])
                                    - amp * g * cos (2.0 * M_PI * f * t));
                        if (d > worst)
                          worst = d;
                      }
                    if (PRECISIONS[pi] == CIC_PRECISION_16)
                      worst16 = worst / dp_cic_get_step (c);
                    if (PRECISIONS[pi] == CIC_PRECISION_64)
                      worst64 = worst / amp;
                  }
                rows++;
              }
            dp_cic_destroy (c);
            free (x);
            free (y);
            free (m);
          }
  printf ("  the word: %zu rows equal the definition on every output; %zu "
          "of them at 16 bits equal the filter this one was past the fill, "
          "and differ from it inside the fill on %zu\n",
          rows, legacy_rows, legacy_fill);
  printf ("  the word: a slow tone at a quarter of full scale is off its "
          "exact value by %.2f steps at 16 bits and by %.1e of itself at "
          "64\n",
          worst16, worst64);
  DP_CHECK (rows == N_PREC * 5 * 4 * 8);
  DP_CHECK (legacy_rows == 5 * 4 * 8 && legacy_fill == legacy_rows);
  /* at 16 bits the error is the word's step (the encoder truncates and
     the decode floors, so it is under two of them and over a tenth); at
     64 it is the float's own rounding, 2^-24 of the value and a little */
  DP_CHECK (worst16 > 0.1 && worst16 < 2.0);
  DP_CHECK (worst64 < 4.0 * ldexp (1.0, -24));

  /* The registers wrap and the answer does not: both rails as DC, at
     every ratio, every precision and four headrooms (none, the
     precision's own, 4 bits and CIC_HEADROOM_BITS_MAX), far past where the
     last integrator passes 2^CIC_REG_BITS. Every output past the fill is the
     input. The rail is built from dp_cic_get_step(), not from the filter's own
     scale fields, so a scale that is wrong at a set headroom shows. Sabotage,
     shown red: the output scale taken at the precision's own headroom rather
     than the one set. */
  static const int hrs[]
      = { 0, -1 /* the precision's own */, 4, (int)CIC_HEADROOM_BITS_MAX };
  for (size_t pi = 0; pi < N_PREC; pi++)
    for (size_t hi = 0; hi < sizeof hrs / sizeof *hrs; hi++)
      for (uint32_t R = 2; R <= CIC_R_MAX; R <<= 1)
        {
          const size_t n  = (size_t)R * 64u > 8192u ? (size_t)R * 64u : 8192u;
          const size_t no = n / R;
          float _Complex *x = malloc (n * sizeof *x);
          float _Complex *y = malloc (no * sizeof *y);
          DP_CHECK (x != NULL && y != NULL);
          for (int r = 0; x && y && r < 2; r++)
            {
              dp_cic_state_t *c = dp_cic_create (R);
              DP_CHECK (c != NULL);
              if (!c)
                continue;
              DP_CHECK (dp_cic_set_precision (c, PRECISIONS[pi]) == DP_OK);
              if (hrs[hi] >= 0)
                DP_CHECK (dp_cic_set_headroom_bits (c, (uint32_t)hrs[hi])
                          == DP_OK);
              /* the top and the bottom of the word, as inputs */
              const float rail
                  = (r ? CIC_WORD_BOTTOM (c->bits) : CIC_WORD_TOP (c->bits))
                    * (float)dp_cic_get_step (c);
              for (size_t i = 0; i < n; i++)
                x[i] = CMPLXF (rail, -0.5f * rail);
              DP_CHECK (dp_cic_decimate (c, x, n, y, no) == no);
              size_t bad = 0;
              for (size_t k = CIC_N; k < no; k++)
                bad += crealf (y[k]) != rail;
              DP_CHECK (bad == 0);
              DP_CHECK (!c->clipped);
              /* and one float past the top is over the bound */
              x[0] = CMPLXF (nextafterf (CIC_WORD_TOP (c->bits), INFINITY)
                                 * (float)dp_cic_get_step (c),
                             0.0f);
              (void)dp_cic_decimate (c, x, R, y, no);
              DP_CHECK (c->clipped);
              dp_cic_destroy (c);
            }
          free (x);
          free (y);
        }

  /* A sample that is not a number is over the bound: it reads as the top
     of the range, raises the flag, and is gone when it has left the
     boxcars. The stream with a NaN, +inf and -inf in it gives exactly the
     outputs of the same stream with values past the two rails in their
     places. */
  for (size_t pi = 0; pi < N_PREC; pi++)
    {
      const uint32_t  R = 16;
      const size_t    n = (size_t)R * 64u, no = n / R;
      dp_cic_state_t *a = dp_cic_create (R), *b = dp_cic_create (R);
      float _Complex *x = malloc (n * sizeof *x), *z = malloc (n * sizeof *z);
      float _Complex *ya = malloc (no * sizeof *ya),
                     *yb = malloc (no * sizeof *yb);
      DP_CHECK (a && b && x && z && ya && yb);
      if (a && b && x && z && ya && yb)
        {
          DP_CHECK (dp_cic_set_precision (a, PRECISIONS[pi]) == DP_OK);
          DP_CHECK (dp_cic_set_precision (b, PRECISIONS[pi]) == DP_OK);
          const float over = 2.0f * (float)dp_cic_get_full_scale (a);
          uint32_t    st   = 77u;
          for (size_t i = 0; i < n; i++)
            {
              const double u = dp_uni (&st) - 0.5;
              const double w = dp_uni (&st) - 0.5;
              x[i]           = CMPLXF ((float)u, (float)w);
              z[i]           = x[i];
            }
          x[100] = CMPLXF (NAN, cimagf (x[100]));
          z[100] = CMPLXF (over, cimagf (z[100]));
          x[200] = CMPLXF (crealf (x[200]), INFINITY);
          z[200] = CMPLXF (crealf (z[200]), over);
          x[300] = CMPLXF (-INFINITY, NAN);
          z[300] = CMPLXF (-over, over);
          DP_CHECK (dp_cic_decimate (a, x, n, ya, no) == no);
          DP_CHECK (dp_cic_decimate (b, z, n, yb, no) == no);
          DP_CHECK (a->clipped && b->clipped);
          DP_CHECK (memcmp (ya, yb, no * sizeof *ya) == 0);
          size_t finite = 0;
          for (size_t k = 0; k < no; k++)
            finite += isfinite (crealf (ya[k])) && isfinite (cimagf (ya[k]));
          DP_CHECK (finite == no);
        }
      dp_cic_destroy (a);
      dp_cic_destroy (b);
      free (x);
      free (z);
      free (ya);
      free (yb);
    }
}

int
main (void)
{

  /* ── Invalid constructor args → NULL ─────────────────────────────────── */
  DP_CHECK (dp_cic_create (0) == NULL);    /* R = 0 */
  DP_CHECK (dp_cic_create (1) == NULL);    /* R = 1: < 2 */
  DP_CHECK (dp_cic_create (3) == NULL);    /* non-power-of-two */
  DP_CHECK (dp_cic_create (8192) == NULL); /* R > 4096 */

  /* NULL destroy is a documented no-op */
  dp_cic_destroy (NULL);

  /* ── shift field: CIC_N * log2(R) ────────────────────────────────────── */
  {
    dp_cic_state_t *obj = dp_cic_create (16);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->R == 16);
    DP_CHECK (obj->shift == 16); /* CIC_N=4, log2(16)=4 → 4*4=16 */
    dp_cic_destroy (obj);
  }
  {
    dp_cic_state_t *obj = dp_cic_create (8);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->shift == 12); /* CIC_N=4, log2(8)=3 → 4*3=12 */
    dp_cic_destroy (obj);
  }

  /* ── Output sample count ─────────────────────────────────────────────── */
  /* For a fresh filter, n_in = k*R must produce exactly k outputs. */
  {
    uint32_t        R   = 8;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    float _Complex in[256] = { 0 }, out[256];
    for (int k = 1; k <= 4; k++)
      {
        dp_cic_reset (obj);
        size_t n = dp_cic_decimate (obj, in, (size_t)k * R, out, 256);
        DP_CHECK (n == (size_t)k);
      }
    /* Partial block: R-1 inputs → 0 outputs */
    dp_cic_reset (obj);
    DP_CHECK (dp_cic_decimate (obj, in, R - 1, out, 256) == 0);
    /* Then 1 more input completes the first decimation cycle */
    DP_CHECK (dp_cic_decimate (obj, in, 1, out, 256) == 1);
    dp_cic_destroy (obj);
  }

  /* ── Zero input → zero settled output ───────────────────────────────── */
  /* With offset-binary encoding, zero input maps to u=32768 (not 0), so
   * the integrators ramp during the first CIC_N output periods before the
   * comb delay chain fills.  From output index CIC_N onward the output is
   * exactly 0+0j. */
  {
    dp_cic_state_t *obj = dp_cic_create (4);
    DP_CHECK (obj != NULL);
    float _Complex in[64] = { 0 }, out[64];
    size_t n              = dp_cic_decimate (obj, in, 64, out, 64);
    DP_CHECK (n == 16);
    for (size_t i = CIC_N; i < n; i++)
      DP_CHECK (dp_cnearf (out[i], 0.0f, 0.0f));
    dp_cic_destroy (obj);
  }

  /* ── DC response: +1.0 real ──────────────────────────────────────────── */
  /* Transient ≈ CIC_N*(R-1) input samples; 8*R*CIC_N ensures full settling. */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 8 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, 1.0f + 0.0f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, 1.0f + 0.0f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── DC response: −1.0 real (tests signed two's-complement path) ─────── */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 8 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, -1.0f + 0.0f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, -1.0f + 0.0f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── DC response: +j (imaginary path independent of real) ────────────── */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 8 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, 0.0f + 1.0f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, 0.0f + 1.0f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── DC response: (0.5 + 0.5j) — typical SDR config R=32 ────────────── */
  /* Transient ≈ CIC_N*(32-1) = 124 inputs; 12*R outputs ensures settling. */
  {
    uint32_t        R   = 32;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 12 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, 0.5f + 0.5f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, 0.5f + 0.5f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── Reset: second run produces byte-identical output ────────────────── */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 64;
    float _Complex *in   = malloc (n_in * sizeof (float _Complex));
    float _Complex *out1 = malloc (n_in * sizeof (float _Complex));
    float _Complex *out2 = malloc (n_in * sizeof (float _Complex));
    /* non-trivial input: ramp on real, constant on imag */
    for (size_t i = 0; i < n_in; i++)
      in[i] = (float)i * 0.01f + 0.5f * I;

    size_t n1 = dp_cic_decimate (obj, in, n_in, out1, n_in);
    dp_cic_reset (obj);
    size_t n2 = dp_cic_decimate (obj, in, n_in, out2, n_in);
    DP_CHECK (n1 == n2);
    DP_CHECK (memcmp (out1, out2, n1 * sizeof (float _Complex)) == 0);

    free (in);
    free (out1);
    free (out2);
    dp_cic_destroy (obj);
  }

  /* ── Reconfigure: output count and DC response update correctly ──────── */
  {
    dp_cic_state_t *obj = dp_cic_create (4);
    DP_CHECK (obj != NULL);
    float _Complex in[256], out[256];
    for (int i = 0; i < 256; i++)
      in[i] = 1.0f;

    /* warm up with R=4 */
    dp_cic_decimate (obj, in, 32, out, 256);

    /* reconfigure to R=8 */
    dp_cic_reconfigure (obj, 8);
    DP_CHECK (obj->R == 8);
    DP_CHECK (obj->shift == 12); /* CIC_N=4, log2(8)=3 */

    /* output count must reflect new R */
    size_t n = dp_cic_decimate (obj, in, 8 * 8 * CIC_N, out, 256);
    DP_CHECK (n == (size_t)(8 * CIC_N));

    /* settled output must be 1.0 */
    DP_CHECK (dp_cnearf (out[n - 1], 1.0f + 0.0f * I, 4e-5f));
    dp_cic_destroy (obj);
  }

  /* ── Reconfigure: invalid args are silently ignored ─────────────────── */
  {
    dp_cic_state_t *obj = dp_cic_create (8);
    DP_CHECK (obj != NULL);
    dp_cic_reconfigure (obj, 0); /* R=0: invalid, ignored */
    DP_CHECK (obj->R == 8);
    dp_cic_reconfigure (obj, 3); /* non-power-of-two: invalid, ignored */
    DP_CHECK (obj->R == 8);
    dp_cic_reconfigure (obj, 8192); /* R > 4096: invalid, ignored */
    DP_CHECK (obj->R == 8);
    dp_cic_destroy (obj);
  }

  /* ── Streaming: split block across two calls ─────────────────────────── */
  /* Two calls of R samples must give same result as one call of 2R samples */
  {
    uint32_t R = 16;
    float _Complex in[64], out_split[4], out_whole[4];
    for (int i = 0; i < 64; i++)
      in[i] = 0.7f - 0.3f * I;

    dp_cic_state_t *a = dp_cic_create (R);
    dp_cic_state_t *b = dp_cic_create (R);
    DP_CHECK (a && b);

    /* whole: 2R in one call */
    dp_cic_decimate (b, in, 2 * R, out_whole, 4);

    /* split: R then R */
    dp_cic_decimate (a, in, R, out_split, 4);
    dp_cic_decimate (a, in + R, R, out_split + 1, 3);

    DP_CHECK (dp_cnearf (out_split[0], out_whole[0], 0.0f));
    DP_CHECK (dp_cnearf (out_split[1], out_whole[1], 0.0f));

    dp_cic_destroy (a);
    dp_cic_destroy (b);
  }

  /* ── Alias rejection: stopband tone must be heavily attenuated ───────── */
  /* Feed a tone at 0.95*fs/R (near first CIC null) and compare output
   * power to a DC (passband) reference.  Requires ≥ 20 dB rejection. */
  {
    uint32_t R       = 8;
    double   f_alias = 0.95 / R; /* just inside first null at fs/R */
    size_t   n_in    = 32 * R * CIC_N;
    size_t   n_out   = n_in / R;
    size_t   n_drop  = CIC_N * (R - 1) / R + 2; /* skip transient */
    size_t   n_meas  = n_out - n_drop;

    float _Complex *in  = malloc (n_in * sizeof (float _Complex));
    float _Complex *out = malloc (n_out * sizeof (float _Complex));

    /* passband reference: DC input → should be ~1.0 at output */
    for (size_t i = 0; i < n_in; i++)
      in[i] = 1.0f + 0.0f * I;
    dp_cic_state_t *obj = dp_cic_create (R);
    dp_cic_decimate (obj, in, n_in, out, n_out);
    double pwr_pass = 0.0;
    for (size_t i = n_drop; i < n_out; i++)
      pwr_pass += (double)cabsf (out[i]) * cabsf (out[i]);
    pwr_pass /= (double)n_meas;

    /* alias-zone tone */
    dp_cic_reset (obj);
    for (size_t i = 0; i < n_in; i++)
      in[i] = CMPLXF ((float)cos (2 * M_PI * f_alias * i),
                      (float)sin (2 * M_PI * f_alias * i));
    dp_cic_decimate (obj, in, n_in, out, n_out);
    double pwr_alias = 0.0;
    for (size_t i = n_drop; i < n_out; i++)
      pwr_alias += (double)cabsf (out[i]) * cabsf (out[i]);
    pwr_alias /= (double)n_meas;

    double rejection_db = 10.0 * log10 (pwr_pass / (pwr_alias + 1e-300));
    DP_CHECK (rejection_db >= 20.0);

    dp_cic_destroy (obj);
    free (in);
    free (out);
  }

  /* ── Serializable state round-trip — the elastic-resume guarantee ─────────
   * Split a stream at a mid-decimation-cycle cut, hand the state to a fresh
   * CIC, and continue: output equals an uninterrupted run byte-for-byte. */
  {
    const uint32_t  R  = 16;
    const size_t    L  = 320;
    float _Complex *in = malloc (L * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = 2.0f * ((float)(i % 7) - 3.0f) + I * ((float)(i % 5) - 2.0f);
    float _Complex outA[64], outB[64];

    dp_cic_state_t *ra = dp_cic_create (R);
    size_t          nA = dp_cic_decimate (ra, in, L, outA, 64);
    dp_cic_destroy (ra);

    const size_t    cut = 173; /* not a multiple of R → mid-cycle phase */
    dp_cic_state_t *r1  = dp_cic_create (R);
    size_t          nB  = dp_cic_decimate (r1, in, cut, outB, 64);
    size_t          sb  = dp_cic_state_bytes (r1);
    DP_CHECK (sb
              == sizeof (dp_state_hdr_t) + 3 * sizeof (uint32_t)
                     + 4 * CIC_N * sizeof (uint64_t) + sizeof (uint32_t)
                     + sizeof (uint8_t));
    /* This block's own input runs to +-6, past the bound as built —
       so the sticky flag must be up, and must survive the round trip.  A
       resumed stream that forgot it had clipped would answer wrongly. */
    DP_CHECK (r1->clipped == 1);
    void *blob = malloc (sb);
    dp_cic_get_state (r1, blob);
    dp_cic_destroy (r1);

    dp_cic_state_t *r2 = dp_cic_create (R);
    DP_CHECK (r2->clipped == 0);
    DP_CHECK (dp_cic_set_state (r2, blob) == DP_OK);
    DP_CHECK (r2->clipped == 1);
    /* standard envelope: a magic-clobbered blob is rejected, r2 untouched */
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (dp_cic_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += dp_cic_decimate (r2, in + cut, L - cut, outB + nB, 64 - nB);
    dp_cic_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    DP_CHECK (nA > 0
              && memcmp (outA, outB, nA * sizeof (float _Complex)) == 0);
    free (in);
  }

  /* ── sticky clip flag: the only signal that the input bound was
   *    exceeded, since the sample stream stays finite and plausible ────── */
  {
    dp_cic_state_t *obj = dp_cic_create (16);
    float _Complex in[64];
    DP_CHECK (obj->clipped == 0);
    /* The bound is the filter's full scale, not 1.0: the word reserves that
       headroom precisely so a unit-amplitude signal's peaks fit (an RRC
       stream peaks at 1.582x its symbol amplitude), so 1.5 is in range. */
    for (size_t i = 0; i < 64; i++)
      in[i] = 0.9f + 0.9f * I;
    float _Complex out[8];
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 0); /* in range — no false positive */

    for (size_t i = 0; i < 64; i++)
      in[i] = 1.5f + 0.0f * I;
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 0); /* inside the PAPR headroom */

    for (size_t i = 0; i < 64; i++)
      in[i] = 1.05f * (float)dp_cic_get_full_scale (obj) + 0.0f * I;
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 1);

    for (size_t i = 0; i < 64; i++)
      in[i] = 0.1f + 0.0f * I;
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 1); /* sticky across later in-range blocks */

    dp_cic_reset (obj);
    DP_CHECK (obj->clipped == 0); /* cleared only by reset() */

    /* every component and sign is caught */
    const float B               = 1.05f * (float)dp_cic_get_full_scale (obj);
    const float _Complex bad[4] = { B, -B, B * I, -B * I };
    for (size_t k = 0; k < 4; k++)
      {
        dp_cic_reset (obj);
        for (size_t i = 0; i < 64; i++)
          in[i] = bad[k];
        dp_cic_decimate (obj, in, 64, out, 8);
        DP_CHECK (obj->clipped == 1);
      }
    dp_cic_destroy (obj);
  }

  /* ── short out: the filter still runs, only emission truncates ──────
   * The integrators and combs must advance over every input sample even
   * when there is no room to write the result -- otherwise the pipeline
   * desynchronises from the stream.  Feed the same input twice, once with
   * room and once without, and check the STATE lands in the same place. */
  {
    const uint32_t R    = 4;
    const size_t   n_in = 64, n_full = n_in / R; /* 16 outputs */
    float _Complex in[64];
    for (size_t i = 0; i < n_in; i++)
      in[i] = CMPLXF ((float)cos (2 * M_PI * 0.05 * (double)i) * 0.5f,
                      (float)sin (2 * M_PI * 0.05 * (double)i) * 0.5f);

    float _Complex full[16], part[16];
    const float _Complex CANARY = -1234.0f - 567.0f * I;
    const size_t K              = 5;

    dp_cic_state_t *a = dp_cic_create (R);
    DP_CHECK (dp_cic_decimate (a, in, n_in, full, n_full) == n_full);

    dp_cic_state_t *b = dp_cic_create (R);
    for (size_t k = 0; k < n_full; k++)
      part[k] = CANARY;
    DP_CHECK (dp_cic_decimate (b, in, n_in, part, K) == K);
    /* what was written is the true prefix ... */
    for (size_t k = 0; k < K; k++)
      DP_CHECK (dp_cnearf (part[k], full[k], 0.0f));
    /* ... and nothing past the capacity was touched. */
    for (size_t k = K; k < n_full; k++)
      DP_CHECK (dp_cnearf (part[k], CANARY, 0.0f));
    /* ... and both filters are in the SAME state: the next block agrees. */
    float _Complex nextA[16], nextB[16];
    size_t nA = dp_cic_decimate (a, in, n_in, nextA, n_full);
    size_t nB = dp_cic_decimate (b, in, n_in, nextB, n_full);
    DP_CHECK (nA == nB);
    for (size_t k = 0; k < nA; k++)
      DP_CHECK (dp_cnearf (nextA[k], nextB[k], 0.0f));

    dp_cic_destroy (a);
    dp_cic_destroy (b);
  }

  /* R travels in the blob (doppler#2642): reconfigure() changes it, so by
     #2022's rule its value must restore. A blob from R = 64 restored into
     an R = 16 CIC used to be accepted at the R = 16 gain; it now restores
     R = 64 (and the shift derived from it), and the continuation matches
     the R = 64 source exactly. An R create() refuses, or a phase not below
     R, is refused and the target's blob is byte-identical afterwards. */
  {
    const size_t    L = 1000, cut = 333;
    float _Complex *in = malloc (L * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = 0.3f * (float)((i % 5) - 2) + I * 0.2f * (float)((i % 3) - 1);
    float _Complex ref[32], got[32];

    dp_cic_state_t *src = dp_cic_create (64);
    (void)dp_cic_decimate (src, in, cut, ref, 32);
    size_t sb = dp_cic_state_bytes (src);
    /* The forgeries below write R just past the envelope and the phase
       five bytes from the end. Requiring room for both up front also tells
       the compiler the offsets cannot wrap: without it CI's gcc saw a path
       where `sb - 5` underflows and flagged a 4-byte memcpy into a region
       of size 0 (-Wstringop-overflow). */
    DP_REQUIRE (sb >= sizeof (dp_state_hdr_t) + sizeof (uint32_t)
                          + sizeof (uint8_t) + sizeof (uint32_t));
    unsigned char *blob = malloc (sb);
    dp_cic_get_state (src, blob);
    size_t nref = dp_cic_decimate (src, in + cut, L - cut, ref, 32);

    dp_cic_state_t *dst = dp_cic_create (16);
    DP_CHECK (dp_cic_set_state (dst, blob) == DP_OK);
    DP_CHECK (dst->R == 64 && dst->shift == src->shift);
    size_t ngot = dp_cic_decimate (dst, in + cut, L - cut, got, 32);
    DP_CHECK (nref == ngot && nref > 0);
    DP_CHECK (memcmp (ref, got, nref * sizeof (float _Complex)) == 0);

    /* Forged R and phase: the envelope passes, the payload check refuses. */
    unsigned char *forged = malloc (sb), *before = malloc (sb),
                  *after = malloc (sb);
    dp_cic_state_t *t    = dp_cic_create (16);
    dp_cic_get_state (t, before);
    const uint32_t bad_r[] = { 0, 1, 3, 2 * CIC_R_MAX };
    for (size_t k = 0; k < sizeof bad_r / sizeof bad_r[0]; k++)
      {
        memcpy (forged, blob, sb);
        memcpy (forged + sizeof (dp_state_hdr_t), &bad_r[k],
                sizeof (uint32_t));
        DP_CHECK (
            dp_state_validate (forged, sb, CIC_STATE_MAGIC, CIC_STATE_VERSION)
            == DP_OK);
        DP_CHECK (dp_cic_set_state (t, forged) == DP_ERR_INVALID);
        dp_cic_get_state (t, after);
        DP_CHECK (memcmp (before, after, sb) == 0);
      }
    memcpy (forged, blob, sb);
    uint32_t phase = 64; /* == R: a live counter wraps at R - 1 */
    memcpy (forged + sb - sizeof (uint8_t) - sizeof (uint32_t), &phase,
            sizeof phase);
    DP_CHECK (dp_cic_set_state (t, forged) == DP_ERR_INVALID);
    dp_cic_get_state (t, after);
    DP_CHECK (memcmp (before, after, sb) == 0);

    free (forged);
    free (before);
    free (after);
    free (blob);
    free (in);
    dp_cic_destroy (t);
    dp_cic_destroy (src);
    dp_cic_destroy (dst);
  }

  test_the_word ();

  DP_TEST_END ("test_cic_core");
}
