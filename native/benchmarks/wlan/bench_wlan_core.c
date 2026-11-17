/* bench_wlan_core.c -- what the table-free builders cost.
 *
 * Two rows, both over a PSDU a real frame size: the minimal MAC's FCS over a
 * 1500-octet body, and the Data field's chain up to the interleaver (the
 * SERVICE, tail and pad assembly, the scrambler, and the punctured BCC) for
 * 100 octets at 16-QAM rate 3/4. The interleaver, the mapper and the layout
 * need the standard's tables and are measured with the characterization.
 *
 *   make bench BENCH_ARGS="--c-only wlan"
 */
#include "doppler/wlan/wlan_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 200

int
main (void)
{
  jm_bench_t _bench = { 0 };
  printf ("=== wlan benchmark ===\n\n");

  /* the FCS over a 1500-octet body */
  enum
  {
    BODY = 1500,
    REPS = 200
  };
  static uint8_t body[BODY], mpdu[BODY + 4];
  for (int i = 0; i < BODY; i++)
    body[i] = (uint8_t)(i * 31 + 7);
  static double   t_mpdu[ITERATIONS];
  volatile size_t sink = 0;
  for (int r = 0; r < ITERATIONS; r++)
    {
      const uint64_t t0 = jm_bench_now_ns ();
      for (int k = 0; k < REPS; k++)
        sink += dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, body, BODY, mpdu,
                              sizeof mpdu);
      t_mpdu[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
    }
  jm_bench_add_unit (&_bench, "mpdu[body=1500]", t_mpdu, ITERATIONS, REPS,
                     "frame");

  /* the Data field's bits: assemble, scramble, encode (100 octets, 16-QAM 3/4)
   */
  const dp_wlan_rate_t rate = { 4, WLAN_CODE_3_4 };
  enum
  {
    PSDU       = 100,
    CHAIN_REPS = 100
  };
  static uint8_t psdu[PSDU], bits[6 * 144], coded[6 * 192];
  for (int i = 0; i < PSDU; i++)
    psdu[i] = (uint8_t)(i * 13 + 5);
  static double t_chain[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      const uint64_t t0 = jm_bench_now_ns ();
      for (int k = 0; k < CHAIN_REPS; k++)
        {
          const size_t n
              = dp_wlan_data_bits (&rate, psdu, PSDU, 93, bits, sizeof bits);
          sink += dp_wlan_encode (rate.code, bits, n, coded, sizeof coded);
        }
      t_chain[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
    }
  jm_bench_add_unit (&_bench, "data_chain[octets=100,16qam,r34]", t_chain,
                     ITERATIONS, CHAIN_REPS, "frame");
  printf ("  mpdu[body=1500]                   %9.2f us/frame\n",
          t_mpdu[0] / REPS * 1e6);
  printf ("  data_chain[octets=100,16qam,r34]  %9.2f us/frame\n",
          t_chain[0] / CHAIN_REPS * 1e6);
  (void)sink;
  jm_bench_write_json (&_bench, "wlan");
  return 0;
}
