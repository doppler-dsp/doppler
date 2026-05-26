/**
 * rate_converter_demo.c — RateConverter cascade demo.
 *
 * Shows:
 *   1. Stage selection  — cascade chosen for several rate ratios
 *   2. DC gain          — settled RMS after transient (expect ~1.0)
 *   3. Rate change      — set_rate() at runtime; output length changes
 *   4. Split-block      — two halves produce byte-identical output
 *
 * RateConverter picks the cheapest sub-stage cascade (CIC, HalfbandDecimator,
 * polyphase Resampler) for the requested output/input ratio.  The cascade is
 * rebuilt transparently when set_rate() is called, so callers need no
 * knowledge of the internal topology.
 *
 * Build:
 *   make build
 *   ./build/examples/c/rate_converter_demo
 */

#include <RateConverter/RateConverter_core.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double
rms(const float _Complex *buf, size_t n)
{
    double acc = 0.0;
    for (size_t i = 0; i < n; i++)
        acc += (double)crealf(buf[i]) * crealf(buf[i])
             + (double)cimagf(buf[i]) * cimagf(buf[i]);
    return sqrt(acc / (double)n);
}

int
main(void)
{
    printf("=== doppler RateConverter cascade demo ===\n\n");

    /* ------------------------------------------------------------------ *
     * 1. Stage selection                                                   *
     *                                                                     *
     * For each rate ratio, print the number of active cascade stages and  *
     * each stage's label.  Useful for verifying that the selector picks   *
     * the expected topology without running any signal through it.        *
     *                                                                     *
     * Expected patterns (D = 1/rate):                                     *
     *   rate=2.0   D<2     → Resampler(2.0)                               *
     *   rate=0.5   D=2^1   → HalfbandDecimator                            *
     *   rate=0.25  D=2^2   → HalfbandDecimator, HalfbandDecimator         *
     *   rate=0.125 D=2^3   → CIC(8)                                       *
     *   rate=0.1   D≈10    → CIC(8), Resampler(0.8)                       *
     *   rate=1/3   D≈3     → Resampler(0.333…)                            *
     * ------------------------------------------------------------------ */
    printf("--- 1. Stage selection ---\n");
    {
        double rates[] = { 2.0, 0.5, 0.25, 0.125, 0.1, 1.0/3.0 };
        int    n_rates = (int)(sizeof(rates) / sizeof(rates[0]));
        char   label[64];

        printf("  %-10s  %-8s  %s\n", "rate", "n_stages", "stages");
        printf("  %-10s  %-8s  %s\n",
               "----------", "--------",
               "-------------------------------");

        for (int r = 0; r < n_rates; r++) {
            RateConverter_state_t *rc =
                RateConverter_create(rates[r], 0);

            printf("  %-10.6f  %-8d  ", rates[r], rc->n_stages);
            for (int s = 0; s < rc->n_stages; s++) {
                RateConverter_stage_label(rc, s, label, sizeof(label));
                if (s > 0)
                    printf(" → ");
                printf("%s", label);
            }
            printf("\n");

            RateConverter_destroy(rc);
        }
        printf("\n");
    }

    /* ------------------------------------------------------------------ *
     * 2. DC gain                                                           *
     *                                                                     *
     * Feed 65536 samples of DC (1.0+0.0j) through the converter and      *
     * measure the settled output RMS.  Every linear filter in the cascade *
     * passes DC with unity gain, so the settled value should be ≈1.0.    *
     *                                                                     *
     * The first n_drop = (int)(0.05 * n_out) + 4 output samples are      *
     * discarded to skip the combined FIR/CIC transient before measuring. *
     * ------------------------------------------------------------------ */
    printf("--- 2. DC gain (settled RMS, expect ~1.0) ---\n");
    {
        double test_rates[] = { 0.5, 0.125, 0.1 };
        int    n_rates = (int)(sizeof(test_rates) / sizeof(test_rates[0]));
        const size_t n_in = 65536;

        float _Complex *in  = malloc(n_in * sizeof(float _Complex));
        for (size_t i = 0; i < n_in; i++)
            in[i] = 1.0f + 0.0f * _Complex_I;  /* DC */

        printf("  %-10s  %-8s  %s\n", "rate", "n_stages", "settled RMS");
        printf("  %-10s  %-8s  %s\n",
               "----------", "--------", "-----------");

        for (int r = 0; r < n_rates; r++) {
            double rate = test_rates[r];

            RateConverter_state_t *rc =
                RateConverter_create(rate, 0);

            /* Upper-bound on output length for this block size. */
            size_t max_out = (size_t)(n_in * (rate > 1.0 ? rate : 1.0)) + 4;
            float _Complex *out = malloc(max_out * sizeof(float _Complex));

            size_t n_out = RateConverter_execute(
                rc, in, n_in, out, max_out);

            /* Drop transient before measuring. */
            int n_drop = (int)(0.05 * (double)n_out) + 4;
            size_t n_meas = (n_out > (size_t)n_drop)
                            ? n_out - (size_t)n_drop : 0;

            double settled = (n_meas > 0)
                             ? rms(out + n_drop, n_meas) : 0.0;

            printf("  %-10.6f  %-8d  %.6f\n",
                   rate, rc->n_stages, settled);

            free(out);
            RateConverter_destroy(rc);
        }

        free(in);
        printf("\n");
    }

    /* ------------------------------------------------------------------ *
     * 3. Rate change at runtime                                            *
     *                                                                     *
     * Create at rate=0.5 (HalfbandDecimator, 2:1), process a 1024-sample *
     * block, then call set_rate(0.25) to rebuild the cascade to 4:1.     *
     * Process the same block again.  Output length should halve because   *
     * the new cascade decimates by an additional factor of 2.             *
     * ------------------------------------------------------------------ */
    printf("--- 3. Rate change at runtime ---\n");
    {
        const size_t n_in = 1024;

        float _Complex *in  = malloc(n_in * sizeof(float _Complex));
        float _Complex *out = malloc(n_in * sizeof(float _Complex));

        /* Arbitrary tone so the output is non-trivial. */
        for (size_t i = 0; i < n_in; i++)
            in[i] = CMPLXF((float)cos(2*M_PI*0.05*(double)i),
                           (float)sin(2*M_PI*0.05*(double)i));

        RateConverter_state_t *rc = RateConverter_create(0.5, 0);

        size_t n1 = RateConverter_execute(rc, in, n_in, out, n_in);
        printf("  rate=0.50  n_in=%zu  n_out=%zu\n", n_in, n1);

        /* Rebuild cascade to 4:1 and process the same block. */
        RateConverter_set_rate(rc, 0.25);
        size_t n2 = RateConverter_execute(rc, in, n_in, out, n_in);
        printf("  rate=0.25  n_in=%zu  n_out=%zu  (halved: %s)\n\n",
               n_in, n2, (n2 == n1 / 2) ? "yes" : "no");

        RateConverter_destroy(rc);
        free(in); free(out);
    }

    /* ------------------------------------------------------------------ *
     * 4. Split-block state persistence                                     *
     *                                                                     *
     * Processing a 2048-sample block as one call must produce             *
     * byte-identical output to processing it as two consecutive 1024-    *
     * sample calls.  The internal ping-pong buffers carry state across    *
     * calls, so splitting at any boundary is transparent to the caller.  *
     * ------------------------------------------------------------------ */
    printf("--- 4. Split-block state persistence ---\n");
    {
        const size_t n_in  = 2048;
        const size_t n_out = n_in / 2;   /* rate=0.5 halves sample count */

        float _Complex *in        = malloc(n_in  * sizeof(float _Complex));
        float _Complex *out_whole = malloc(n_out * sizeof(float _Complex));
        float _Complex *out_split = malloc(n_out * sizeof(float _Complex));

        memset(out_whole, 0, n_out * sizeof(float _Complex));
        memset(out_split, 0, n_out * sizeof(float _Complex));

        /* 0.03*fs tone — arbitrary, just avoids trivial DC outputs. */
        for (size_t i = 0; i < n_in; i++)
            in[i] = CMPLXF((float)cos(2*M_PI*0.03*(double)i),
                           (float)sin(2*M_PI*0.03*(double)i));

        RateConverter_state_t *whole = RateConverter_create(0.5, 0);
        RateConverter_state_t *split = RateConverter_create(0.5, 0);

        /* Whole: single call with all 2048 input samples. */
        size_t nw = RateConverter_execute(
            whole, in, n_in, out_whole, n_out);

        /* Split: first half then second half, state threads between calls. */
        size_t ns1 = RateConverter_execute(
            split, in,           n_in/2, out_split,           n_out/2);
        size_t ns2 = RateConverter_execute(
            split, in + n_in/2,  n_in/2, out_split + ns1,     n_out/2);
        size_t ns  = ns1 + ns2;

        int ok = (nw == ns)
                 && memcmp(out_whole, out_split,
                           nw * sizeof(float _Complex)) == 0;

        printf("  whole=%zu samples  split=%zu+%zu=%zu samples\n",
               nw, ns1, ns2, ns);
        printf("  byte comparison: %s\n", ok ? "MATCH" : "MISMATCH");

        /* Print a few samples from each run side-by-side. */
        size_t show = (nw < 4) ? nw : 4;
        for (size_t i = 0; i < show; i++)
            printf("  [%zu] whole=(%8.5f,%8.5f)  "
                   "split=(%8.5f,%8.5f)\n", i,
                   (double)crealf(out_whole[i]),
                   (double)cimagf(out_whole[i]),
                   (double)crealf(out_split[i]),
                   (double)cimagf(out_split[i]));

        RateConverter_destroy(whole);
        RateConverter_destroy(split);
        free(in); free(out_whole); free(out_split);
        printf("\n");
    }

    printf("Done.\n");
    return 0;
}
