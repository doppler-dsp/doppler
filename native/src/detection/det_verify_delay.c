#include "detection/detection_core.h"
#include <math.h>

double
det_verify_delay (double p_look, int n)
{
  /* Expected waiting time (in looks) for the first run of n consecutive
   * successes at per-look success probability p — the standard
   * consecutive-run result E[T] = (1 - p^n) / (p^n * (1 - p)). This is
   * the mean latency of a lockdet declare whose verify count is n. */
  if (n < 1)
    n = 1;
  if (p_look >= 1.0)
    return (double)n; /* certain hits: the run completes in exactly n */
  if (p_look <= 0.0)
    return HUGE_VAL; /* a run of successes never happens */
  /* 1 - p^n via expm1 keeps precision when p -> 1 makes p^n -> 1. */
  double nlp = (double)n * log (p_look);
  return -expm1 (nlp) / ((1.0 - p_look) * exp (nlp));
}
