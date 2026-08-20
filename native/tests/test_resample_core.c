/* test_resample_core.c — smoke test for the resample module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 */
#include "resample/resample_core.h"

/* Both defines come BEFORE the include: jm_test.h defaults each if the
 * including file has not set it, so a later define would be ignored. */
#define JM_TEST_NAME "test_resample_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

int
main (void)
{
  /* TODO: ciccompmf(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* kaiser_beta: verify it runs without crashing */
  (void)kaiser_beta (0.0);
  /* kaiser_num_taps: verify it runs without crashing */
  (void)kaiser_num_taps (0, 0.0, 0.0, 0.0);
  JM_TEST_EPILOGUE ();
}
