/**
 * @file ddcr_core.c
 * @brief DDCR buffer-sizing hook for the Python extension.
 *
 * All ddcr_* lifecycle functions (create/destroy/reset/execute/get/set)
 * are implemented in native/src/ddc/ddc_core.c.  This file provides
 * only ddcr_execute_max_out, the hook used by the jm-generated Python
 * extension to pre-size the output buffer.
 *
 * Returns 0 to trigger the dynamic n_in fallback in ddc_ext_ddcr.c —
 * n_in samples is always sufficient capacity for a decimating DDC.
 */
#include "ddcr/ddcr_core.h"

size_t
ddcr_execute_max_out (ddcr_state_t *state)
{
  (void)state;
  return 0;
}
