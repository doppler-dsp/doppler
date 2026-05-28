/**
 * standalone/main.c — minimal doppler example.
 *
 * Generates 4096 complex AWGN samples with the one-shot awgn() function,
 * then prints the empirical mean and per-component standard deviation.
 *
 * Build from source tree (doppler built but not installed):
 *   cmake -B build -DBUILD_PYTHON=OFF && cmake --build build -j
 *   cmake -B examples/standalone/build examples/standalone \
 *         -DDOPPLER_BUILD_DIR=$(pwd)/build
 *   cmake --build examples/standalone/build
 *   ./examples/standalone/build/awgn_example
 *
 * Build from installed artifact (after cmake --install):
 *   cmake -B examples/standalone/build examples/standalone
 *   cmake --build examples/standalone/build
 *   ./examples/standalone/build/awgn_example
 *
 * Or with plain gcc against the build tree:
 *   gcc -o awgn_example examples/standalone/main.c \
 *       -Inative/inc -Ibuild/native/inc \
 *       build/libdoppler.a -lm -lstdc++ -lpthread
 */

#include <awgn/awgn_core.h>
#include <clib_common.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define N 4096

int main(void)
{
    float complex out[N];

    /* One-shot: no persistent state needed. */
    if (awgn(/*seed=*/42, /*amplitude=*/1.0f, N, out) != DP_OK) {
        fprintf(stderr, "awgn: allocation failed\n");
        return 1;
    }

    /* Compute mean and per-component standard deviation. */
    double sum_re = 0, sum_im = 0;
    for (int i = 0; i < N; i++) {
        sum_re += crealf(out[i]);
        sum_im += cimagf(out[i]);
    }
    double mean_re = sum_re / N;
    double mean_im = sum_im / N;

    double var_re = 0, var_im = 0;
    for (int i = 0; i < N; i++) {
        double dr = crealf(out[i]) - mean_re;
        double di = cimagf(out[i]) - mean_im;
        var_re += dr * dr;
        var_im += di * di;
    }
    double std_re = sqrt(var_re / N);
    double std_im = sqrt(var_im / N);

    printf("samples : %d\n", N);
    printf("mean    : %.4f + %.4fi  (expect ≈ 0)\n", mean_re, mean_im);
    printf("std dev : %.4f (Re)  %.4f (Im)  (expect ≈ 1.0)\n",
           std_re, std_im);
    return 0;
}
