/*
 * snr_core.c — snr module core.
 *
 * Implementations live in per-function .c files: snr_data_aided_db.c and
 * snr_m2m4_db.c hold the two algorithms; their _series siblings call them
 * directly on sliding-window slices, so each algorithm lives exactly
 * once. Mirrors the detection/wfm module layout.
 */
#include "snr/snr_core.h"
