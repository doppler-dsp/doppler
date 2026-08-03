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

/**
 * @brief Default dynamic-range target (dB) implied by an ADC bit depth.
 *
 * Ideal quantisation SNR is 6.02*bits + 1.76 dB; add MEASURE_DR_MARGIN_DB of
 * headroom so the window's first sidelobe sits below the converter's own floor.
 *
 * @param bits  ADC depth in bits (> 0).
 * @return Dynamic-range / sidelobe-attenuation target in dB.
 */
static inline double
measure_dr_from_bits (size_t bits)
{
  return 6.02 * (double)bits + 1.76 + MEASURE_DR_MARGIN_DB;
}

/**
 * @brief Resolve the dynamic-range target from the override/bits/default chain.
 *
 * @param dynamic_range_db  Explicit target (dB); used when > 0.
 * @param bits              ADC depth; used (via measure_dr_from_bits) when the
 *                          override is unset and bits > 0.
 * @return Dynamic-range / sidelobe-attenuation target in dB.
 */
static inline double
measure_resolve_dr (double dynamic_range_db, size_t bits)
{
  if (dynamic_range_db > 0.0)
    return dynamic_range_db;
  if (bits > 0)
    return measure_dr_from_bits (bits);
  return MEASURE_DR_DEFAULT_DB;
}

/**
 * @brief Single-tone dynamic-measurement bag.
 *
 * All ratios (SNR/SINAD/THD/THD+N) are dimensionless dB and independent of the
 * dBFS reference; the absolute `*_dbfs` levels reference a full-scale tone to
 * 0 dBFS (real captures: a peak-`full_scale` sine; complex: a `full_scale`
 * exponential).  Accuracy fields describe the analysis grid that produced them.
 */
typedef struct {
    double snr;               ///< SNR = 10log10(P_fund / P_noise) (dB).
    double sinad;             ///< SINAD = 10log10(fund/(noise+harm)) (dB).
    double thd;               ///< THD = 10log10(P_harm / P_fund) (dBc).
    double thd_pct;           ///< THD = 100 sqrt(P_harm / P_fund) (%).
    double thd_n;             ///< THD+N = 10log10((noise+harm)/fund) = -SINAD.
    double sfdr_dbc;          ///< SFDR: fundamental - worst spur (dBc).
    double sfdr_dbfs;         ///< SFDR: full scale - worst spur (dBFS).
    double enob;              ///< ENOB = (SINAD - 1.76)/6.02.
    double enob_fs;           ///< Full-scale-corrected ENOB.
    double noise_floor_dbfs;  ///< Mean per-bin noise power (dBFS).
    double fund_freq;         ///< Fundamental frequency (Hz).
    double fund_dbfs;         ///< Fundamental level (dBFS).
    double worst_spur_freq;   ///< Worst spur frequency (Hz).
    double worst_spur_dbc;    ///< Worst spur level vs the fundamental (dBc).
    int    worst_spur_is_harm;///< 1 if the worst spur is a harmonic, else 0.
    double rbw_hz;            ///< Resolution bandwidth = enbw*fs/n (Hz).
    double enbw_hz;           ///< Equivalent noise bandwidth (Hz) (= rbw_hz).
    double bin_hz;            ///< FFT bin spacing = fs/nfft (Hz).
    size_t lobe_bins;         ///< Window main-lobe half-width L (bins).
    size_t n_noise_bins;      ///< Number of bins counted as noise.
    double proc_gain_db;      ///< FFT processing gain = 10log10(nfft/2) (dB).
    double amp_uncert_db;     ///< Amplitude-read uncertainty bound (dB).
    double floor_uncert_db;   ///< Noise-floor standard error (dB).
} tone_meas_t;

/**
 * @brief Time-domain capture statistics (AC-coupled crest/PAPR).
 */
