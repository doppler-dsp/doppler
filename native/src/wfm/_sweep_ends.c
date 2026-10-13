/*
 * _sweep_ends.c -- wfm module-level function (private).
 *
 * Exposes dp_wfm_synth_sweep_ends() (the one home of the chirp-centering
 * arithmetic, in wfm_synth_core.c) as _sweep_ends(fs, f_mid, rate, n, center)
 * -> [f0, f_end], the shape jm binds: a self-sizing array of two doubles.
 */
#include "doppler/wfm/wfm_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"

void
dp__sweep_ends (double fs, double f_mid, double rate, size_t n, double center,
                double *out)
{
  dp_wfm_synth_sweep_ends (fs, f_mid, rate, n, center, &out[0], &out[1]);
}
