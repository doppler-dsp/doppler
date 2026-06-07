#define _POSIX_C_SOURCE 199309L /* clock_gettime / CLOCK_MONOTONIC */
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
run(const char *name, int type, int sps, int pnlen, int lfsr, double snr,
    double freq, long total)
{
    synth_state_t *s =
        synth_create(type, 1e6, freq, snr, 0, 1, sps, pnlen, 0, lfsr);
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
    double msas = (double)done / dt / 1e6;
    printf("  %-26s %8.1f MSa/s   (%.2f GSa/s)\n", name, msas, msas / 1000.0);
}

int
main(void)
{
    long N = 200L * 1000 * 1000; /* 200 M samples per config */
    printf("wfmgen / synth throughput (block=%d, %ld M samples each)\n", BLK,
           N / 1000000);
    printf("  snr 100 = clean (no AWGN); freq 0 = baseband (no LO)\n\n");
    /*    name                       type sps   n  lfsr   snr   freq       */
    run("noise (AWGN)", 1, 8, 7, 0, 20.0, 0.0, N);
    run("tone  clean", 0, 8, 7, 0, 100.0, 1e5, N);
    run("tone  +noise", 0, 8, 7, 0, 20.0, 1e5, N);
    run("pn    baseband clean", 2, 1, 23, 0, 100.0, 0.0, N);
    run("pn    +LO +noise", 2, 1, 23, 0, 20.0, 1e5, N);
    run("pn    n=40 baseband (64b)", 2, 1, 40, 0, 100.0, 0.0, N);
    run("pn    fibonacci baseband", 2, 1, 23, 1, 100.0, 0.0, N);
    run("bpsk  clean", 3, 8, 7, 0, 100.0, 1e5, N);
    run("bpsk  +noise", 3, 8, 7, 0, 20.0, 1e5, N);
    run("qpsk  clean", 4, 8, 7, 0, 100.0, 1e5, N);
    run("qpsk  +noise", 4, 8, 7, 0, 20.0, 1e5, N);
    return 0;
}
