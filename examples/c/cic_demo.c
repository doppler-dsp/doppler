/**
 * cic_demo.c — CIC decimation filter demo.
 *
 * Shows:
 *   1. Alias rejection — two tones (passband + alias zone) measured at output
 *   2. Reconfigure — change R at runtime without reallocation
 *   3. State persistence — split input produces same output as one block
 *
 * CIC filters are computationally cheap (no multiplications) and suit
 * very high decimation ratios (R up to 4096) as a first stage before a
 * polyphase FIR.  Fixed at N=4 stages, M=1.  Passes DC with unity gain;
 * alias rejection at the first null (f = fs/R) is ~77 dB.
 *
 * Build:
 *   make build
 *   ./build/examples/c/cic_demo
 */

#include <cic/cic_core.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double
rms(const float complex *buf, size_t n)
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
    printf("=== doppler CIC decimation filter demo ===\n\n");

    /* ------------------------------------------------------------------ *
     * 1. Alias rejection                                                  *
     *                                                                     *
     * R=8, N=4, M=1: theoretical stopband null at f = fs/R (0.125*fs).   *
     * Feed two simultaneous tones:                                        *
     *   - passband tone at 0.01*fs (well inside output Nyquist 0.0625)    *
     *   - alias-zone tone at 0.11*fs (inside stopband, near first null)   *
     * Then measure output RMS for each tone in isolation.                 *
     * ------------------------------------------------------------------ */
    printf("--- 1. Alias rejection (R=8, N=%d, M=1) ---\n", CIC_N);
    {
        const uint32_t R = 8;
        /* Give enough samples for the transient (CIC_N*(R-1) inputs) to settle
         * and to get a reliable RMS estimate. */
        const size_t n_in  = 24 * R * CIC_N;
        const size_t n_out = n_in / R;

        double f_pass  = 0.01;            /* well inside output Nyquist */
        double f_alias = 1.0 / R * 0.95; /* just inside first CIC null */

        float complex *in_pass  = malloc(n_in  * sizeof(float complex));
        float complex *in_alias = malloc(n_in  * sizeof(float complex));
        float complex *out      = malloc(n_out * sizeof(float complex));

        for (size_t i = 0; i < n_in; i++) {
            in_pass[i]  = CMPLXF((float)cos(2*M_PI*f_pass *i),
                                 (float)sin(2*M_PI*f_pass *i));
            in_alias[i] = CMPLXF((float)cos(2*M_PI*f_alias*i),
                                 (float)sin(2*M_PI*f_alias*i));
        }

        cic_state_t *cic = cic_create(R);

        /* Drop the first n_drop outputs (filter transient). */
        size_t n_drop = CIC_N * (R - 1) / R + 1;
        size_t n_meas = n_out - n_drop;

        size_t n = cic_decimate(cic, in_pass, n_in, out);
        double rms_pass = rms(out + n_drop, n_meas);

        cic_reset(cic);
        n = cic_decimate(cic, in_alias, n_in, out);
        double rms_alias = rms(out + n_drop, n_meas);

        double rejection_db = 20.0 * log10(rms_pass / (rms_alias + 1e-300));

        printf("  passband tone  (f=%.3f*fs)  RMS = %.4f\n",
               f_pass,  rms_pass);
        printf("  alias-zone tone(f=%.3f*fs)  RMS = %.4f\n",
               f_alias, rms_alias);
        printf("  alias rejection: %.1f dB\n\n", rejection_db);

        (void)n;
        cic_destroy(cic);
        free(in_pass); free(in_alias); free(out);
    }

    /* ------------------------------------------------------------------ *
     * 2. Runtime reconfigure — change R without reallocation              *
     *                                                                     *
     * cic_reconfigure() resets state and updates R and shift in place.   *
     * Useful in scanning receivers that switch decimation on-the-fly.     *
     * ------------------------------------------------------------------ */
    printf("--- 2. Runtime reconfigure ---\n");
    {
        cic_state_t *cic = cic_create(4);
        printf("  initial:      R=%u  shift=%u\n", cic->R, cic->shift);

        cic_reconfigure(cic, 32);
        printf("  after reconf: R=%u  shift=%u\n", cic->R, cic->shift);

        /* Invalid args are silently ignored — existing config preserved. */
        cic_reconfigure(cic, 0);
        printf("  after R=0:    R=%u (unchanged — invalid arg ignored)\n\n",
               cic->R);

        cic_destroy(cic);
    }

    /* ------------------------------------------------------------------ *
     * 3. State persistence across block boundaries                        *
     *                                                                     *
     * Splitting a block at an arbitrary boundary must produce             *
     * byte-identical output to processing it whole.                       *
     * ------------------------------------------------------------------ */
    printf("--- 3. State persistence (split vs whole block) ---\n");
    {
        const uint32_t R = 8;
        const size_t   n_in = 4 * R;   /* two full decimation cycles per half */

        float complex in[32], out_whole[4], out_split[4];
        memset(out_whole, 0, sizeof(out_whole));
        memset(out_split, 0, sizeof(out_split));

        for (size_t i = 0; i < n_in; i++)
            in[i] = CMPLXF((float)cos(2*M_PI*0.03*i),
                           (float)sin(2*M_PI*0.03*i));

        cic_state_t *whole = cic_create(R);
        cic_state_t *split = cic_create(R);

        /* Whole: one call of 4R samples → 4 outputs. */
        cic_decimate(whole, in, n_in, out_whole);

        /* Split: two calls of 2R samples each → 2 outputs per call. */
        cic_decimate(split, in,          2*R, out_split);
        cic_decimate(split, in + 2*R,    2*R, out_split + 2);

        int ok = memcmp(out_whole, out_split, sizeof(out_whole)) == 0;
        printf("  whole vs split output: %s\n", ok ? "MATCH" : "MISMATCH");
        for (size_t i = 0; i < 4; i++)
            printf("  [%zu] whole=(%7.4f,%7.4f)  split=(%7.4f,%7.4f)\n", i,
                   (double)crealf(out_whole[i]), (double)cimagf(out_whole[i]),
                   (double)crealf(out_split[i]), (double)cimagf(out_split[i]));

        cic_destroy(whole);
        cic_destroy(split);
        printf("\n");
    }

    printf("Done.\n");
    return 0;
}
