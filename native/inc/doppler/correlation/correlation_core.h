/**
 * @file correlation_core.h
 * @brief Correlation module — public C API.
 */
#ifndef DP_CORRELATION_CORE_H
#define DP_CORRELATION_CORE_H

#include "doppler/clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare module-level functions here. */

/**
 * @brief One signal's code phase: 1 when code phases @p a and @p b, chips,
 *        are within DP_CORRELATOR_SAME_PHASE_CHIPS of each other,
 *        circularly over a code of @p code_len chips; else 0.
 *
 * The one key the receiver's pick and the pool's duplicate test share,
 * and the one the peak zone is sized from. The receiver's pick drops a
 * peak within it of one already taken or of an excluded phase (rules 1
 * and 3 of docs/design/async-dsss-receiver.md section 7.2, keyed on code phase
 * alone, never carrier: async-dsss-receiver-measurements.md section
 * 12.14); the pool resets the second of two holders within it of each
 * other; and the zone the receiver asks its search for
 * (dp_correlator_set_peak_zone()) is half of it, so the zone stays
 * inside the key (#3071: a zone as wide as the key masked pairs two
 * lags apart that the key keeps). An example that
 * names which emitter a receiver holds calls this, so it identifies an
 * emitter by exactly the test the pool does. A C caller's example is on
 * DP_CORRELATOR_SAME_PHASE_CHIPS (correlator_core.h).
 *
 * The distance is |a - b| folded over the code, so phases either side of
 * chip 0 are near: 0.4 and 1022.8 are 0.6 chips apart on a 1023-chip
 * code. The bound is inclusive: phases exactly
 * DP_CORRELATOR_SAME_PHASE_CHIPS apart are one signal. Phases are taken
 * as given, each in [0, code_len); a NaN phase compares as two signals.
 *
 * @param a         A code phase, chips.
 * @param b         Another, chips.
 * @param code_len  The code's length, chips.
 * @return 1 when @p a and @p b are one signal's code phase, else 0.
 * @code
 * >>> from doppler.correlation import correlator_same_code_phase
 * >>> correlator_same_code_phase(100.0, 101.0, 1023.0)
 * 1
 * >>> correlator_same_code_phase(100.0, 101.5, 1023.0)
 * 0
 * >>> correlator_same_code_phase(0.4, 1022.8, 1023.0)  # across chip 0
 * 1
 *
 * @endcode
 */
int dp_correlator_same_code_phase(double a, double b, double code_len);

#ifdef __cplusplus
}
#endif

#endif /* DP_CORRELATION_CORE_H */
