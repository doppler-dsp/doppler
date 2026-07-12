

# File detection\_core.h

[**File List**](files.md) **>** [**detection**](dir_3a1e0e8c534208cc3745b2f53a028862.md) **>** [**detection\_core.h**](detection__core_8h.md)

[Go to the documentation of this file](detection__core_8h.md)


```C++

#ifndef DETECTION_CORE_H
#define DETECTION_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

double marcum_q(int m, double a, double b);

double det_threshold(double pfa);

double det_pd(double snr, int dwell, double threshold);

int det_dwell(double snr, double pd_min, double pfa, int max_dwell);

double det_snr(int dwell, double pd_min, double pfa);

/* ── Non-coherent integration ────────────────────────────────────────────── */
/*                                                                            */
/* Non-coherent integration sums the squared magnitude of n_noncoh coherent  */
/* "looks", each itself an n_coh-sample coherent integration.  The normalized */
/* statistic R = sqrt(sum |z_k|^2 / noise) has, under H0, P(R > b) =          */
/* marcum_q(n_noncoh, 0, b) (order-M central Marcum-Q), and under H1          */
/* P(R > b) = marcum_q(n_noncoh, sqrt(2*n_coh*n_noncoh)*snr, b).  All three    */
/* helpers reduce to their coherent (order-1) counterparts at n_noncoh = 1.   */

double det_threshold_noncoherent(double pfa, int n_noncoh);

double det_ema_alpha(double snr_in_db, double snr_out_db);

int det_verify_count(double p_look, double p_target);

double det_verify_delay(double p_look, int n);

double det_threshold_f(double pfa, int n);

double det_pd_noncoherent(double snr, int n_coh, int n_noncoh,
                          double threshold);

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

double det_threshold_power(double pfa);

double det_pd_power(double snr_power, int dwell, double power_threshold);

int det_dwell_power (double snr_power, double pd_min, double pfa,
                     int max_dwell);

double det_snr_power(int dwell, double pd_min, double pfa);

int det_dwell_power(double snr_power, double pd_min, double pfa, int max_dwell);
#ifdef __cplusplus
}
#endif

#endif /* DETECTION_CORE_H */
```


