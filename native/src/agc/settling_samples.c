/*
 * settling_samples.c — agc module-level function.
 *
 * A delegating shim, deliberately. The module function jm binds is
 * `settling_samples`, which is the right name on the PYTHON face
 * (`doppler.agc.settling_samples`) and the wrong one in a C library's flat
 * namespace, where every other symbol here carries the `agc_` prefix. So
 * the kernel keeps the prefix and lives beside the loop it simulates, in
 * agc_core.c, and this file is the two lines that join the two naming
 * conventions.
 *
 * Same shape as write_blue_header.c in wfm_writer, for the same reason.
 * There is no logic here to drift: the body is one call.
 */
#include "agc/agc_core.h"

size_t
settling_samples (double loop_bw, double alpha, double gain_err_db,
                  double tol_db)
{
  return agc_settling_samples (loop_bw, alpha, gain_err_db, tol_db);
}
