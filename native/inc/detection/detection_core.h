/**
 * @file detection_core.h
 * @brief Detection-theory utilities for the amplitude-ratio test statistic.
 *
 * The doppler detector forms the test statistic:
 *
 *   test_stat = peak_mag / noise_est
 *
 * With M-point coherent integration (dwell = M) and per-sample amplitude
 * SNR `snr` (signal amplitude / noise amplitude, linear):
 *
 *   Under H0 (noise only):   test_stat ~ Rayleigh(1)
 *   Under H1 (signal+noise): test_stat ~ Rice(a, 1),
 *                            a = sqrt(2*M) * snr
 *
 * False-alarm probability (threshold-only, M-independent):
 *
 *   Pfa = exp(-eta^2/2)  =>  eta = sqrt(-2 ln Pfa)     (exact)
 *
 * Detection probability:
 *
 *   Pd = Q_1(a, eta)    (Marcum Q function, order 1)
 *
 * All functions are stateless and thread-safe.
 */
#ifndef DETECTION_CORE_H
#define DETECTION_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Marcum Q function Q_M(a, b) for integer M >= 1.
 *
 * Probability that a Rice(a, sigma=1) random variable exceeds b.
 * For M=1: Q_1(a, b) = P(Rice(a,1) > b).  General integer M relates
 * to the noncentral chi-squared CDF with 2M degrees of freedom.
 *
 * Computed via the Poisson-weighted chi-squared series (exact for M=1,
 * converges in ~60 terms for practical a, b <= 15):
 *
 *   Q_M(a, b) = sum_{k=0}^inf  w_k * Q_{M+k}(0, b)
 *
 * where:
 *   w_k       = exp(-u) * u^k/k!                    (u = a^2/2)
 *   Q_n(0,b)  = exp(-v) * sum_{j=0}^{n-1} v^j/j!   (v = b^2/2)
 *
 * Each iteration advances both the Poisson weight and the chi-sum in O(1)
 * using the recurrences w_{k+1} = w_k * u/(k+1) and
 * Q_{n+1}(0,b) = Q_n(0,b) + exp(-v)*v^n/n!.
 * Total cost: O(K) where K ~ max(u, M) + safety margin.
 *
 * Special cases:
 *   - a = 0:   Q_M(0, b) = exp(-b^2/2) * sum_{j=0}^{M-1} (b^2/2)^j/j!
 *   - b <= 0:  Q_M(a, b) = 1.0
 *
 * @param m  Integration order; must be >= 1.
 * @param a  Non-centrality parameter (signal strength).  a = 0 for H0.
 * @param b  Threshold (same units as test_stat).
 * @return   Q_M(a, b) in &#91;0, 1&#93;.
 *
 * @code
 * >>> from doppler.detection import marcum_q
 * >>> round(marcum_q(m=1, a=0.0, b=1.0), 5)   # P(Rayleigh > 1) = exp(-0.5)
 * 0.60653
 * >>> round(marcum_q(m=1, a=0.0, b=2.0), 5)   # exp(-2)
 * 0.13534
 * >>> round(marcum_q(m=2, a=0.0, b=2.0), 5)   # 3*exp(-2)
 * 0.40601
 * >>> round(marcum_q(m=1, a=2.0, b=1.0), 5)   # signal present (a=2)
 * 0.91811
 *
 * @endcode
 */
double marcum_q(int m, double a, double b);

/**
 * @brief Threshold eta for a given false-alarm probability.
 *
 * Exact closed-form inversion of Pfa = exp(-eta^2/2):
 *
 *   eta = sqrt(-2 * ln(pfa))
 *
 * The threshold is independent of dwell and SNR; it depends only on the
 * desired Pfa.
 *
 * @param pfa  Desired false-alarm probability; must be in (0, 1).
 * @return     Threshold eta > 0.
 *
 * @code
 * >>> from doppler.detection import det_threshold
 * >>> round(det_threshold(pfa=1e-6), 4)
 * 5.2565
 *
 * @endcode
 */
