/* test_ccsds_core.c — smoke test for the ccsds module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 *
 * `asm_bits` is an alias over `ccsds_tm_asm_bits`, and the pattern itself is
 * held to the published figure by test_ccsds_tm_asm. What is checked HERE is
 * the thing an alias can get wrong on its own: that the marker survives the
 * hop to this face byte for byte, in the same bit order, with all 32 written.
 */
#include "dp_test.h"

#include "ccsds/ccsds_core.h"
#include "ccsds_tm/ccsds_tm.h"
#include <stdio.h>
#include <string.h>

int
main (void)
{
  /* ── asm_bits: the CCSDS attached sync marker, exactly ───────────── */
  {
    uint8_t  bits[32];
    uint32_t v = 0;

    /* Poisoned, so a function that writes only a prefix is caught rather
       than reading back whatever the stack happened to hold. */
    memset (bits, 0xAA, sizeof bits);
    asm_bits (bits);
    for (int i = 0; i < 32; i++)
      {
        DP_CHECK (bits[i] == 0 || bits[i] == 1);
        v = (v << 1) | bits[i];
      }
    /* 0x1ACFFC1D, MSB first. A frame synchroniser that disagrees with
       this by one bit finds nothing, forever, silently. */
    DP_CHECK (v == 0x1ACFFC1Du);
  }

  /* ── the alias does not diverge from what it aliases ─────────────── */
  {
    uint8_t here[32], there[32];

    memset (here, 0xAA, sizeof here);
    memset (there, 0x55, sizeof there);
    asm_bits (here);
    ccsds_tm_asm_bits (there);
    /* The whole reason this is a call and not a constant: two expansions
       of 0x1ACFFC1D can disagree, and a receiver that disagrees with the
       assembler syncs to nothing. Assert they cannot. */
    DP_CHECK (memcmp (here, there, sizeof here) == 0);
  }

  DP_TEST_END ("test_ccsds_core");
}
