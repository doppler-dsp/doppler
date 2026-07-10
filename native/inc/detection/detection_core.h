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

/* ── Non-coherent integration ────────────────────────────────────────────── */
/*                                                                            */
/* Non-coherent integration sums the squared magnitude of n_noncoh coherent  */
/* "looks", each itself an n_coh-sample coherent integration.  The normalized */
/* statistic R = sqrt(sum |z_k|^2 / noise) has, under H0, P(R > b) =          */
/* marcum_q(n_noncoh, 0, b) (order-M central Marcum-Q), and under H1          */
/* P(R > b) = marcum_q(n_noncoh, sqrt(2*n_coh*n_noncoh)*snr, b).  All three    */
/* helpers reduce to their coherent (order-1) counterparts at n_noncoh = 1.   */

/**
 * @brief CFAR threshold eta_nc for a non-coherent detector of n_noncoh looks.
 *
 * Solves marcum_q(n_noncoh, 0, eta_nc) = pfa (the order-M central tail,
 * monotone decreasing in eta_nc) by bisection.  For n_noncoh = 1 this is the
 * exact closed form sqrt(-2 ln pfa) (== det_threshold).
 *
 * @param pfa       Per-test false-alarm probability in (0, 1).
 * @param n_noncoh  Number of non-coherent looks; must be >= 1.
 * @return          Threshold eta_nc on the normalized statistic R.
 *
 * @code
 * >>> from doppler.detection import det_threshold_noncoherent, det_threshold
 * >>> round(det_threshold_noncoherent(pfa=1e-3, n_noncoh=4), 3)
 * 5.111
 * >>> det_threshold_noncoherent(pfa=1e-6, n_noncoh=1) == det_threshold(pfa=1e-6)
 * True
 *
 * @endcode
 */
double det_threshold_noncoherent(double pfa, int n_noncoh);

/**
 * @brief EMA coefficient for a target estimator SNR (DC level in noise).
 *
 * Sizes a first-order EMA `y = (1-alpha)*y + alpha*x` that estimates a DC
 * level from noisy i.i.d. measurements x. Per sample the estimator SNR
 * (mean^2 / variance) is `snr_in`; the EMA improves it by its variance
 * reduction `(2-alpha)/alpha`, so the output SNR is
 * `snr_out = snr_in * (2-alpha)/alpha`. Solving for the coefficient:
 *
 *   alpha = 2 * snr_in / (snr_in + snr_out)      (SNRs linear)
 *
 * Returns 1.0 (no averaging) when snr_out_db <= snr_in_db. Typical inputs:
 * a signal-free power reference |n|^2 is exponential (0 dB per sample); a
 * lock signal at known C/N0 has per-look SNR from its coherent integration
 * (minus squaring loss), and this picks the smoothing bandwidth that makes
 * the lock decision variable meet a chosen decision SNR.
 *
 * @param snr_in_db   Per-sample estimator SNR, dB (mean^2 / variance).
 * @param snr_out_db  Desired EMA-output estimator SNR, dB.
 * @return            EMA coefficient alpha in (0, 1].
 *
 * @code
 * >>> from doppler.detection import det_ema_alpha
 * >>> det_ema_alpha(0.0, 0.0)      # no gain requested -> no averaging
 * 1.0
 * >>> round(1 / det_ema_alpha(0.0, 20.0), 1)   # 20 dB gain ~ 50 looks
 * 50.5
 * >>> round(1 / det_ema_alpha(10.0, 30.0), 1)  # same 20 dB gain, shifted
 * 50.5
 *
 * @endcode
 */
double det_ema_alpha(double snr_in_db, double snr_out_db);

/**
 * @brief Detection probability for n_noncoh non-coherent looks.
 *
 * Computes Pd = Q_{n_noncoh}(a, threshold) with the non-centrality
 * a = sqrt(2 * n_coh * n_noncoh) * snr.  At n_noncoh = 1 this is exactly
 * det_pd(snr, n_coh, threshold); at snr = 0 it returns the per-test Pfa.
 *
 * @param snr        Per-sample amplitude SNR (signal / noise amplitude).
 * @param n_coh      Coherent integration length in samples (dwell * N).
 * @param n_noncoh   Number of non-coherent looks; must be >= 1.
 * @param threshold  Threshold eta_nc, e.g. from det_threshold_noncoherent().
 * @return           Detection probability in &#91;0, 1&#93;.
 *
 * @code
 * >>> from doppler.detection import det_pd_noncoherent, det_pd, det_threshold
 * >>> from doppler.detection import det_threshold_noncoherent
 * >>> eta = det_threshold(pfa=1e-6)
 * >>> det_pd_noncoherent(snr=0.5, n_coh=8, n_noncoh=1, threshold=eta) \
 * ...     == det_pd(snr=0.5, dwell=8, threshold=eta)        # reduces to coherent
 * True
 * >>> eta4 = det_threshold_noncoherent(pfa=1e-3, n_noncoh=4)
 * >>> round(det_pd_noncoherent(snr=0.3, n_coh=16, n_noncoh=4, threshold=eta4), 2)
 * 0.19
 *
 * @endcode
 */
double det_pd_noncoherent(double snr, int n_coh, int n_noncoh,
                          double threshold);

/**
 * @brief Minimum non-coherent looks achieving Pd >= pd_min at fixed n_coh.
 *
 * Iterates n_noncoh = 1, 2, ..., max_n_noncoh, recomputing the threshold
 * (det_threshold_noncoherent, which grows with the look count) at each step.
 * Returns the first look count that meets the Pd requirement, or -1 if none
 * does within max_n_noncoh.  Used by the acquisition engine's (M, N_nc) split.
 *
 * @param snr            Per-sample amplitude SNR (linear).
 * @param n_coh          Coherent integration length in samples (dwell * N).
 * @param pd_min         Required detection probability, e.g. 0.9.
 * @param pfa            Per-test false-alarm probability.
 * @param max_n_noncoh   Search upper bound on the look count.
 * @return               Minimum n_noncoh >= 1, or -1 if not achievable.
 *
 * @code
 * >>> from doppler.detection import det_n_noncoh
 * >>> det_n_noncoh(snr=2.0, n_coh=16, pd_min=0.9, pfa=1e-3, max_n_noncoh=64)
 * 1
 *
 * @endcode
 */
int det_n_noncoh(double snr, int n_coh, double pd_min, double pfa,
                 int max_n_noncoh);

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