double det_threshold(double pfa);

/**
 * @brief Detection probability for given per-sample amplitude SNR and dwell.
 *
 * Computes Pd = Q_1(a, eta) where a = sqrt(2 * dwell) * snr.
 *
 * At snr = 0, det_pd returns Pfa (the false-alarm rate, as expected for a
 * noise-only input).  As snr or dwell increase, Pd approaches 1.
 *
 * @param snr        Per-sample amplitude SNR (signal / noise amplitude,
 *                   linear).  snr = 0 gives Pd = Pfa.
 * @param dwell      Coherent integration depth; must be >= 1.
 * @param threshold  Test-stat threshold eta, e.g. from det_threshold().
 * @return           Detection probability in &#91;0, 1&#93;.
 *
 * @code
 * >>> from doppler.detection import det_pd, det_threshold
 * >>> thr = det_threshold(pfa=1e-6)
 * >>> round(det_pd(snr=1.613, dwell=8, threshold=thr), 2)  # 8-dwell -> Pd~0.9
 * 0.9
 * >>> round(det_pd(snr=0.0, dwell=8, threshold=thr), 6)    # snr=0 -> Pd=Pfa
 * 1e-06
 *
 * @endcode
 */
double det_pd(double snr, int dwell, double threshold);

/**
 * @brief Minimum dwell such that Pd >= pd_min for the given SNR and Pfa.
 *
 * Iterates dwell = 1, 2, ..., max_dwell, computing det_pd() at each step.
 * Returns the first dwell that satisfies the Pd requirement, or -1 if none
 * is found within max_dwell iterations.
 *
 * @param snr        Per-sample amplitude SNR (linear).
 * @param pd_min     Required detection probability, e.g. 0.9.
 * @param pfa        False-alarm probability; used to derive eta.
 * @param max_dwell  Search upper bound; prevents infinite loops for low SNR.
 * @return           Minimum dwell >= 1, or -1 if not achievable.
 *
 * @code
 * >>> from doppler.detection import det_dwell
 * >>> det_dwell(snr=0.5, pd_min=0.9, pfa=1e-6, max_dwell=256)
 * 84
 *
 * @endcode
 */
int det_dwell(double snr, double pd_min, double pfa, int max_dwell);

/**
 * @brief Minimum per-sample amplitude SNR achieving Pd >= pd_min.
 *
 * Binary search over SNR in &#91;0, hi&#93; where hi is doubled from 1.0 until
 * det_pd(hi, dwell, threshold) >= pd_min.  64 bisection iterations yield
 * ~1e-19 relative precision on the final interval.
 *
 * @param dwell   Coherent integration depth; must be >= 1.
 * @param pd_min  Required detection probability.
 * @param pfa     False-alarm probability; used to derive eta.
 * @return        Minimum amplitude SNR >= 0.
 *
 * @code
 * >>> from doppler.detection import det_snr, det_pd, det_threshold
 * >>> snr = det_snr(dwell=8, pd_min=0.9, pfa=1e-6)
 * >>> round(snr, 3)
 * 1.613
 * >>> det_pd(snr=snr, dwell=8, threshold=det_threshold(pfa=1e-6)) >= 0.9
 * True
 *
 * @endcode
 */
double det_snr(int dwell, double pd_min, double pfa);

/* ── Power detector ──────────────────────────────────────────────────────── */
/*                                                                            */
/* The power detector uses power_stat = |R[0]|² / mean(|R[τ]|²) instead of  */
/* the envelope ratio.  Under H0 (noise only):                               */
/*                                                                            */
/*   power_stat ~ Exponential(1)   →   P(power_stat > p) = exp(-p)          */
/*                                                                            */
/* The threshold is simply p = -ln(Pfa), and the detection probability is    */
/*                                                                            */
/*   Pd = Q_1(sqrt(2·dwell·snr_power), sqrt(2·p))                           */
/*                                                                            */
/* where snr_power = snr_amplitude^2 (signal power / noise power per sample  */
/* at the correlator output).                                                 */
/*                                                                            */
/* Since sqrt(2·p) = sqrt(-2·ln(Pfa)) = det_threshold(Pfa), the Pd formula  */
/* is identical to the envelope case expressed in power units.  The two      */
/* detectors are equivalent in performance (same Pd at the same SNR in dB); */
/* the power detector offers a simpler threshold formula and an exponential  */
/* null distribution.                                                         */

