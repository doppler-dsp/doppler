/* bench_wfm_core.c -- the waveform generator's per-sample kernels.
 *
 * `wfm`'s objects are benchmarked (`wfm_synth`, `wfm_reader`,
 * `wfm_writer`, `pn`, `gold`). The free functions underneath them are
 * not, and they are where a generated waveform actually spends its time:
 * `dsss_spread` runs once per CHIP, `bpsk_map`/`qpsk_map` once per symbol,
 * `crc16` once per frame bit. A composer that looks slow is slow in one of
 * these, and until now there was no row to look at.
 *
 * The design-time pair is here for a reason beyond completeness.
 * `rrc_taps` builds a 2*span*sps+1 filter, and `rrc_h` is the analytic
 * impulse response it evaluates. So the two rows together answer a
 * question neither answers alone -- **is `rrc_taps` doing anything except
 * calling `rrc_h` at each tap?** If the per-tap costs match, it is not,
 * and the normalisation is honest. If they diverge, the difference is the
 * energy normalisation and the singularity handling, and that is worth
 * knowing before anyone reaches for a hand-rolled version.
 *
 * Units differ per row because the natural unit differs -- chips for the
 * spreader, bits for the CRC, taps for the design. Each row is normalised
 * to its OWN output element, so `ops` in the JSON means what it says.
 */
#include "dp_bench.h"
#include "wfm/wfm_core.h"
#include <complex.h>
#include <stdint.h>
#include <stdio.h>

#define ITERATIONS 100
#define N_BITS 8192
#define N_SYMS 512
#define SF 31
#define CODE_LEN 31
#define RRC_SPS 4
#define RRC_SPAN 8
#define RRC_TAPS (2 * RRC_SPAN * RRC_SPS + 1)
#define N_T 4096

enum
{
  C_BPSK_MAP,
  C_QPSK_MAP,
  C_CRC16,
  C_DSSS_SPREAD,
  C_RRC_TAPS,
  C_RRC_H,
  C_RC_H,
  C_MLS_POLY,
  C_ASM_BITS,
  N_CFG
};

static const char *const cfg_name[N_CFG] = {
  "bpsk_map[8192]",
  "qpsk_map[8192]",
  "crc16[8192]",
  "dsss_spread[sf=31,512sym]",
  "rrc_taps[sps=4,span=8]",
  "rrc_h[4096]",
  "rc_h[4096]",
  "mls_poly",
  "ccsds_asm_bits",
};

/* Calls per round, and the output elements one call produces. */
static const int    cfg_reps[N_CFG] = { 8, 8, 8, 4, 1024, 16, 16, 8192, 2048 };
static const size_t cfg_out[N_CFG]  = {
  N_BITS, N_BITS, N_BITS, (size_t)N_SYMS *SF, RRC_TAPS, N_T, N_T, 1u, 32u
};
static const char *const cfg_unit[N_CFG] = {
  "bit", "sym", "bit", "chip", "tap", "eval", "eval", "call", "bit",
};

static uint8_t       bits[N_BITS];
static uint8_t       qsyms[N_BITS];
static float complex map_out[N_BITS];
static float complex syms[N_SYMS];
static uint8_t       code[CODE_LEN];
static float complex chips[(size_t)N_SYMS * SF];
static float         taps[RRC_TAPS];
static double        tvec[N_T], hout[N_T];
static uint8_t       asm_out[32];

static volatile double sink = 0.0;

static void
run (int cfg, int i)
{
  switch (cfg)
    {
    case C_BPSK_MAP:
      bpsk_map (bits, N_BITS, map_out);
      sink += crealf (map_out[0]);
      break;
    case C_QPSK_MAP:
      qpsk_map (qsyms, N_BITS, map_out);
      sink += crealf (map_out[0]);
      break;
    case C_CRC16:
      sink += (double)crc16 (bits, N_BITS);
      break;
    case C_DSSS_SPREAD:
      dsss_spread (syms, N_SYMS, code, CODE_LEN, SF, chips);
      sink += crealf (chips[0]);
      break;
    case C_RRC_TAPS:
      rrc_taps (0.35, RRC_SPS, RRC_SPAN, taps);
      sink += taps[RRC_TAPS / 2];
      break;
    case C_RRC_H:
      rrc_h (tvec, N_T, hout, 0.35);
      sink += hout[0];
      break;
    case C_RC_H:
      rc_h (tvec, N_T, hout, 0.35);
      sink += hout[0];
      break;
    case C_MLS_POLY:
      sink += (double)mls_poly (7u + (uint32_t)(i & 7));
      break;
    default:
      ccsds_asm_bits (asm_out);
      sink += asm_out[3];
      break;
    }
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];

  for (int i = 0; i < N_BITS; i++)
    {
      bits[i]  = (uint8_t)(i & 1);
      qsyms[i] = (uint8_t)(i & 3);
    }
  for (int i = 0; i < N_SYMS; i++)
    syms[i] = (i & 1) ? -1.0f + 0.0f * I : 1.0f + 0.0f * I;
  for (int i = 0; i < CODE_LEN; i++)
    code[i] = (uint8_t)((i * 7 + 3) & 1);
  /* Symbol-normalised time, span/2 either side of the centre tap, and
     deliberately NOT hitting t = 0 or t = +-1/(4*beta) exactly: those are
     the removable singularities, and a grid that lands on them would time
     the special case instead of the formula. */
  for (int i = 0; i < N_T; i++)
    tvec[i] = -4.0 + 8.0 * ((double)i + 0.5) / (double)N_T;

  printf ("=== wfm (waveform generator kernels) ===\n");
  printf ("%d rounds; each row normalised to its own output unit\n\n",
          ITERATIONS);

  DP_BENCH_SETTLE (run (C_DSSS_SPREAD, 0));

  /* Rounds outside, kernels inside -- rrc_taps and rrc_h are read
     against each other per tap, so drift must not land on one alone. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < cfg_reps[c]; k++)
          run (c, k);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    dp_bench_record (&_bench, cfg_name[c], t[c], ITERATIONS,
                     cfg_out[c] * (size_t)cfg_reps[c], cfg_unit[c]);

  {
    const double per_tap
        = dp_bench_min (t[C_RRC_TAPS], ITERATIONS)
          / ((double)cfg_out[C_RRC_TAPS] * cfg_reps[C_RRC_TAPS]);
    const double per_eval = dp_bench_min (t[C_RRC_H], ITERATIONS)
                            / ((double)cfg_out[C_RRC_H] * cfg_reps[C_RRC_H]);
    printf ("\n  rrc_taps costs %.2fx rrc_h per output. Near 1.0 the design\n"
            "  is its formula and nothing else; above it, the excess is the\n"
            "  unit-energy normalisation -- a second pass over the taps --\n"
            "  which is exactly what a hand-rolled copy tends to omit.\n",
            per_tap / per_eval);
  }

  (void)sink;
  jm_bench_write_json (&_bench, "wfm");
  return 0;
}
