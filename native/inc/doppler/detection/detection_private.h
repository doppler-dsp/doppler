/**
 * @file detection_private.h
 * @brief The detection module's numeric kernels other modules reuse.
 *
 * Not part of the public API. One copy of each kernel, defined in the
 * detection module and called from wherever a statistic needs it (the
 * carrier acquisition's derived threshold), never re-derived there.
 */
#ifndef DP_DETECTION_PRIVATE_H
#define DP_DETECTION_PRIVATE_H

/**
 * @brief The regularized upper incomplete gamma, Q(a, x) = Gamma(a, x) /
 *        Gamma(a), for any real shape a > 0.
 *
 * The survival function of a Gamma(a, 1) variable at x: P(G > x). The
 * series / continued-fraction pair with a log-domain prefactor
 * (marcum_q.c), so it does not underflow where exp(-x) would. dp_marcum_q()
 * calls it at integer shapes.
 *
 * @param a  Shape, > 0.
 * @param x  The point; x <= 0 returns 1.
 * @return Q(a, x), from 0 to 1 inclusive.
 */
double dp__gammaq (double a, double x);

#endif /* DP_DETECTION_PRIVATE_H */
