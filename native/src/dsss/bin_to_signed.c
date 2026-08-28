/*
 * bin_to_signed.c — dsss module-level function.
 *
 * A WRAPPER, deliberately: the implementation is the `dp_fftfreq_index`
 * inline in clib_common.h, which every C caller includes and inlines. This
 * exists only so the Python face calls the SAME code instead of restating
 * the arithmetic.
 *
 * That is not hypothetical. Four Python call sites did restate it, in three
 * mutually inconsistent ways, and they disagreed with the engine at exactly
 * the Nyquist bin of an even grid -- reachable in burst mode, where
 * `doppler_bins` is a coherent depth and routinely even. The engine has
 * since been moved onto numpy's convention (see dp_fftfreq_index), so a
 * formula ported in from numpy now agrees; this wrapper is what makes
 * porting one unnecessary.
 */
#include "dsss/dsss_core.h"

int
bin_to_signed (size_t bin, size_t n_bins)
{
  return (int)dp_fftfreq_index (bin, n_bins);
}
