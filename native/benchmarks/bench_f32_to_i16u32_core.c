#include "f32_to_i16u32/f32_to_i16u32_core.h"
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
    float *in  = malloc(BENCH_N * sizeof(float));
    if (!in) { fprintf(stderr, "OOM\n"); return 1; }
    uint32_t *out = malloc(BENCH_N * sizeof(uint32_t));
    if (!out) { fprintf(stderr, "OOM\n"); return 1; }
    for (int i = 0; i < BENCH_N; i++) in[i] = (float)(i);

    f32_to_i16u32_state_t *obj = f32_to_i16u32_create(32768.0f);

    /* volatile sink prevents DCE of the step() loop */
    volatile uint32_t _sink;

    /* warmup */
    for (int i = 0; i < 16; i++) _sink = f32_to_i16u32_step(obj, in[i]);

    struct timespec t0, t1;
    jm_bench_t _bench = {0};

    printf("=== f32_to_i16u32 benchmark ===\n");
    printf("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

    double _times_step[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
            _sink = f32_to_i16u32_step(obj, in[i]);
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
        f32_to_i16u32_steps(obj, in, out, BENCH_N);
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

    jm_bench_write_json(&_bench, "f32_to_i16u32");
    f32_to_i16u32_destroy(obj);
    free(in);
    free(out);
    return 0;
}
