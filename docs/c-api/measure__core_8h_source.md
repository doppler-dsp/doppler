

# File measure\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**measure**](dir_4f61a452d1df39cf8c2e8be27f29f1f2.md) **>** [**measure\_core.h**](measure__core_8h.md)

[Go to the documentation of this file](measure__core_8h.md)


```C++

#ifndef MEASURE_CORE_H
#define MEASURE_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── auto-window design policy ──────────────────────────────────────────────
 * The measurement objects pick their Kaiser window automatically: the user
 * states a target dynamic range (directly, or implied by the ADC bit depth)
 * and the analyser chooses the *minimum* Kaiser beta whose sidelobes sit below
 * that range — so window leakage never caps SFDR/SNR — while keeping the main
 * lobe (hence resolution bandwidth) as narrow as the data allows. */

/* Internal zero-pad factor (nfft = next_pow2(n * MEASURE_PAD)). */
#define MEASURE_PAD 2u

/* Sidelobe headroom below the ideal converter SNR: a B-bit ADC's spur/noise
 * floor sits at -(6.02*B+1.76) dBc, so target sidelobes this much deeper to be
 * sure window leakage stays under the floor being measured. */
#define MEASURE_DR_MARGIN_DB 12.0

/* Dynamic-range target when neither `bits` nor an explicit override is given
 * (general DUT): deep enough for ~19-bit measurements. */
#define MEASURE_DR_DEFAULT_DB 120.0

/* Extra main-lobe widths excluded past the first null when searching for spurs,
 * so a component's near-in sidelobes are never mistaken for a spur. */
#define MEASURE_SPUR_SIDELOBES 1.0

static inline double
measure_dr_from_bits (size_t bits)
{
  return 6.02 * (double)bits + 1.76 + MEASURE_DR_MARGIN_DB;
}

static inline double
measure_resolve_dr (double dynamic_range_db, size_t bits)
{
  if (dynamic_range_db > 0.0)
    return dynamic_range_db;
  if (bits > 0)
    return measure_dr_from_bits (bits);
  return MEASURE_DR_DEFAULT_DB;
}

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

typedef struct {
    double rms;          /* root-mean-square (DC included)        */
    double peak;         /* peak |x − DC|                         */
    double crest_db;     /* 20log10(peak_ac / rms_ac)             */
    double papr_db;      /* peak-to-average power ratio (= crest) */
    double dc_offset;    /* mean(x)                               */
    double fs_util_pct;  /* 100 * max|x| / full_scale             */
} time_stats_t;

typedef struct {
    double f1;            /* lower tone frequency [Hz]                     */
    double f2;            /* upper tone frequency [Hz]                     */
    double p1_dbfs;       /* lower tone level [dBFS]                       */
    double p2_dbfs;       /* upper tone level [dBFS]                       */
    double imd2_dbc;      /* 2nd-order product (f2-f1) vs mean tone [dBc]  */
    double imd3_dbc;      /* worst 3rd-order product vs mean tone [dBc]    */
    double imd2_freq;     /* 2nd-order product frequency [Hz]              */
    double imd3_lo_freq;  /* 2f1-f2 product frequency [Hz]                 */
    double imd3_hi_freq;  /* 2f2-f1 product frequency [Hz]                 */
    double toi_dbfs;      /* third-order intercept [dBFS]                  */
    double soi_dbfs;      /* second-order intercept [dBFS]                 */
    double rbw_hz;        /* resolution bandwidth [Hz]                     */
} imd_meas_t;

typedef struct {
    double npr_db;            /* 10log10(mean in-band PSD / mean notch PSD)  */
    double inband_psd_dbfs;   /* mean in-band noise power per bin (dBFS)      */
    double notch_psd_dbfs;    /* mean power that folded into the notch (dBFS) */
    size_t n_inband_bins;     /* bins averaged in the active band             */
    size_t n_notch_bins;      /* bins averaged inside the notch               */
    double rbw_hz;            /* resolution bandwidth (Hz)                    */
} npr_meas_t;

/* ── capture-planning helpers ──────────────────────────────────────────────
 * Pure functions that answer "how much data, and at what frequency?" for an
 * IEEE-1241 single-tone test.  See docs/design/measurement-suite.md. */

size_t measure_min_samples(double fs, double target_rbw, size_t bits,
                           double dynamic_range_db, int complex_input);

size_t measure_rec_nfft(size_t n, size_t pad);

double measure_proc_gain(size_t nfft);

double dp_coherent_freq(double fs, double f_target, size_t N);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_CORE_H */
```


