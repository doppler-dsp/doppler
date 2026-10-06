#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_complex.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* SPEC.md's geometry: 3.069 Mcps at spc=2, a 2.5 GHz carrier, and the +/-50
   kHz uncertainty expressed as what it physically is — 20 ppm of the time
   base. */
#define T_FS 6.138e6
#define T_FC 2.5e9
#define T_PPM 20.0
#define T_RATE 0.2 /* ppm/s == 500 Hz/s at 2.5 GHz */
#define T_N 65536u

/* Dominant frequency of a block, by peak of a naive DFT evaluated only near
   the expected bin — enough to confirm the offset without pulling in an FFT
   dependency for one test. */
static double
_peak_hz (const float _Complex *y, size_t n, double fs, double lo, double hi,
          double step)
{
  double best = 0.0, best_mag = -1.0;
  for (double f = lo; f <= hi; f += step)
    {
      double sr = 0.0, si = 0.0;
      double w = -2.0 * 3.14159265358979323846 * f / fs;
      for (size_t k = 0; k < n; k++)
        {
          double ph = w * (double)k;
          sr += crealf (y[k]) * cos (ph) - cimagf (y[k]) * sin (ph);
          si += crealf (y[k]) * sin (ph) + cimagf (y[k]) * cos (ph);
        }
      double mag = sr * sr + si * si;
      if (mag > best_mag)
        {
          best_mag = mag;
          best     = f;
        }
    }
  return best;
}