/**
 * @brief Power threshold p from Pfa for the power detector.
 *
 * Exact closed-form: P(Exponential(1) > p) = exp(-p) = Pfa, so
 *
 *   p = -ln(Pfa)
 *
 * @param pfa  Desired false-alarm probability; must be in (0, 1).
 * @return     Threshold p > 0.
 *
 * @code
 * >>> from doppler.detection import det_threshold_power
 * >>> round(det_threshold_power(pfa=1e-6), 3)   # -ln(1e-6) = 6*ln(10)
 * 13.816
 *
 * @endcode
 */
double det_threshold_power(double pfa);

/**
 * @brief Detection probability for the power detector.
 *
 * Pd = Q_1(sqrt(2·dwell·snr_power), sqrt(2·power_threshold))
 *
 * @param snr_power       Per-sample power SNR (signal power / noise power at
 *                        the correlator output, linear).  0 gives Pd = Pfa.
 * @param dwell           Coherent integration depth; must be >= 1.
 * @param power_threshold Threshold p, e.g. from det_threshold_power().
 * @return                Detection probability in &#91;0, 1&#93;.
 *
 * @code
 * >>> from doppler.detection import det_pd_power, det_threshold_power
 * >>> thr = det_threshold_power(pfa=1e-6)
 * >>> round(det_pd_power(snr_power=2.6017, dwell=8, power_threshold=thr), 2)
 * 0.9
 *
 * @endcode
 * The result equals det_pd() at the equivalent amplitude SNR: power SNR
 * `s` corresponds to amplitude SNR `sqrt(s)`, and the Q_1 arguments match.
 */
double det_pd_power(double snr_power, int dwell, double power_threshold);

/**
 * @brief Minimum dwell such that Pd >= pd_min for the power detector.
 *
 * @param snr_power  Per-sample power SNR (linear).
 * @param pd_min     Required detection probability.
 * @param pfa        False-alarm probability; used to derive p.
 * @param max_dwell  Search upper bound.
 * @return           Minimum dwell >= 1, or -1 if not achievable.
 *
 * @code
 * >>> from doppler.detection import det_dwell_power
 * >>> det_dwell_power(snr_power=0.25, pd_min=0.9, pfa=1e-6, max_dwell=256)
 * 84
 *
 * @endcode
 */
int det_dwell_power (double snr_power, double pd_min, double pfa,
                     int max_dwell);

/**
 * @brief Minimum per-sample power SNR achieving Pd >= pd_min.
 *
 * @param dwell   Coherent integration depth; must be >= 1.
 * @param pd_min  Required detection probability.
 * @param pfa     False-alarm probability.
 * @return        Minimum power SNR >= 0.
 *
 * @code
 * >>> from doppler.detection import (det_snr_power, det_pd_power,
 * ...                                det_threshold_power)
 * >>> sp = det_snr_power(dwell=8, pd_min=0.9, pfa=1e-6)
 * >>> round(sp, 4)
 * 2.6017
 * >>> det_pd_power(snr_power=sp, dwell=8,
 * ...              power_threshold=det_threshold_power(pfa=1e-6)) >= 0.9
 * True
 *
 * @endcode
 */
double det_snr_power(int dwell, double pd_min, double pfa);

int det_dwell_power(double snr_power, double pd_min, double pfa, int max_dwell);
#ifdef __cplusplus
}
#endif

#endif /* DETECTION_CORE_H */
