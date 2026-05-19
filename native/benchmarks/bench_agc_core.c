#include "agc/agc_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N    65536
#define ITERATIONS 200

static double
elapsed_sec(struct timespec *t0, struct timespec *t1)
{
    return (double)(t1->tv_sec - t0->tv_sec)
           + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main(void)
{
    float complex *in  = malloc(BENCH_N * sizeof(float complex));
    if (!in) { fprintf(stderr, "OOM\n"); return 1; }
    float complex *out = malloc(BENCH_N * sizeof(float complex));
    if (!out) { fprintf(stderr, "OOM\n"); return 1; }
    /* Constant-envelope input: a ramp would push the loop through
       near-zero power and denormal arithmetic, skewing the timing. */
    for (int i = 0; i < BENCH_N; i++) in[i] = 0.6f + 0.8f * I;

    agc_state_t *obj = agc_create(0.0, 0.0025, 0.05);

    /* volatile sink prevents DCE of the step() loop */
    volatile float complex _sink;

    /* warmup */
    for (int i = 0; i < 16; i++) _sink = agc_step(obj, in[i]);

    struct timespec t0, t1;
    jm_bench_t _bench = {0};

    printf("=== agc benchmark ===\n");
    printf("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

    double _times_step[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
            _sink = agc_step(obj, in[i]);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        _times_step[r] = elapsed_sec(&t0, &t1);
    }
    jm_bench_add(&_bench, "step", _times_step, ITERATIONS, BENCH_N);
    {
        double _s = 0.0;
        for (int r = 0; r < ITERATIONS; r++) _s += _times_step[r];
        printf("  step()   %8.1f MSa/s\n",
               (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
    double _times_steps[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        agc_steps(obj, in, out, BENCH_N);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        _times_steps[r] = elapsed_sec(&t0, &t1);
    }
    jm_bench_add(&_bench, "steps", _times_steps, ITERATIONS, BENCH_N);
    {
        double _s = 0.0;
        for (int r = 0; r < ITERATIONS; r++) _s += _times_steps[r];
        printf("  steps()  %8.1f MSa/s\n",
               (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }

    jm_bench_write_json(&_bench, "agc");
    agc_destroy(obj);
    free(in);
    free(out);
    return 0;
}
