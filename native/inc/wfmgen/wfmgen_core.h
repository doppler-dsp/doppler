/**
 * @file wfmgen_core.h
 * @brief Wfmgen module — public C API.
 */
#ifndef WFMGEN_CORE_H
#define WFMGEN_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare module-level functions here. */

void bpsk_map(const uint8_t *bits, size_t bits_len, float complex *out);
void qpsk_map(const uint8_t *syms, size_t syms_len, float complex *out);
float wfm_awgn_amplitude(float snr_db, float signal_power);
float wfm_ebno_to_snr_db(float ebno_db, int bits_per_symbol, float samples_per_symbol);
#ifdef __cplusplus
}
#endif

#endif /* WFMGEN_CORE_H */
