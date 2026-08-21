/*
 * test_measure_core.c — the measure module's capture-planning helpers.
 *
 * These four answer "how much data, and at what frequency?" for an
 * IEEE-1241 single-tone test. None of them is arithmetic on its arguments:
 * `measure_min_samples` designs a Kaiser window to hit a dynamic-range
 * target and reads its ENBW back, and `dp_coherent_freq` searches for a
 * cycle count coprime with N. Both are cheap to get subtly wrong and
 * expensive to notice, because a wrong plan produces a capture that
 * measures something — just not what was asked for.
 *
 * So the claims here are the ones the docstrings make, stated as
 * relations rather than as golden numbers: an RBW target twice as fine
 * needs about twice the samples, a coherent frequency really does fit a
 * whole number of cycles in the capture, and every bad-argument path
 * returns the documented 0 rather than a plausible-looking value.
 */
#include "dp_test.h"
#include "measure/measure_core.h"
#include <math.h>
#include <stdio.h>

#define FS 1.0e6

/* Greatest common divisor, for the coprimality claim. */
static size_t
gcd_ (size_t a, size_t b)
{
  while (b)
    {
      const size_t t = a % b;
      a              = b;
      b              = t;
    }
  return a;
}

int
main (void)
{
  /* ── measure_min_samples: RBW = ENBW * fs / n, so n scales as 1/RBW ─ */
  {
    const size_t n1k  = measure_min_samples (FS, 1000.0, 12, 0.0, 1);
    const size_t n500 = measure_min_samples (FS, 500.0, 12, 0.0, 1);
    const size_t n250 = measure_min_samples (FS, 250.0, 12, 0.0, 1);

    DP_CHECK (n1k > 0);
    DP_CHECK (n500 > n1k);
    DP_CHECK (n250 > n500);

    /* Halving the RBW at a fixed dynamic range keeps the same window, so
       the same ENBW, so the count should double to within rounding. A
       relation, not a literal -- the ENBW itself is a property of the
       Kaiser design and may legitimately change. */
    DP_CHECK (fabs ((double)n500 / (double)n1k - 2.0) < 0.05);
    DP_CHECK (fabs ((double)n250 / (double)n500 - 2.0) < 0.05);

    /* A deeper ADC asks for more dynamic range, which asks for a wider
       window, which costs samples at the same RBW. */
    DP_CHECK (measure_min_samples (FS, 1000.0, 16, 0.0, 1)
              >= measure_min_samples (FS, 1000.0, 8, 0.0, 1));

    /* An explicit dynamic-range override is used INSTEAD of `bits`, so
       the same override under two different bit depths must agree. */
    DP_CHECK (measure_min_samples (FS, 1000.0, 8, 90.0, 1)
              == measure_min_samples (FS, 1000.0, 16, 90.0, 1));

    /* target_rbw <= 0 defaults to span/1000, and the span differs between
       real and complex -- so the two must NOT come out equal. */
    const size_t def_c = measure_min_samples (FS, 0.0, 12, 0.0, 1);
    const size_t def_r = measure_min_samples (FS, 0.0, 12, 0.0, 0);
    DP_CHECK (def_c > 0 && def_r > 0);
    DP_CHECK (def_c != def_r);

    /* Bad args return 0, per the docstring -- not a plausible length. */
    DP_CHECK (measure_min_samples (0.0, 1000.0, 12, 0.0, 1) == 0);
    DP_CHECK (measure_min_samples (-1.0, 1000.0, 12, 0.0, 1) == 0);
  }

  /* ── measure_rec_nfft: next_pow2(n * max(pad, 1)) ───────────────── */
  {
    DP_CHECK (measure_rec_nfft (1024, 1) == 1024); /* already a power of two */
    DP_CHECK (measure_rec_nfft (1000, 1) == 1024);
    DP_CHECK (measure_rec_nfft (1024, 2) == 2048);
    DP_CHECK (measure_rec_nfft (1000, 4) == 4096);

    /* pad of 0 is treated as 1, which is what max(pad, 1) means. */
    DP_CHECK (measure_rec_nfft (1000, 0) == measure_rec_nfft (1000, 1));

    /* Whatever it returns must BE a power of two and must not lose data. */
    for (size_t n = 1; n <= 5000; n = n * 3 + 1)
      {
        const size_t k = measure_rec_nfft (n, 1);
        DP_CHECK (k >= n);
        DP_CHECK ((k & (k - 1)) == 0);
      }
  }

  /* ── measure_proc_gain: 10*log10(nfft / 2) ──────────────────────── */
  {
    DP_CHECK (fabs (measure_proc_gain (2) - 0.0) < 1e-9);
    DP_CHECK (fabs (measure_proc_gain (2048) - 10.0 * log10 (1024.0)) < 1e-9);

    /* Doubling the transform length buys 3.01 dB, every time. */
    for (size_t n = 64; n <= 65536; n *= 2)
      DP_CHECK (fabs ((measure_proc_gain (2 * n) - measure_proc_gain (n))
                      - 10.0 * log10 (2.0))
                < 1e-9);
  }

  /* ── dp_coherent_freq: an integer, coprime, cycle count ─────────── */
  {
    const size_t N = 4096;

    for (double target = 9000.0; target < 200000.0; target *= 1.7)
      {
        const double f = dp_coherent_freq (FS, target, N);
        DP_CHECK (f > 0.0);

        /* J = f * N / fs must be an INTEGER -- that is what "no leakage"
           means, and it is the whole reason this function exists. */
        const double jf = f * (double)N / FS;
        const double j  = floor (jf + 0.5);
        DP_CHECK (fabs (jf - j) < 1e-9);

        /* ...and coprime with N, so the quantisation noise does not
           correlate with the tone. N is a power of two here, so this is
           the same as J being odd -- checked the general way regardless,
           since N need not be a power of two. */
        DP_CHECK (gcd_ ((size_t)j, N) == 1);

        /* It is the NEAREST such frequency, so it cannot be far: one
           cycle is fs/N, and coprimality can cost at most a couple more. */
        DP_CHECK (fabs (f - target) < 4.0 * FS / (double)N);
      }

    /* Bad args return 0. */
    DP_CHECK (dp_coherent_freq (0.0, 10000.0, N) == 0.0);
    DP_CHECK (dp_coherent_freq (FS, 10000.0, 0) == 0.0);
  }

  DP_TEST_END ("test_measure_core");
}
