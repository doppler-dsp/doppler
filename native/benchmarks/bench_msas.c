#include <complex.h>
#include <stdio.h>
#include <time.h>

#include "synth/synth_core.h"

#define BLK 65536

static double
now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static float complex buf[BLK];

static void
run(const char *name, int type, int sps, int pnlen, int lfsr, long total)
{
    synth_state_t *s =
        synth_create(type, 1e6, 0.1, 20.0, 0, 1, sps, pnlen, 0, lfsr);
    if (!s) {
        printf("  %-26s   (create failed)\n", name);
        return;
    }
    synth_steps(s, buf, BLK); /* warm */
    long done = 0;
    double t0 = now();
    while (done < total) {
        synth_steps(s, buf, BLK);
        done += BLK;
    }
    double dt = now() - t0;
    synth_destroy(s);
    printf("  %-26s %8.1f MSa/s   (%.0f M samples in %.2fs)\n", name,
           (double)done / dt / 1e6, (double)done / 1e6, dt);
}

int
main(void)
{
    long N = 200L * 1000 * 1000; /* 200 M samples per config */
    printf("wfmgen / synth engine throughput (block=%d, %ld M samples each)\n\n",
           BLK, N / 1000000);
    run("tone", 0, 8, 7, 0, N);
    run("noise", 1, 8, 7, 0, N);
    run("pn  galois  n=23", 2, 1, 23, 0, N);
    run("pn  fibonacci n=23", 2, 1, 23, 1, N);
    run("pn  galois  n=40 (64b)", 2, 1, 40, 0, N);
    run("bpsk  sps=8", 3, 8, 7, 0, N);
    run("qpsk  sps=8", 4, 8, 7, 0, N);
    return 0;
}
