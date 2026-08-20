/* test_measure_core.c — smoke test for the measure module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 */
#include "measure/measure_core.h"

/* Both defines come BEFORE the include: jm_test.h defaults each if the
 * including file has not set it, so a later define would be ignored. */
#define JM_TEST_NAME "test_measure_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

int
main (void)
{
  /* measure_min_samples: verify it runs without crashing */
  (void)measure_min_samples (0.0, 0.0, 0, 0.0, 0);
  /* measure_rec_nfft: verify it runs without crashing */
  (void)measure_rec_nfft (0, 0);
  /* measure_proc_gain: verify it runs without crashing */
  (void)measure_proc_gain (0);
  /* dp_coherent_freq: verify it runs without crashing */
  (void)dp_coherent_freq (0.0, 0.0, 0);
  JM_TEST_EPILOGUE ();
}
