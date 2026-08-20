/* test_wfm_core.c — smoke test for the wfm module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 */
#include "wfm/wfm_core.h"

/* Both defines come BEFORE the include: jm_test.h defaults each if the
 * including file has not set it, so a later define would be ignored. */
#define JM_TEST_NAME "test_wfm_core"
#define JM_SCAFFOLD_CHECKS 0

#include "jm_test.h"

int
main (void)
{
  /* TODO: bpsk_map(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: qpsk_map(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* wfm_awgn_amplitude: verify it runs without crashing */
  (void)wfm_awgn_amplitude (0.0f, 0.0f);
  /* wfm_ebno_to_snr_db: verify it runs without crashing */
  (void)wfm_ebno_to_snr_db (0.0f, 0, 0.0f);
  /* mls_poly: verify it runs without crashing */
  (void)mls_poly (0U);
  /* TODO: ccsds_asm_bits(...) writes into a caller-sized output buffer jm
   * cannot synthesise; call it here. */
  /* TODO: crc16(...) takes a non-scalar argument jm cannot synthesise; call it
   * here. */
  /* TODO: rrc_h(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: rc_h(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: rrc_taps(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  /* TODO: dsss_spread(...) writes into a caller-sized output buffer jm cannot
   * synthesise; call it here. */
  JM_TEST_EPILOGUE ();
}
