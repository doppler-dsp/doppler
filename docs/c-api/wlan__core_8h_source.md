

# File wlan\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wlan**](dir_9cf874ec4d1fafc8af25ee841f0634a9.md) **>** [**wlan\_core.h**](wlan__core_8h.md)

[Go to the documentation of this file](wlan__core_8h.md)


```C++

#ifndef DP_WLAN_CORE_H
#define DP_WLAN_CORE_H

#include "doppler/clib_common.h"
#include "doppler/conv/conv_core.h"
#include "doppler/dp_crc.h"
#include "doppler/ofdm/ofdm_core.h"
#include <stddef.h>
#include <stdint.h>
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/pn/pn_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* ── The numbers the design page marks checked (section 9) ─────────────── */

#define WLAN_N_FFT 64u
#define WLAN_N_SD 48u
#define WLAN_N_SP 4u
#define WLAN_GI 16u
#define WLAN_SYM_LEN 80u
#define WLAN_OVERSAMPLE_MAX 128u
#define WLAN_WINDOW_STANDARD UINT32_MAX
#define WLAN_STF_LEN 160u
#define WLAN_LTF_LEN 160u
#define WLAN_SIG_BITS 24u
#define WLAN_SERVICE_BITS 16u
#define WLAN_TAIL_BITS 6u
#define WLAN_PSDU_MAX 4095u
#define WLAN_N_SYM_MAX 1366u
#define WLAN_PILOT_K { -21, -7, 7, 21 }
#define WLAN_PILOT_V { 1.0, 1.0, 1.0, -1.0 }
#define WLAN_POLARITY_LEN 127u

typedef enum
{
  WLAN_CODE_1_2 = 0, 
  WLAN_CODE_2_3 = 1, 
  WLAN_CODE_3_4 = 2, 
  WLAN_CODE_5_6 = 3  
} dp_wlan_code_t;

typedef struct
{
  unsigned       n_bpsc; 
  dp_wlan_code_t code;   
} dp_wlan_rate_t;

typedef struct
{
  dp_wlan_rate_t rate; 
  uint8_t        bits; 
  uint8_t        mbps; 
} dp_wlan_signal_rate_t;

typedef size_t (*dp_wlan_perm2_fn) (size_t i, size_t n_cbps, size_t n_bpsc);

typedef struct
{
  const float _Complex *stf; 
  const float _Complex *ltf; 
  double scale_stf;  
  double scale_ltf;  
  double scale_data; 
  /* The standard tables set all three to 1/sqrt(52): unit mean power over
     the 52 occupied subcarriers, this library's convention (#3148). Annex I
     prints the same math with the inverse transform's own 1/64, so its
     time-domain samples equal this library's times sqrt(52)/64; a table whose
     three scales are 1/64 renders them. One gain covers every field. */
  const dp_wlan_signal_rate_t *signal_rates; 
  size_t                       n_signal_rates; 
  dp_wlan_perm2_fn perm2; 
  const uint16_t *relabel[7];
} dp_wlan_tables_t;

const dp_wlan_tables_t *dp_wlan_tables_empty (void);

const dp_wlan_tables_t *dp_wlan_tables_standard (void);

/* ── The object ──────────────────────────────────────────────────────────── */

typedef struct
{
  uint32_t n_bpsc; 
  uint32_t code;   
  uint32_t n_cbps; 
  uint32_t n_dbps; 
  uint32_t rate_mbps; 
  uint32_t window;     
  uint32_t oversample; 
} dp_wlan_state_t;

dp_wlan_state_t *dp_wlan_create (int constellation, int code_rate,
                                 uint32_t window, uint32_t oversample);

unsigned dp_wlan_n_bpsc (int constellation);

static inline uint32_t
dp_wlan_window_standard (uint32_t oversample)
{
  return 2u * oversample;
}

uint32_t dp_wlan_window_resolve (uint32_t window, uint32_t oversample);

const char *dp_wlan_params_why (int constellation, int code_rate,
                                uint32_t window, uint32_t oversample);

dp_wlan_state_t *dp_wlan_create_why (int constellation, int code_rate,
                                     uint32_t window, uint32_t oversample,
                                     const char **why);

void dp_wlan_destroy (dp_wlan_state_t *state);

void dp_wlan_reset (dp_wlan_state_t *state);

/* ── Rates ───────────────────────────────────────────────────────────────── */

int dp_wlan_rate_ok (const dp_wlan_rate_t *r);

unsigned dp_wlan_n_cbps (const dp_wlan_rate_t *r);

unsigned dp_wlan_n_dbps (const dp_wlan_rate_t *r);

int dp_wlan_mbps (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                  unsigned *mbps);

int dp_wlan_rate_from_mbps (const dp_wlan_tables_t *t, unsigned mbps,
                            dp_wlan_rate_t *r);

size_t dp_wlan_rate_n_sym (const dp_wlan_rate_t *r, size_t psdu_len);

size_t dp_wlan_n_sym (const dp_wlan_state_t *state, size_t psdu_len);

size_t dp_wlan_ppdu_max_out (const dp_wlan_state_t *state, size_t n_in);

int64_t dp_wlan_ppdu_why(dp_wlan_state_t *state, const uint8_t *in, size_t n_in, uint32_t scrambler_state, float _Complex *out, size_t max_out, const char **why);

size_t dp_wlan_ppdu_len (size_t n_sym, unsigned window,
                         unsigned oversample);

int dp_wlan_bcc (dp_wlan_code_t code, conv_code_t *c);

/* ── The scrambler ───────────────────────────────────────────────────────── */

uint8_t dp_wlan_scrambler_state_from_sequence (uint8_t seq7);

size_t dp_wlan_scrambler_sequence (uint8_t state, size_t n, uint8_t *out);

/* ── The minimal MAC ─────────────────────────────────────────────────────── */

typedef enum
{
  WLAN_MAC_NONE = 0, 
  WLAN_MAC_FCS  = 1, 
  WLAN_MAC_DATA = 2  
} dp_wlan_mac_t;

size_t dp_wlan_mpdu_len (dp_wlan_mac_t mode, size_t header_len,
                         size_t body_len);

size_t dp_wlan_mpdu (dp_wlan_mac_t mode, const uint8_t *header,
                     size_t header_len, const uint8_t *body, size_t body_len,
                     uint8_t *out, size_t out_cap);

/* ── The coding chain, stage by stage ────────────────────────────────────── */

size_t dp_wlan_signal_bits (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                            size_t length, uint8_t *out);

size_t dp_wlan_data_bits (const dp_wlan_rate_t *r, const uint8_t *psdu,
                          size_t len, uint8_t state, uint8_t *out, size_t cap);

size_t dp_wlan_encode (dp_wlan_code_t code, const uint8_t *in, size_t n,
                       uint8_t *out, size_t cap);

size_t dp_wlan_interleave (const dp_wlan_tables_t *t, unsigned n_bpsc,
                           const uint8_t *in, size_t n, uint8_t *out);

size_t dp_wlan_map (const dp_wlan_tables_t *t, unsigned n_bpsc,
                    const uint8_t *bits, size_t n, float _Complex *out);

size_t dp_wlan_points (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                       const uint8_t *psdu, size_t len, uint8_t state,
                       float _Complex *out, size_t cap);

/* ── The layout and the PPDU ─────────────────────────────────────────────── */

int dp_wlan_layout (dp_ofdm_state_t *o, const dp_wlan_tables_t *t,
                    size_t n_sym, unsigned window, unsigned oversample);

size_t dp_wlan_ppdu (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                     const uint8_t *psdu, size_t len, uint8_t state,
                     unsigned window, unsigned oversample, float _Complex *out,
                     size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* DP_WLAN_CORE_H */
```


