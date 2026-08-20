/* test_spectral_core.c — smoke test for the spectral module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 */
#include "spectral/spectral_core.h"

/* Both defines come BEFORE the include: jm_test.h defaults each if the
 * including file has not set it, so a later define would be ignored. */
#define JM_TEST_NAME "test_spectral_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

int
main (void)
{
  /* TODO: kaiser_enbw(...) takes a non-scalar argument jm cannot synthesise;
   * call it here. */
  /* TODO: kaiser_window(...) takes a non-scalar argument jm cannot synthesise;
   * call it here. */
  /* kaiser_beta_for_sidelobe: verify it runs without crashing */
  (void)kaiser_beta_for_sidelobe (0.0);
  /* TODO: hann_window(...) takes a non-scalar argument jm cannot synthesise;
   * call it here. */
  /* TODO: blackman_harris_window(...) takes a non-scalar argument jm cannot
   * synthesise; call it here. */
  /* TODO: magnitude_db_cf32(...) writes into a caller-sized output buffer jm
   * cannot synthesise; call it here. */
  /* TODO: magnitude_db_cf64(...) writes into a caller-sized output buffer jm
   * cannot synthesise; call it here. */
  /* TODO: find_peaks_f32(...) takes a non-scalar argument jm cannot
   * synthesise; call it here. */
  /* TODO: obw_from_power(...) takes a non-scalar argument jm cannot
   * synthesise; call it here. */
  /* TODO: noise_floor_db(...) takes a non-scalar argument jm cannot
   * synthesise; call it here. */
  JM_TEST_EPILOGUE ();
}
