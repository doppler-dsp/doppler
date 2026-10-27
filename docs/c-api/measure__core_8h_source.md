

# File measure\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**measure**](dir_0caaff9683efc5ec33140a5572570268.md) **>** [**measure\_core.h**](measure__core_8h.md)

[Go to the documentation of this file](measure__core_8h.md)


```C++

#ifndef DP_MEASURE_CORE_H
#define DP_MEASURE_CORE_H

#include "doppler/clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── auto-window design policy ──────────────────────────────────────────────
 * The measurement objects pick their Kaiser window automatically: the user
 * states a target dynamic range (directly, or implied by the ADC bit depth)
 * and the analyser chooses the *minimum* Kaiser beta whose sidelobes sit below
 * that range — so window leakage never caps SFDR/SNR — while keeping the main
 * lobe (hence resolution bandwidth) as narrow as the data allows. */

#define MEASURE_PAD 2u

#define MEASURE_DR_MARGIN_DB 12.0

#define MEASURE_DR_DEFAULT_DB 120.0

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
    double snr;               
    double sinad;             
    double thd;               
    double thd_pct;           
    double thd_n;             
    double sfdr_dbc;          
    double sfdr_dbfs;         
    double enob;              
    double enob_fs;           
    double noise_floor_dbfs;  
    double fund_freq;         
    double fund_dbfs;         
    double worst_spur_freq;   
    double worst_spur_dbc;    
    int    worst_spur_is_harm;
    double rbw_hz;            
    double enbw_hz;           
    double bin_hz;            
    size_t lobe_bins;         
    size_t n_noise_bins;      
    double proc_gain_db;      
    double amp_uncert_db;     
    double floor_uncert_db;   
} tone_meas_t;

typedef struct {
    double rms;          
    double peak;         
    double crest_db;     
    double papr_db;      
    double dc_offset;    
    double fs_util_pct;  
} time_stats_t;

typedef struct {
    double f1;            
    double f2;            
    double p1_dbfs;       
    double p2_dbfs;       
    double imd2_dbc;      
    double imd3_dbc;      
    double imd2_freq;     
    double imd3_lo_freq;  
    double imd3_hi_freq;  
    double toi_dbfs;      
    double soi_dbfs;      
    double rbw_hz;        
} imd_meas_t;

typedef struct {
    double npr_db;            
    double inband_psd_dbfs;   
    double notch_psd_dbfs;    
    size_t n_inband_bins;     
    size_t n_notch_bins;      
    double rbw_hz;            
} npr_meas_t;

/* ── capture-planning helpers ──────────────────────────────────────────────
 * Pure functions that answer "how much data, and at what frequency?" for an
 * IEEE-1241 single-tone test.  See docs/design/measurement-suite.md. */

size_t dp_measure_min_samples(double fs, double target_rbw, size_t bits,
                           double dynamic_range_db, int complex_input);

size_t dp_measure_rec_nfft(size_t n, size_t pad);

double dp_measure_proc_gain(size_t nfft);

double dp_coherent_freq(double fs, double f_target, size_t N);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_CORE_H */
```


