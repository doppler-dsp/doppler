#include "RateConverter/RateConverter_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N    65536
#define ITERATIONS 100

static double
elapsed_sec(struct timespec *t0, struct timespec *t1)
{
    return (double)(t1->tv_sec - t0->tv_sec)
           + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

typedef struct {
    const char *label;
    double      rate;
} bench_regime_t;

static const bench_regime_t REGIMES[] = {
    { "HB(0.5)",         0.5         },
    { "HB2(0.25)",       0.25        },
    { "CIC(0.125)",      0.125       },
    { "CIC+Rs(0.1)",     0.1         },
    { "Resamp(1/3)",     1.0 / 3.0   },
    { "Interp(2.0)",     2.0         },
};
#define N_REGIMES (int)(sizeof(REGIMES) / sizeof(REGIMES[0]))

int
main(void)
{
    float complex *in  = malloc(BENCH_N * sizeof(float complex));
    if (!in) { fprintf(stderr, "OOM\n"); return 1; }
    /* Worst-case output: interpolation doubles length */
    float complex *out = malloc(BENCH_N * 2 * sizeof(float complex));
    if (!out) { fprintf(stderr, "OOM\n"); free(in); return 1; }
    for (int i = 0; i < BENCH_N; i++)
        in[i] = (float)(i % 128) / 128.0f + 0.0f * I;

    printf("=== RateConverter benchmark ===\n");
    printf("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

    jm_bench_t _bench = {0};
    double _times[ITERATIONS];

    for (int ri = 0; ri < N_REGIMES; ri++) {
        double rate        = REGIMES[ri].rate;
        const char *label  = REGIMES[ri].label;
        size_t max_out     = BENCH_N * 2;

        RateConverter_state_t *rc = RateConverter_create(rate, 0);
        if (!rc) {
            fprintf(stderr, "create failed for rate=%.4f\n", rate);
            continue;
        }

        /* warm-up */
        for (int w = 0; w < 4; w++)
            RateConverter_execute(rc, in, BENCH_N, out, max_out);
        RateConverter_reset(rc);

        for (int r = 0; r < ITERATIONS; r++) {
            struct timespec t0, t1;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            RateConverter_execute(rc, in, BENCH_N, out, max_out);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            _times[r] = elapsed_sec(&t0, &t1);
        }
        jm_bench_add(&_bench, label, _times, ITERATIONS, BENCH_N);

        {
            double s = 0.0;
            for (int r = 0; r < ITERATIONS; r++) s += _times[r];
            printf("  %-20s %8.1f MSa/s\n",
                   label, (double)BENCH_N / (s / ITERATIONS) / 1e6);
        }

        RateConverter_destroy(rc);
    }

    jm_bench_write_json(&_bench, "RateConverter");
    free(in);
    free(out);
    return 0;
}