int
main (void)
{

  float _Complex *x = malloc (T_N * sizeof *x);
  DP_CHECK (x != NULL);
  if (!x)
    return 1;
  for (size_t i = 0; i < T_N; i++)
    x[i]
        = 1.0f + 0.0f * I; /* DC — any offset in the output is the channel's */

  /* ---- 1. carrier offset is fc * d ------------------------------------ */
  {
    dp_doppler_channel_state_t *ch
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
    DP_CHECK (ch != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (ch);
    float _Complex *y   = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    size_t n = dp_doppler_channel_execute (ch, x, T_N, y, cap);
    DP_CHECK (n > 0);

    /* +/-2 kHz around the expected 50 kHz, 50 Hz resolution. */
    double f = _peak_hz (y, n < 4096 ? n : 4096, T_FS, 48000.0, 52000.0, 50.0);
    DP_CHECK (dp_nearf (f, T_FC * T_PPM * 1e-6, 200.0f));
    DP_CHECK (dp_nearf (dp_doppler_channel_get_offset_hz (ch), 50000.0, 1.0f));

    /* ---- 2. the time base dilates: n_out ~= n_in / (1 + d) --------- */
    double expect = (double)T_N / (1.0 + T_PPM * 1e-6);
    DP_CHECK (fabs ((double)n - expect) <= 2.0);

    free (y);
    dp_doppler_channel_destroy (ch);
  }

  /* ---- 3. d = 0 is a pass-through in rate and carrier alike ----------- */
  {
    dp_doppler_channel_state_t *ch
        = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    DP_CHECK (ch != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (ch);
    float _Complex *y   = malloc (cap * sizeof *y);
    size_t          n   = dp_doppler_channel_execute (ch, x, T_N, y, cap);
    DP_CHECK (n == T_N);
    DP_CHECK (dp_nearf (dp_doppler_channel_get_offset_hz (ch), 0.0, 1e-9f));
    free (y);
    dp_doppler_channel_destroy (ch);
  }

  /* ---- 4. the ramp is the INTEGRAL, not t*d(t) ------------------------ */
  /* The distinguishing test: offset(t) must be fc*d_dot*t, NOT twice that.
     A t*d(t) implementation passes every static-Doppler check above and
     fails only here, which is exactly why this case exists. */
  {
    dp_doppler_channel_state_t *ch
        = dp_doppler_channel_create (T_FS, T_FC, 0.0, T_RATE);
    DP_CHECK (ch != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (ch);
    float _Complex *y   = malloc (cap * sizeof *y);
    for (int b = 0; b < 16; b++)
      (void)dp_doppler_channel_execute (ch, x, T_N, y, cap);
    double t = dp_doppler_channel_get_elapsed_s (ch);
    DP_CHECK (t > 0.0);
    DP_CHECK (dp_nearf (dp_doppler_channel_get_offset_hz (ch),
                        T_FC * T_RATE * 1e-6 * t, 0.01f));
    free (y);
    dp_doppler_channel_destroy (ch);
  }

  /* ---- 5. blockwise == one big call (chunk invariance) ---------------- */
  {
    dp_doppler_channel_state_t *a
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    dp_doppler_channel_state_t *b
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    DP_CHECK (a != NULL && b != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (a);
    float _Complex *ya  = malloc (cap * sizeof *ya);
    float _Complex *yb  = malloc (cap * sizeof *yb);
    size_t          na  = dp_doppler_channel_execute (a, x, T_N, ya, cap);

    size_t nb = 0;
    for (size_t off = 0; off < T_N; off += 4096)
      nb += dp_doppler_channel_execute (b, x + off, 4096, yb + nb, cap - nb);

    DP_CHECK (na == nb);
    int same = 1;
    for (size_t k = 0; k < (na < nb ? na : nb); k++)
      if (!dp_cnearf (ya[k], yb[k], 1e-4f))
        {
          same = 0;
          break;
        }
    DP_CHECK (same);
    free (ya);
    free (yb);
    dp_doppler_channel_destroy (a);
    dp_doppler_channel_destroy (b);
  }

  /* ---- 6. mid-stream resume is bit-exact ------------------------------ */
  {
    dp_doppler_channel_state_t *a
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    dp_doppler_channel_state_t *b
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    DP_CHECK (a != NULL && b != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (a);
    float _Complex *ya  = malloc (cap * sizeof *ya);
    float _Complex *yb  = malloc (cap * sizeof *yb);

    /* Run `a` through one block, hand its state to `b`, then run both
       over an identical second block: the outputs must agree exactly. */
    (void)dp_doppler_channel_execute (a, x, 8192, ya, cap);
    size_t cb   = dp_doppler_channel_state_bytes (a);
    void  *blob = malloc (cb);
    DP_CHECK (blob != NULL);
    dp_doppler_channel_get_state (a, blob);
    DP_CHECK (dp_doppler_channel_set_state (b, blob) == DP_OK);

    size_t na = dp_doppler_channel_execute (a, x, 8192, ya, cap);
    size_t nb = dp_doppler_channel_execute (b, x, 8192, yb, cap);
    DP_CHECK (na == nb);
    int same = 1;
    for (size_t k = 0; k < (na < nb ? na : nb); k++)
      if (ya[k] != yb[k])
        {
          same = 0;
          break;
        }
    DP_CHECK (same);

    free (blob);
    free (ya);
    free (yb);
    dp_doppler_channel_destroy (a);
    dp_doppler_channel_destroy (b);
  }

  /* ---- 7. the standard round-trip + envelope reject ------------------- */
  {
    dp_doppler_channel_state_t *a
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    dp_doppler_channel_state_t *b
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    DP_CHECK (a != NULL && b != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (a);
    float _Complex *y   = malloc (cap * sizeof *y);
    (void)dp_doppler_channel_execute (a, x, 4096, y, cap);
    DP_STATE_ROUNDTRIP_TEST (dp_doppler_channel, a, b);
    free (y);
    dp_doppler_channel_destroy (a);
    dp_doppler_channel_destroy (b);
  }

  /* ---- 8. invalid configuration is rejected, not silently accepted ---- */
  DP_CHECK (dp_doppler_channel_create (0.0, T_FC, 0.0, 0.0) == NULL);
  DP_CHECK (dp_doppler_channel_create (-1.0, T_FC, 0.0, 0.0) == NULL);
  /* d <= -1 (scale <= 0) would stop or reverse time. Use d = -2 (well inside
   * the rejected region) rather than the exact d = -1 boundary: 1 +
   * (-1e6)*1e-6 is not representable as exactly 0, so it lands at +/-1e-17
   * depending on the platform's FP evaluation (rejected on x86, accepted on
   * arm64/macOS) -- testing the unrepresentable boundary is inherently
   * non-portable. */
  DP_CHECK (dp_doppler_channel_create (T_FS, T_FC, -2e6, 0.0) == NULL);

  /* ---- 9. reset returns both clocks to zero --------------------------- */
  {
    dp_doppler_channel_state_t *ch
        = dp_doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
    DP_CHECK (ch != NULL);
    size_t          cap = dp_doppler_channel_execute_max_out (ch);
    float _Complex *y   = malloc (cap * sizeof *y);
    (void)dp_doppler_channel_execute (ch, x, T_N, y, cap);
    DP_CHECK (dp_doppler_channel_get_elapsed_s (ch) > 0.0);
    dp_doppler_channel_reset (ch);
    DP_CHECK (dp_nearf (dp_doppler_channel_get_elapsed_s (ch), 0.0, 1e-12f));
    free (y);
    dp_doppler_channel_destroy (ch);
  }

  /* ---- 10. the output is delayed by delay_samples, on top of the ---- *
   *          dilation: out[k] carries in[k + excess*fs - delay]            */
  /* A random +/-1 sequence (the shipped bits) through the channel; the lag
     of the cross-correlation peak between output and input, refined by a
     parabola through the peak and its neighbours, against the closed form
     the header states. Two points: no Doppler (lag = delay, 10.5 -- the
     peak splits equally over 10 and 11, so the vertex is exact by
     symmetry), and 20 ppm at the receive time where the dilation has
     bought exactly half a sample (lag = delay - 0.5 = 10.0, symmetric
     again). A delay off by the pipeline's one sample, or a truth line
     with the dilation's sign wrong, misses by 1.0 and 1.0. */
  {
    const size_t    N = 65536, B = 2048;
    float _Complex *seq = malloc (N * sizeof *seq);
    DP_CHECK (seq != NULL);
    uint32_t st = 0x5EEDu;
    for (size_t i = 0; i < N; i++)
      seq[i] = (float)dp_bit (&st) + 0.0f * I;
    const double ppms[2] = { 0.0, T_PPM };
    for (int c = 0; c < 2; c++)
      {
        /* Carrier 0: the pure time-dilation configuration the header names
           for isolating a code loop -- a carrier on the output would
           scramble a real cross-correlation. */
        dp_doppler_channel_state_t *ch
            = dp_doppler_channel_create (T_FS, 0.0, ppms[c], 0.0);
        DP_CHECK (ch != NULL);
        double D = dp_doppler_channel_get_delay_samples (ch);
        DP_CHECK (D > 1.0);
        size_t          cap = dp_doppler_channel_execute_max_out (ch);
        float _Complex *y   = malloc (cap * sizeof *y);
        size_t          n   = dp_doppler_channel_execute (ch, seq, N, y, cap);
        /* The block centre: where the dilation has bought half a sample
           (k = 0.5 / (ppm*1e-6)), or the stream's middle without one. */
        size_t k0 = c ? (size_t)(0.5 / (T_PPM * 1e-6)) : N / 2;
        DP_CHECK (k0 + B + 32 < n);
        double t    = ((double)k0 + 0.5 * B) / T_FS;
        double e    = doppler_channel_excess (ch, t) * T_FS;
        double want = D - e; /* out[k] ~ in[k - want] */
        /* r(L) = sum out[k] in[k - L] over the block, L around want. */
        double r[32];
        int    L0 = (int)floor (want) - 8, best = 0;
        for (int i = 0; i < 32; i++)
          {
            double acc = 0.0;
            for (size_t k = k0; k < k0 + B; k++)
              acc += crealf (y[k]) * crealf (seq[k - (size_t)(L0 + i)]);
            r[i] = acc;
            if (r[i] > r[best])
              best = i;
          }
        DP_CHECK (best > 0 && best < 31);
        double a = r[best - 1], b = r[best], cc = r[best + 1];
        double vertex
            = (double)(L0 + best) + 0.5 * (a - cc) / (a - 2.0 * b + cc);
        printf ("  delay: %.0f ppm, lag %.3f samples, closed form %.3f\n",
                ppms[c], vertex, want);
        DP_CHECK (fabs (vertex - want) < 0.1);
        free (y);
        dp_doppler_channel_destroy (ch);
      }
    free (seq);
  }

  /* ---- 8. profile mode: the carrier is the resampler's, so chunking and
          the closed form both hold ------------------------------------- */
  {
    /* More than one internal block (DOPPLER_CHANNEL_MAX_BLOCK) so the
       reverted first attempt's per-block chord would have been re-cut. */
    size_t          n   = 3u * DOPPLER_CHANNEL_MAX_BLOCK / 2u;
    float _Complex *xs  = malloc (n * sizeof *xs);
    double         *ppm = malloc (n * sizeof *ppm);
    DP_CHECK (xs && ppm);
    for (size_t i = 0; i < n; i++)
      {
        xs[i]  = 1.0f + 0.0f * I;
        ppm[i] = 25.0 * cos (3.14159265358979323846 * (double)i / (double)n);
      }

    dp_doppler_channel_state_t *whole_ch
        = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    size_t          cap   = 2u * n + 2u;
    float _Complex *whole = malloc (cap * sizeof *whole);
    DP_CHECK (whole_ch && whole);
    size_t nw = dp_doppler_channel_execute_profile (whole_ch, xs, n, ppm, n,
                                                    whole, cap);
    DP_CHECK (nw > 0);

    /* 8a. Chunk-independence, bit for bit, at block sizes that do not divide
       each other. This is the assertion the first attempt failed (2.75e-2). */
    static const size_t blocks[] = { 1000u, 7777u, 10000u, 50000u };
    for (size_t b = 0; b < sizeof blocks / sizeof *blocks; b++)
      {
        dp_doppler_channel_state_t *ch
            = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
        float _Complex *y = malloc (cap * sizeof *y);
        DP_CHECK (ch && y);
        size_t ny = 0;
        for (size_t off = 0; off < n; off += blocks[b])
          {
            size_t m = (n - off < blocks[b]) ? n - off : blocks[b];
            ny += dp_doppler_channel_execute_profile (
                ch, xs + off, m, ppm + off, m, y + ny, cap - ny);
          }
        DP_CHECK (ny == nw);
        int same = 1;
        for (size_t i = 0; i < nw && i < ny; i++)
          if (crealf (y[i]) != crealf (whole[i])
              || cimagf (y[i]) != cimagf (whole[i]))
            same = 0;
        DP_CHECK (same);
        free (y);
        dp_doppler_channel_destroy (ch);
      }

    /* 8b. A flat profile is the scalar route to the rate quantum: the
       resampler steps in 2^-32 of an input interval, so the carrier read off
       it differs from the ideal closed form by at most fc*2^-32 Hz, i.e. a
       phase of at most 2*pi*fc*2^-32*T. */
    {
      size_t          nf   = 200000u;
      size_t          fcap = 2u * nf + 2u;
      float _Complex *ones = malloc (nf * sizeof *ones);
      double         *flat = malloc (nf * sizeof *flat);
      float _Complex *ya   = malloc (fcap * sizeof *ya);
      float _Complex *yb   = malloc (fcap * sizeof *yb);
      DP_CHECK (ones && flat && ya && yb);
      for (size_t i = 0; i < nf; i++)
        {
          ones[i] = 1.0f + 0.0f * I;
          flat[i] = T_PPM;
        }
      dp_doppler_channel_state_t *sc
          = dp_doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
      dp_doppler_channel_state_t *pr
          = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
      DP_CHECK (sc && pr);
      /* the scalar form takes at most a MAX_BLOCK block per call */
      size_t na = 0;
      for (size_t off = 0; off < nf; off += DOPPLER_CHANNEL_MAX_BLOCK)
        {
          size_t m = (nf - off < DOPPLER_CHANNEL_MAX_BLOCK)
                         ? nf - off
                         : DOPPLER_CHANNEL_MAX_BLOCK;
          na += dp_doppler_channel_execute (sc, ones + off, m, ya + na,
                                            fcap - na);
        }
      size_t nb = dp_doppler_channel_execute_profile (pr, ones, nf, flat, nf,
                                                      yb, fcap);
      DP_CHECK (na == nb);
      double bound = 2.0 * 3.14159265358979323846 * T_FC
                     * 2.3283064365386963e-10 * ((double)nb / T_FS);
      double worst = 0.0;
      for (size_t i = 200; i < nb && i < na; i++)
        {
          double d = cargf (ya[i] * conjf (yb[i]));
          if (fabs (d) > worst)
            worst = fabs (d);
        }
      printf ("  flat profile vs scalar: worst phase %.4f rad, bound %.4f\n",
              worst, bound);
      DP_CHECK (worst <= bound);
      free (ones);
      free (flat);
      free (ya);
      free (yb);
      dp_doppler_channel_destroy (sc);
      dp_doppler_channel_destroy (pr);
    }

    /* 8c. Invalid calls write nothing: length mismatch, a time-reversing
       sample LAST (checked before any output), NULL pointers. */
    {
      dp_doppler_channel_state_t *ch
          = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
      float _Complex *y = malloc (cap * sizeof *y);
      DP_CHECK (ch && y);
      DP_CHECK (
          dp_doppler_channel_execute_profile (ch, xs, 1000, ppm, 999, y, cap)
          == 0);
      double saved = ppm[999];
      ppm[999]     = -2.0e6;
      DP_CHECK (
          dp_doppler_channel_execute_profile (ch, xs, 1000, ppm, 1000, y, cap)
          == 0);
      ppm[999] = saved;
      DP_CHECK (dp_doppler_channel_execute_profile (ch, NULL, 1000, ppm, 1000,
                                                    y, cap)
                == 0);
      DP_CHECK (
          dp_doppler_channel_execute_profile (ch, xs, 1000, NULL, 1000, y, cap)
          == 0);
      DP_CHECK (dp_doppler_channel_get_elapsed_s (ch) == 0.0);
      free (y);
      dp_doppler_channel_destroy (ch);
    }

    /* 8c2. The validation and the sizing are ONE number (PR #1857 review).
       execute_profile_max_out() holds a 2x expansion, so a scale under 1/2
       would ask for more outputs than the buffer has and the kernel would
       stop short -- silently, after advancing the input clock. It is refused
       instead. Just inside the floor every output is produced and none is
       lost to the cap; non-finite samples are refused too (+inf passes a
       "> 0" test and used to yield a single output with no error). The
       boundary itself is not tested: 1 + (-5e5)*1e-6 is not exactly 0.5. */
    {
      const size_t                m = 1000u;
      dp_doppler_channel_state_t *ch
          = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
      DP_CHECK (ch != NULL);
      size_t bcap        = dp_doppler_channel_execute_profile_max_out (ch, m);
      float _Complex *y  = malloc (bcap * sizeof *y);
      double         *fp = malloc (m * sizeof *fp);
      DP_CHECK (y && fp);
      for (size_t i = 0; i < m; i++)
        fp[i] = -4.9e5; /* scale 0.51: a 1.96x expansion */
      size_t ni
          = dp_doppler_channel_execute_profile (ch, xs, m, fp, m, y, bcap);
      DP_CHECK (ni < bcap); /* the cap was not what ended the call */
      DP_CHECK (fabs ((double)ni - (double)m / 0.51) <= 2.0);

      dp_doppler_channel_reset (ch);
      fp[500] = -5.5e5; /* scale 0.45: past the sizing */
      DP_CHECK (dp_doppler_channel_execute_profile (ch, xs, m, fp, m, y, bcap)
                == 0);
      DP_CHECK (dp_doppler_channel_get_elapsed_s (ch) == 0.0);

      const double bad[] = { NAN, INFINITY, -INFINITY };
      for (size_t b = 0; b < sizeof bad / sizeof bad[0]; b++)
        {
          fp[500] = -4.9e5;
          fp[m - 1u]
              = bad[b]; /* last, so only a whole-profile check sees it */
          DP_CHECK (
              dp_doppler_channel_execute_profile (ch, xs, m, fp, m, y, bcap)
              == 0);
          DP_CHECK (dp_doppler_channel_get_elapsed_s (ch) == 0.0);
        }
      free (fp);
      free (y);
      dp_doppler_channel_destroy (ch);
    }

    /* 8d. A mid-stream split resumes bit-exact through the blob, and the
       restored stream still reports the profile's last d. */
    {
      size_t                      half = n / 2u;
      dp_doppler_channel_state_t *a
          = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
      float _Complex *y1 = malloc (cap * sizeof *y1);
      float _Complex *y2 = malloc (cap * sizeof *y2);
      DP_CHECK (a && y1 && y2);
      size_t n1 = dp_doppler_channel_execute_profile (a, xs, half, ppm, half,
                                                      y1, cap);
      size_t bytes = dp_doppler_channel_state_bytes (a);
      void  *blob  = malloc (bytes);
      DP_CHECK (blob != NULL);
      dp_doppler_channel_get_state (a, blob);
      dp_doppler_channel_state_t *b
          = dp_doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
      DP_CHECK (b && dp_doppler_channel_set_state (b, blob) == DP_OK);
      DP_CHECK (dp_doppler_channel_get_offset_hz (b)
                == dp_doppler_channel_get_offset_hz (a));
      DP_CHECK (dp_doppler_channel_get_offset_hz (b) != 0.0);
      size_t n2 = dp_doppler_channel_execute_profile (
          b, xs + half, n - half, ppm + half, n - half, y2, cap);
      DP_CHECK (n1 + n2 == nw);
      int same = 1;
      for (size_t i = 0; i < n1; i++)
        if (crealf (y1[i]) != crealf (whole[i])
            || cimagf (y1[i]) != cimagf (whole[i]))
          same = 0;
      for (size_t i = 0; i < n2; i++)
        if (crealf (y2[i]) != crealf (whole[n1 + i])
            || cimagf (y2[i]) != cimagf (whole[n1 + i]))
          same = 0;
      DP_CHECK (same);
      /* envelope reject: clobber the magic. */
      ((unsigned char *)blob)[0] ^= 0xFFu;
      DP_CHECK (dp_doppler_channel_set_state (b, blob) == DP_ERR_INVALID);
      free (blob);
      free (y1);
      free (y2);
      dp_doppler_channel_destroy (a);
      dp_doppler_channel_destroy (b);
    }

    free (whole);
    dp_doppler_channel_destroy (whole_ch);
    free (xs);
    free (ppm);
  }

  free (x);
  DP_TEST_END ("test_doppler_channel_core");
}
