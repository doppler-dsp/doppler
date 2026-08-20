/* test_arith_core.c — smoke test for the arith module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 */
#include "arith/arith_core.h"

/* Both defines come BEFORE the include: jm_test.h defaults each if the
 * including file has not set it, so a later define would be ignored. */
#define JM_TEST_NAME "test_arith_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

int
main (void)
{
  /* TODO: add_q15(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: sub_q15(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: mul_q15(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: dot_q15(...) takes a non-scalar argument jm cannot synthesise; call
   * it here. */
  /* TODO: shl_q15(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: shr_q15(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: add_q8(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: sub_q8(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: mul_q8(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: dot_q8(...) takes a non-scalar argument jm cannot synthesise; call
   * it here. */
  /* TODO: shl_q8(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: shr_q8(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: shl_i64(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: shr_i64(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  JM_TEST_EPILOGUE ();
}
