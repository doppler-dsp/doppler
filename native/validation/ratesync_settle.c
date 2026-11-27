/**
 * @file ratesync_settle.c
 * @brief RateSync's settle_time against the object, past what the pin holds.
 *
 * dp_ratesync_get_settle_time() is a linear loop's figure. test_ratesync_core
 * pins it where the header claims it (RRC, Gardner, a start of 0.25 symbol or
 * less). This prints the rest: the error left from further out, and the
 * Gardner loop on the rectangle, which is the baseline the derivative
 * detector is compared against (doppler#3377,
 * docs/design/ratesync-timing.md section 9.2), with the detector's
 * open-loop slopes. The measurement is ratesync_settle_test.h's.
 *
 * The full run asserts what the header says: the held rows are inside the
 * bound, the RRC starts of 0.37 and 0.44 symbol are not, and the rectangle
 * has not settled by the figure. `--check` is the part of the last that
 * costs nothing to repeat: the open-loop slopes, the RRC's near 1 and the
 * rectangle's below it.
 *
 * Usage:  validate_ratesync_settle [--check]
 */
#include "dp_test.h"
#include "ratesync/ratesync_settle_test.h"

#include <stdio.h>
#include <string.h>

#define SEEDS 300

/* The detector's slope at one offset, open loop (bn = 0, so the offset
   stays put): its mean output over the mean offset. A chord, not the
   derivative. */
static double
chord (int pulse, double sps, size_t m, double tau)
{
  static double e[2000], ted[2000];
  if (!rs_settle_run (pulse, sps, m, 0.0, tau, 7u, 2000, e, ted))
    return 0.0;
  double se = 0.0, st = 0.0;
  for (size_t i = 50; i < 2000; i++)
    {
      se += e[i];
      st += ted[i];
    }
  printf ("open loop %-5s sps %g offset %+.4f symbol: detector slope %.3f\n",
          pulse == RATESYNC_PULSE_RRC ? "rrc" : "iandd", sps, se / 1950.0,
          st / se);
  return st / se;
}

static void
rows (void)
{
  static const struct
  {
    int    pulse;
    double sps;
    size_t m;
    double tau;
    int    expect; /* 1 settled by the figure, -1 not, 0 printed only */
  } row[] = {
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.25, 1 },
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.375, 1 },
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.4375, 1 },
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.625, 1 },
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.6875, 1 },
    /* starts of 0.31, 0.37 and 0.44 symbol */
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.8125, 0 },
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.125, -1 },
    { RATESYNC_PULSE_RRC, 4.0, 2, 0.0625, -1 },
    /* the rectangle */
    { RATESYNC_PULSE_IANDD, 4.0, 4, 0.25, -1 },
    { RATESYNC_PULSE_IANDD, 8.0, 4, 0.125, -1 },
    { RATESYNC_PULSE_IANDD, 8.0, 4, 0.25, -1 },
    { RATESYNC_PULSE_IANDD, 8.0, 4, 0.375, -1 },
    { RATESYNC_PULSE_IANDD, 8.0, 4, 0.5, -1 },
    { RATESYNC_PULSE_IANDD, 8.0, 4, 0.625, -1 },
  };
  for (size_t i = 0; i < sizeof row / sizeof *row; i++)
    {
      rs_settle_t r;
      const int   ran = rs_settle_measure (row[i].pulse, row[i].sps, row[i].m,
                                           row[i].tau, SEEDS, &r);
      DP_CHECK (ran);
      if (!ran)
        continue;
      printf ("%-5s sps %g start %+.4f symbol, worst residual from symbol %d "
              "%.5f = %.1f%% of it, %d symbols over the bound\n",
              row[i].pulse == RATESYNC_PULSE_RRC ? "rrc" : "iandd", row[i].sps,
              r.start, r.from, r.worst, 100.0 * r.worst / fabs (r.start),
              r.over);
      if (row[i].expect > 0)
        DP_CHECK (r.over == 0);
      else if (row[i].expect < 0)
        DP_CHECK (r.over > 0);
    }
}

int
main (int argc, char **argv)
{
  const int check = argc > 1 && strcmp (argv[1], "--check") == 0;
  DP_CHECK (chord (RATESYNC_PULSE_RRC, 4.0, 2, 0.4375) > 0.9);
  DP_CHECK (chord (RATESYNC_PULSE_IANDD, 8.0, 4, 0.125) < 0.9);
  DP_CHECK (chord (RATESYNC_PULSE_IANDD, 64.0, 4, 0.875) < 0.9);
  if (!check)
    rows ();
  DP_TEST_END ("validate_ratesync_settle");
}
