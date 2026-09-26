

# File detection\_core.h

[**File List**](files.md) **>** [**detection**](dir_c7528e0bd68524c48f260a564c045102.md) **>** [**detection\_core.h**](detection__core_8h.md)

[Go to the documentation of this file](detection__core_8h.md)


```C++

#ifndef DP_DETECTION_CORE_H
#define DP_DETECTION_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

double dp_marcum_q(int m, double a, double b);

double dp_det_threshold(double pfa);

double dp_det_pd(double snr, int dwell, double threshold);

int dp_det_dwell(double snr, double pd_min, double pfa, int max_dwell);

double dp_det_snr(int dwell, double pd_min, double pfa);

/* ── Non-coherent integration ────────────────────────────────────────────── */
/*                                                                            */
/* Non-coherent integration sums the squared magnitude of n_noncoh coherent  */
/* "looks", each itself an n_coh-sample coherent integration.  The normalized */
/* statistic R = sqrt(sum |z_k|^2 / noise) has, under H0, P(R > b) =          */
/* marcum_q(n_noncoh, 0, b) (order-M central Marcum-Q), and under H1          */
/* P(R > b) = marcum_q(n_noncoh, sqrt(2*n_coh*n_noncoh)*snr, b).  All three    */
/* helpers reduce to their coherent (order-1) counterparts at n_noncoh = 1.   */

double dp_det_threshold_noncoherent(double pfa, int n_noncoh);

/* ── Gaussian test statistic ─────────────────────────────────────────────── */
/*                                                                            */
/* The helpers above size the amplitude-ratio detector, whose H0 law is       */
/* Rayleigh. A different family of detectors in this tree threshold a         */
/* statistic that is GAUSSIAN under H0 -- a lock metric block-averaged over   */
/* enough looks for the CLT to hold. Those three (symsync's timing lock,      */
/* dll's code lock, the carrier NDA lock) share one sizing chain, and it      */
/* lives here so they cannot drift apart.                                     */
/*                                                                            */
/* Do NOT reach for dp_det_threshold() on a Gaussian statistic. It inverts       */
/* Pfa = exp(-eta^2/2), the envelope law, and returns 4.9409 where            */
/* dp_det_q_inv() returns 4.4172 at the same pfa = 5e-6 -- two plausible small   */
/* numbers near 5, only one of which is a sigma count.                        */

double dp_det_q_inv(double p);

int dp_det_dwell_gauss(double mean, double var, double pd, double pfa);

double dp_det_threshold_gauss(double mean, double pd, double pfa);

double dp_det_ema_alpha(double snr_in_db, double snr_out_db);

int dp_det_verify_count(double p_look, double p_target);

double dp_det_verify_delay(double p_look, int n);

double dp_det_threshold_f(double pfa, int n);

double dp_det_pd_noncoherent(double snr, int n_coh, int n_noncoh,
                          double threshold);

int dp_det_n_noncoh(double snr, int n_coh, double pd_min, double pfa,
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

double dp_det_threshold_power(double pfa);

double dp_det_pd_power(double snr_power, int dwell, double power_threshold);

int dp_det_dwell_power (double snr_power, double pd_min, double pfa,
                     int max_dwell);

double dp_det_snr_power(int dwell, double pd_min, double pfa);

/* ── Search-level quantities ─────────────────────────────────────────────── */
/* A detector searches many cells, measures its noise from some of them, and */
/* is specified in C/N0. These turn each of those into the per-cell,         */
/* amplitude-SNR terms the functions above take.                             */

double dp_det_pfa_cell(double pfa, double n_cells);

double dp_det_cn0_to_snr(double cn0_dbhz, double fs);

double dp_det_snr_to_cn0(double snr, double fs);

double dp_det_pd_cfar(double snr, int dwell, double threshold, double k,
                   double leak, double leak_cells);

#ifdef __cplusplus
}
#endif

#endif /* DETECTION_CORE_H */
```


