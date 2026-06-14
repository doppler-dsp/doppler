/*
 * measure_core.c — Measure module implementation.
 *
 * The module-level capture-planning helpers (measure_min_samples,
 * measure_rec_nfft, measure_proc_gain, dp_coherent_freq) are each compiled
 * from their own translation unit, per jm's per-function layout.
 */
#include "measure/measure_core.h"
