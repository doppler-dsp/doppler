/* bench_nco_core.c -- what each of the NCO's six faces costs over the bare
 * phase accumulator.
 *
 * The kernel underneath all six is one add: `phase += phase_inc`, wrapping
 * for free because the accumulator is `uint32_t`. Everything else the
 * component offers is a decoration on that add, and each decoration has its
 * own entry point:
 *
 *   scaled  -- map the accumulator onto `[0, nmax)` instead of the full
 *              32-bit turn, which costs a multiply and a shift per sample.
 *   ovf     -- also write the per-sample carry, so a caller can see the
 *              wrap the accumulator swallowed. A second output stream.
 *   ctrl    -- take a per-sample frequency offset, so `phase_inc` is
 *              recomputed every sample from a `double` instead of held.
 *
 * A caller picks one of the six by name and gets no signal about which
 * ones are nearly free and which are not. That is the question here: every
 * row is reported as a multiple of the plain `steps_u32` row measured in
 * the same interleave, so the comparison survives a machine that drifts.
 *
 * `ctrl` is the one worth predicting before reading: it turns a loop with
 * one integer add into a loop that reads a `double`, converts it, and
 * re-derives a phase increment. It also streams a second input buffer --
 * so part of what it costs is bandwidth, not arithmetic, and the two are
 * not separable from this row alone.
 */
#include "dp_bench.h"
#include "nco/nco_core.h"
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 65536
#define ITERATIONS 200
#define NORM_FREQ 0.01
#define NMAX 4096u

enum
{
  CFG_PLAIN,
  CFG_SCALED,
  CFG_OVF,
  CFG_CTRL,
  CFG_SCALED_CTRL,
  CFG_OVF_CTRL,
  N_CFG
};

static const char *cfg_name[N_CFG] = {
  "steps_u32",      "steps_u32_scaled",      "steps_u32_ovf",
  "steps_u32_ctrl", "steps_u32_scaled_ctrl", "steps_u32_ovf_ctrl",
};

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  nco_state_t    *nco  = nco_create (NORM_FREQ, NMAX);
  uint32_t       *out  = NULL;
  uint8_t        *ovf  = NULL;
  double         *ctrl = NULL;

  if (!nco)
    return 1;

  out  = malloc (BLOCK * sizeof *out);
  ovf  = malloc (BLOCK * sizeof *ovf);
  ctrl = malloc (BLOCK * sizeof *ctrl);
  if (!out || !ovf || !ctrl)
    return 1;

  /* A small non-zero control: zero everywhere would let the compiler and
     the branch predictor see a constant increment, which is not what a
     tracking loop hands this function. */
  for (size_t i = 0; i < BLOCK; i++)
    ctrl[i] = 1e-6 * (double)((i % 17) - 8);

  printf ("=== nco (%d samples per call) ===\n", BLOCK);
  printf ("norm_freq = %.3f, nmax = %u, %d rounds, min over rounds\n\n",
          NORM_FREQ, NMAX, ITERATIONS);

  DP_BENCH_SETTLE (nco_steps_u32 (nco, BLOCK, out, BLOCK));

  /* Rounds outside, faces inside. Every row here is read as a multiple of
     the first one, so drift the settle missed must land on all six. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        switch (c)
          {
          case CFG_PLAIN:
            nco_steps_u32 (nco, BLOCK, out, BLOCK);
            break;
          case CFG_SCALED:
            nco_steps_u32_scaled (nco, BLOCK, out, BLOCK);
            break;
          case CFG_OVF:
            nco_steps_u32_ovf (nco, BLOCK, out, ovf, BLOCK);
            break;
          case CFG_CTRL:
            nco_steps_u32_ctrl (nco, ctrl, BLOCK, out, BLOCK);
            break;
          case CFG_SCALED_CTRL:
            nco_steps_u32_scaled_ctrl (nco, ctrl, BLOCK, out, BLOCK);
            break;
          case CFG_OVF_CTRL:
            nco_steps_u32_ovf_ctrl (nco, ctrl, BLOCK, out, ovf, BLOCK);
            break;
          default:
            break;
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    dp_bench_record (&_bench, cfg_name[c], t[c], ITERATIONS, BLOCK, "sample");

  printf ("\n  cost over the bare accumulator (steps_u32 = 1.00x):\n");
  for (int c = 1; c < N_CFG; c++)
    printf ("    %-22s %.2fx\n", cfg_name[c],
            dp_bench_min (t[c], ITERATIONS)
                / dp_bench_min (t[CFG_PLAIN], ITERATIONS));
  printf ("  The six are one `phase += phase_inc` plus a decoration. A row\n"
          "  near 1.00x is a face worth taking for free; a row well above\n"
          "  it is a reason to hold phase_inc rather than re-derive it.\n");

  free (out);
  free (ovf);
  free (ctrl);
  nco_destroy (nco);
  jm_bench_write_json (&_bench, "nco");
  return 0;
}
