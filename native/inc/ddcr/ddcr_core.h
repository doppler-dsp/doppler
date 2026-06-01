/**
 * @file ddcr_core.h
 * @brief DDCR (real-input DDC) — re-exports from ddc_core.h.
 *
 * All state and lifecycle declarations live in ddc/ddc_core.h.
 * This header exists so jm-managed ddc_ext_ddcr.c can include a
 * component-specific path while still resolving against the shared
 * ddcr_state_t defined alongside ddc_state_t.
 */
#ifndef DDCR_CORE_H
#define DDCR_CORE_H

#include "ddc/ddc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the maximum output samples for one execute call.
 *
 * Returns 0, which signals the Python extension to fall back to
 * allocating n_in samples — always sufficient for a decimating DDC.
 */
size_t ddcr_execute_max_out (ddcr_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* DDCR_CORE_H */