typedef struct {
    double rms;          ///< Root-mean-square amplitude (DC included).
    double peak;         ///< Peak deviation, max|x - DC|.
    double crest_db;     ///< Crest factor, 20log10(peak_ac / rms_ac) (dB).
    double papr_db;      ///< Peak-to-average power ratio (= crest) (dB).
    double dc_offset;    ///< DC offset, mean(x).
    double fs_util_pct;  ///< Full-scale use, 100*max|x|/full_scale (%).
} time_stats_t;

/**
 * @brief Two-tone intermodulation result (IMD2/IMD3/TOI).
 */
typedef struct {
    double f1;            ///< Lower tone frequency (Hz).
    double f2;            ///< Upper tone frequency (Hz).
    double p1_dbfs;       ///< Lower tone level (dBFS).
    double p2_dbfs;       ///< Upper tone level (dBFS).
    double imd2_dbc;      ///< 2nd-order product (f2-f1) vs mean tone (dBc).
    double imd3_dbc;      ///< Worst 3rd-order product vs mean tone (dBc).
    double imd2_freq;     ///< 2nd-order product frequency (Hz).
    double imd3_lo_freq;  ///< 3rd-order (2f1-f2) product frequency (Hz).
    double imd3_hi_freq;  ///< 3rd-order (2f2-f1) product frequency (Hz).
    double toi_dbfs;      ///< Third-order intercept (dBFS).
    double soi_dbfs;      ///< Second-order intercept (dBFS).
    double rbw_hz;        ///< Resolution bandwidth (Hz).
} imd_meas_t;

/**
 * @brief Noise Power Ratio (notched-noise loading) result.
 */
typedef struct {
    double npr_db;            ///< NPR = 10log10(in-band PSD / notch PSD) (dB).
    double inband_psd_dbfs;   ///< Mean in-band noise power per bin (dBFS).
    double notch_psd_dbfs;    ///< Mean power folded into the notch (dBFS).
    size_t n_inband_bins;     ///< Bins averaged in the active band.
    size_t n_notch_bins;      ///< Bins averaged inside the notch.
    double rbw_hz;            ///< Resolution bandwidth (Hz).
} npr_meas_t;

/* ── capture-planning helpers ──────────────────────────────────────────────
 * Pure functions that answer "how much data, and at what frequency?" for an
 * IEEE-1241 single-tone test.  See docs/design/measurement-suite.md. */

/**
 * @brief Samples needed to reach a target resolution bandwidth.
 *
 * Plans a capture for the same auto-Kaiser window the measurement objects use:
 * the dynamic-range target (from @p dynamic_range_db, else @p bits) selects the
 * Kaiser beta, whose ENBW (measured via kaiser_enbw) sets the bins-per-RBW.
 * RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).
 *
 * @param fs                Sample rate (Hz, > 0).
 * @param target_rbw        Desired resolution bandwidth (Hz).  When <= 0 it
 *                          defaults to span/1000, where span = fs/2 for real
 *                          captures and fs for complex (@p complex_input).
 * @param bits              ADC depth: sets the dynamic-range target when no
 *                          explicit override is given.
 * @param dynamic_range_db  Explicit dynamic-range target (dB); used when > 0.
 * @param complex_input     Non-zero if the capture is complex (span = fs).
 * @return Required capture length, or 0 on bad args.
 */
size_t measure_min_samples(double fs, double target_rbw, size_t bits,
                           double dynamic_range_db, int complex_input);

/** @brief Recommended zero-padded transform length: next_pow2(n * max(pad,1)). */
size_t measure_rec_nfft(size_t n, size_t pad);

/** @brief FFT processing gain in dB: 10*log10(nfft / 2). */
double measure_proc_gain(size_t nfft);

/**
 * @brief Nearest leakage-free coherent test frequency.
 *
 * Snaps `f_target` to `J * fs / N` where J is the nearest integer cycle count
 * that is coprime with N — an integer number of cycles in the capture (no
 * leakage) with J coprime to N (so quantisation-noise correlation is minimised).
 *
 * @return The coherent frequency (Hz), or 0 on bad args.
 */
double dp_coherent_freq(double fs, double f_target, size_t N);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_CORE_H */
