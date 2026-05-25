/*
 * resample_core.c — Module-level design helpers for the resample module.
 *
 * These scalar functions are exposed to Python via resample_ext.c.
 * The polyphase engine lives in resamp_core.c (linked as resamp_core OBJECT).
 */
#include "resample/resample_core.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Kaiser window beta from stopband attenuation.
 *
 * Classic Parks-McClellan / Kaiser empirical formula.
 *
 * @param atten  Stopband attenuation in dB (positive).
 * @return beta parameter for scipy.signal.kaiser or build_bank.
 */
double
kaiser_beta (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

/**
 * @brief Taps-per-phase from Kaiser design spec.
 *
 * Computes the prototype filter length using the Kaiser order estimate,
 * then divides by num_phases to get taps per polyphase branch.
 *
 * @param num_phases  Number of polyphase branches (must be >= 1).
 * @param atten       Stopband attenuation in dB.
 * @param pb          Normalised passband edge (0 < pb < sb < 0.5).
 * @param sb          Normalised stopband edge.
 * @return Taps per phase (>= 1).
 */
int
kaiser_num_taps (int num_phases, double atten, double pb, double sb)
{
  double pb_ph = pb / (double)num_phases;
  double sb_ph = sb / (double)num_phases;
  double tw = sb_ph - pb_ph; /* transition width per phase */
  size_t proto = (size_t)(1.0 + (atten - 8.0) / (2.285 * (2.0 * M_PI * tw)));
  size_t halflen = proto / 2;
  size_t htaps = 2 * halflen + 1;
  return (int)(htaps / (size_t)num_phases + 1);
}

/* Bernoulli numbers B_{2k} for k = 1 ... 9 */
static const double _berno[9] = {
    1.0 / 6.0,       -1.0 / 30.0,     1.0 / 42.0,
    -1.0 / 30.0,     5.0 / 66.0,      -691.0 / 2730.0,
    7.0 / 6.0,       -3617.0 / 510.0, 43867.0 / 798.0,
};

/*
 * Gaussian elimination with partial pivoting.
 * Solves A*x = b in-place; result left in b.  n <= 9.
 */
static void
_solve(double *A, double *b, int n)
{
    for (int col = 0; col < n; col++) {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(A[r * n + col]) > fabs(A[piv * n + col]))
                piv = r;
        if (piv != col) {
            for (int k = 0; k < n; k++) {
                double t = A[col * n + k];
                A[col * n + k] = A[piv * n + k];
                A[piv * n + k] = t;
            }
            double t = b[col]; b[col] = b[piv]; b[piv] = t;
        }
        for (int r = col + 1; r < n; r++) {
            double f = A[r * n + col] / A[col * n + col];
            for (int k = col; k < n; k++)
                A[r * n + k] -= f * A[col * n + k];
            b[r] -= f * b[col];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        for (int j = i + 1; j < n; j++)
            b[i] -= A[i * n + j] * b[j];
        b[i] /= A[i * n + i];
    }
}

void
ciccompmf(double *h, uint32_t N, uint32_t R, uint32_t M)
{
    double A[9 * 9], b[9], a[9];

    uint32_t max_M = (M % 2 != 0) ? 19u : 18u;
    if (M < 1 || M > max_M) { memset(h, 0, M * sizeof(double)); return; }

    if (M % 2 != 0) {                    /* -- odd M -- */
        uint32_t half = (M - 1) / 2;
        if (half == 0) { h[0] = 1.0; return; }

        for (uint32_t i = 0; i < half; i++) {
            double sgn = (i % 2 == 0) ? -1.0 : 1.0;
            for (uint32_t j = 0; j < half; j++)
                A[i * half + j] =
                    2.0 * sgn * pow((double)(j + 1), 2.0 * (i + 1));
        }

        memset(b, 0, half * sizeof(double));
        for (uint32_t u = 1; u <= half; u++) {
            for (uint32_t q = 1; q <= u; q++) {
                uint32_t idx = u - q + 1;
                double bval = fabs(_berno[idx - 1]);
                double term = (double)N * bval / (2.0 * idx)
                              * (1.0 - pow((double)R, -2.0 * (double)idx));
                b[u - 1] += (double)(2 * q - 1) * pow(term, (double)q);
            }
        }

        memcpy(a, b, half * sizeof(double));
        double Atmp[9 * 9];
        memcpy(Atmp, A, half * half * sizeof(double));
        _solve(Atmp, a, (int)half);

        double a0 = 1.0;
        for (uint32_t i = 0; i < half; i++) a0 -= 2.0 * a[i];
        for (uint32_t i = 0; i < half; i++) {
            h[i]         = a[half - 1 - i];
            h[M - 1 - i] = a[half - 1 - i];
        }
        h[half] = a0;

    } else {                             /* -- even M -- */
        uint32_t half = M / 2;
        if (half == 0) return;
        if (half > 9)  return;

        for (uint32_t i = 0; i < half; i++) {
            double sgn = (i % 2 == 0) ? 1.0 : -1.0;
            for (uint32_t j = 0; j < half; j++)
                A[i * half + j] =
                    2.0 * sgn * pow((double)j + 0.5, 2.0 * i);
        }

        b[0] = 1.0;
        memset(b + 1, 0, (half - 1) * sizeof(double));
        for (uint32_t u = 2; u <= half; u++) {
            for (uint32_t q = 1; q < u; q++) {
                uint32_t idx = u - q;
                double bval = fabs(_berno[idx - 1]);
                double term = (double)N * bval / (2.0 * idx)
                              * (1.0 - pow((double)R, -2.0 * (double)idx));
                b[u - 1] += (double)(2 * q - 1) * pow(term, (double)q);
            }
        }

        memcpy(a, b, half * sizeof(double));
        double Atmp[9 * 9];
        memcpy(Atmp, A, half * half * sizeof(double));
        _solve(Atmp, a, (int)half);

        for (uint32_t i = 0; i < half; i++) {
            h[i]         = a[half - 1 - i];
            h[M - 1 - i] = a[half - 1 - i];
        }
    }
}
