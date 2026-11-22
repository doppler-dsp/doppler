/* bench_wlan_core.c -- what a frame costs, stage by stage.
 *
 * The first two rows are the table-free builders: the minimal MAC's FCS over
 * a 1500-octet body, and the Data field's chain up to the interleaver (the
 * SERVICE, tail and pad assembly, the scrambler, and the punctured BCC) for
 * 100 octets at 16-QAM rate 3/4. The other five run on the standard's tables
 * at the largest rate pair the standard has (64-QAM, rate 3/4, 54 Mb/s) over
 * a 1000-octet PSDU: the interleaver, the mapper, the points (those two
 * and the chain before them), the layout, and the whole PPDU, so a frame's
 * cost end to end and where it goes are both numbers (#3147).
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
  /* The stages that need the standard's tables, at the largest rate pair:
     64-QAM, rate 3/4. */
  {
    const dp_wlan_tables_t *tab = dp_wlan_tables_standard ();
    const dp_wlan_rate_t    big = { 6, WLAN_CODE_3_4 };
    enum
    {
      OCTETS = 1000,
      HEAVY  = 20
    };
    static uint8_t big_psdu[OCTETS];
    for (int i = 0; i < OCTETS; i++)
      big_psdu[i] = (uint8_t)(i * 29 + 11);
    const size_t n_sym  = dp_wlan_rate_n_sym (&big, OCTETS);
    const size_t cbps   = dp_wlan_n_cbps (&big);
    const size_t n_bits = n_sym * dp_wlan_n_dbps (&big);
    const size_t n_cod  = n_sym * cbps;
    const size_t n_pts  = 48 + 48 * n_sym;
    const size_t n_smp  = dp_wlan_ppdu_len (n_sym, 0u, 1u);
    uint8_t     *bits = malloc (n_bits), *coded = malloc (n_cod),
            *inter       = malloc (n_cod);
    float _Complex *pts  = malloc (n_pts * sizeof *pts),
                   *ppdu = malloc (n_smp * sizeof *ppdu);
    if (!bits || !coded || !inter || !pts || !ppdu || n_sym == 0)
      return 1;
    if (dp_wlan_data_bits (&big, big_psdu, OCTETS, 93, bits, n_bits) != n_bits
        || dp_wlan_encode (big.code, bits, n_bits, coded, n_cod) != n_cod)
      return 1;

    static double t_il[ITERATIONS], t_map[ITERATIONS], t_pts[ITERATIONS],
        t_lay[ITERATIONS], t_ppdu[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        uint64_t t0 = jm_bench_now_ns ();
        for (int k = 0; k < HEAVY; k++)
          sink += dp_wlan_interleave (tab, big.n_bpsc, coded, n_cod, inter);
        t_il[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());

        t0 = jm_bench_now_ns ();
        for (int k = 0; k < HEAVY; k++)
          sink += dp_wlan_map (tab, big.n_bpsc, inter, n_cod, pts);
        t_map[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());

        t0 = jm_bench_now_ns ();
        for (int k = 0; k < HEAVY; k++)
          sink += dp_wlan_points (tab, &big, big_psdu, OCTETS, 93, pts, n_pts);
        t_pts[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());

        t0 = jm_bench_now_ns ();
        for (int k = 0; k < HEAVY; k++)
          {
            dp_ofdm_state_t *o = dp_ofdm_create ();
            sink += (size_t)dp_wlan_layout (o, tab, n_sym, 0u, 1u) + 1u;
            dp_ofdm_destroy (o);
          }
        t_lay[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());

        t0 = jm_bench_now_ns ();
        for (int k = 0; k < HEAVY; k++)
          sink += dp_wlan_ppdu (tab, &big, big_psdu, OCTETS, 93, 0u, 1u, ppdu,
                                n_smp);
        t_ppdu[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
      }
    jm_bench_add_unit (&_bench, "interleave[octets=1000,64qam,r34]", t_il,
                       ITERATIONS, HEAVY, "frame");
    jm_bench_add_unit (&_bench, "map[octets=1000,64qam,r34]", t_map,
                       ITERATIONS, HEAVY, "frame");
    jm_bench_add_unit (&_bench, "points[octets=1000,64qam,r34]", t_pts,
                       ITERATIONS, HEAVY, "frame");
    jm_bench_add_unit (&_bench, "layout[octets=1000,64qam,r34]", t_lay,
                       ITERATIONS, HEAVY, "frame");
    jm_bench_add_unit (&_bench, "ppdu[octets=1000,64qam,r34]", t_ppdu,
                       ITERATIONS, HEAVY, "frame");
    printf ("  interleave[octets=1000,64qam,r34] %9.2f us/frame\n",
            t_il[0] / HEAVY * 1e6);
    printf ("  map[octets=1000,64qam,r34]        %9.2f us/frame\n",
            t_map[0] / HEAVY * 1e6);
    printf ("  points[octets=1000,64qam,r34]     %9.2f us/frame\n",
            t_pts[0] / HEAVY * 1e6);
    printf ("  layout[octets=1000,64qam,r34]     %9.2f us/frame\n",
            t_lay[0] / HEAVY * 1e6);
    printf ("  ppdu[octets=1000,64qam,r34]       %9.2f us/frame\n",
            t_ppdu[0] / HEAVY * 1e6);
    free (bits);
    free (coded);
    free (inter);
    free (pts);
    free (ppdu);
  }

  (void)sink;
  jm_bench_write_json (&_bench, "wlan");
  return 0;
}
