

# File ber\_core.h

[**File List**](files.md) **>** [**ber**](dir_b6e9705448f5ec813187161d6664687c.md) **>** [**ber\_core.h**](ber__core_8h.md)

[Go to the documentation of this file](ber__core_8h.md)


```C++

#ifndef BER_CORE_H
#define BER_CORE_H

#include "dp_state.h"
#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BER_TARGET_SER 1e-3

#define BER_TARGET_ERRORS 200u

#define BER_CONF 0.99

#define BER_LAG_SPAN 200

#define BER_SYNC_SYMS 256u

#define BER_SYNC_PFA 1e-6

#define BER_MAX_LAGS 2048

  /* ── records ──────────────────────────────────────────────────────────── */

  typedef struct
  {
    double p_hat;   
    double lo;      
    double hi;      
    double rel;     
    double conf;    
    size_t errors;  
    size_t symbols; 
  } ber_interval_t;

  typedef struct
  {
    int    lag;         
    double phase;       
    double stat;        
    double threshold;   
    double margin_db;   
    double runner_db;   
    size_t occurrences; 
    size_t slips;       
    int    saturated;   
    int    ok;          
  } ber_align_t;

  /* ── free functions: theory and windows ───────────────────────────────── */

  double ber_qfunc (double x);

  double ber_theory_ser (int m, double esn0);

  double ber_theory_ber (int m, double esn0);

  double ber_esn0_db_for_ser (int m, double ser);

  double ber_evm_scatter_floor_db (int m);

  size_t ber_settle_syms (double bn_timing, double bn_carrier);

  int ber_lock_symbol (const uint8_t *flags, size_t flags_len, size_t sustain,
                        double min_frac);

  double ber_evm_db (const float _Complex *rx, size_t rx_len, size_t lo,
                     size_t hi, int m);

  size_t ber_settle_from (size_t budget, int timing_lock, int carrier_lock);

  ber_interval_t ber_confidence (size_t errors, size_t symbols, double conf);

#ifdef __cplusplus
}
#endif

#endif /* BER_CORE_H */
```


