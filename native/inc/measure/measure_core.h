/**
 * @file measure_core.h
 * @brief Measure module — shared result structs and module-level helpers.
 *
 * The `doppler.measure` objects (ToneMeasure, …) each own a window + FFT and
 * analyse a time-domain capture, returning one of these plain-C result bags by
 * out-parameter.  Every spectral metric integrates a component's power over its
 * window MAIN LOBE (IEEE Std 1241) rather than reading a single peak bin, so the
 * reading is independent of where the tone falls between FFT bins.
 */
#ifndef MEASURE_CORE_H
#define MEASURE_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Single-tone dynamic-measurement bag.
 *
 * All ratios (SNR/SINAD/THD/THD+N) are dimensionless dB and independent of the
 * dBFS reference; the absolute `*_dbfs` levels reference a full-scale tone to
 * 0 dBFS (real captures: a peak-`full_scale` sine; complex: a `full_scale`
 * exponential).  Accuracy fields describe the analysis grid that produced them.
 */
typedef struct {
    double snr;               /* 10log10(P_fund / P_noise)                  */
    double sinad;             /* 10log10(P_fund / (P_noise + P_harm))       */
    double thd;               /* 10log10(P_harm / P_fund)  [dBc]            */
    double thd_pct;           /* 100*sqrt(P_harm / P_fund) [%]             */
    double thd_n;             /* 10log10((P_noise+P_harm)/P_fund) = -SINAD  */
    double sfdr_dbc;          /* fundamental − worst spur [dBc]            */
    double sfdr_dbfs;         /* full scale − worst spur  [dBFS]           */
    double enob;              /* (SINAD − 1.76)/6.02                        */
    double enob_fs;           /* full-scale-corrected ENOB                  */
    double noise_floor_dbfs;  /* mean per-bin noise power, dBFS             */
    double fund_freq;         /* fundamental frequency [Hz]                */
    double fund_dbfs;         /* fundamental level [dBFS]                  */
    double worst_spur_freq;   /* worst spur frequency [Hz]                 */
    double worst_spur_dbc;    /* worst spur level relative to fund [dBc]    */
    int    worst_spur_is_harm;/* 1 if the worst spur is a harmonic          */
    double rbw_hz;            /* resolution bandwidth = enbw*fs/n [Hz]      */
    double enbw_hz;           /* equivalent noise bandwidth [Hz] (= rbw_hz) */
    double bin_hz;            /* FFT bin spacing = fs/nfft [Hz]             */
    size_t lobe_bins;         /* window main-lobe half-width L [bins]       */
    size_t n_noise_bins;      /* bins counted as noise                      */
    double proc_gain_db;      /* FFT processing gain = 10log10(nfft/2)      */
    double amp_uncert_db;     /* amplitude-read uncertainty bound [dB]      */
    double floor_uncert_db;   /* noise-floor standard error [dB]            */
} tone_meas_t;

/**
 * @brief Time-domain capture statistics (AC-coupled crest/PAPR).
 */
typedef struct {
    double rms;          /* root-mean-square (DC included)        */
    double peak;         /* peak |x − DC|                         */
    double crest_db;     /* 20log10(peak_ac / rms_ac)             */
    double papr_db;      /* peak-to-average power ratio (= crest) */
    double dc_offset;    /* mean(x)                               */
    double fs_util_pct;  /* 100 * max|x| / full_scale             */
} time_stats_t;

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_CORE_H */
