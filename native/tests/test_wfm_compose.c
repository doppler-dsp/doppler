/*
 * test_wfm_compose.c — multi-segment composer (Phase B).
 *
 * Verifies segment sequencing, off-time gaps (zeros), once-through completion,
 * and repeat looping — all over the reused Phase-A synth engine.
 */
#define _GNU_SOURCE
#include "wfmgen/wfm_compose.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, msg)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", msg);                                \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

int
main (void)
{
  /* tone @100kHz (1000 on, 500 off), then qpsk (4096 on, 0 off). */
  wfm_segment_t segs[2] = {
    { .type        = 0,
      .fs          = 1e6,
      .freq        = 1e5,
      .snr         = 100.0,
      .snr_mode    = 0,
      .seed        = 1,
      .sps         = 8,
      .pn_length   = 7,
      .pn_poly     = 0,
      .num_samples = 1000,
      .off_samples = 500 },
    { .type        = 4,
      .fs          = 1e6,
      .freq        = 0,
      .snr         = 100.0,
      .snr_mode    = 0,
      .seed        = 5,
      .sps         = 8,
      .pn_length   = 7,
      .pn_poly     = 0,
      .num_samples = 4096,
      .off_samples = 0 },
  };

  /* ── once-through: collect the whole stream in odd-sized chunks ── */
  wfm_compose_state_t *c = wfm_compose_create (segs, 2, 0, 0);
  CHECK (c, "create");
  static float complex all[8192];
  size_t               total = 0, n;
  float complex        buf[777];
  while ((n = wfm_compose_execute (c, buf, 777)) > 0)
    {
      CHECK (total + n <= 8192, "overflow");
      for (size_t i = 0; i < n; i++)
        all[total + i] = buf[i];
      total += n;
    }
  CHECK (total == 1000 + 500 + 4096, "total sample count");

  /* tone region non-zero; off region exactly zero; qpsk region non-zero */
  for (size_t i = 0; i < 1000; i++)
    CHECK (all[i] != 0.0f, "tone region should be non-zero");
  for (size_t i = 1000; i < 1500; i++)
    CHECK (all[i] == 0.0f, "off-time gap should be zero");
  for (size_t i = 1500; i < 1500 + 4096; i++)
    CHECK (all[i] != 0.0f, "qpsk region should be non-zero");

  /* tone sits at +0.1 cyc/sample (100kHz / 1MHz): correlation ≈ 1 */
  double re = 0, im = 0;
  for (int k = 0; k < 1000; k++)
    {
      double ph = -2.0 * M_PI * 0.1 * k;
      re += creal (all[k]) * cos (ph) - cimag (all[k]) * sin (ph);
      im += creal (all[k]) * sin (ph) + cimag (all[k]) * cos (ph);
    }
  CHECK (sqrt (re * re + im * im) / 1000.0 > 0.95, "tone freq/correlation");
  wfm_compose_destroy (c);

  /* ── repeat: the sequence loops, execute never returns short ── */
  wfm_compose_state_t *r = wfm_compose_create (segs, 2, 1, 0);
  CHECK (r, "create repeat");
  for (int it = 0; it < 8; it++)
    CHECK (wfm_compose_execute (r, all, 8192) == 8192, "repeat loops full");
  wfm_compose_destroy (r);

  /* ── JSON round-trip: spec → JSON → spec produces identical output ── */
  char *json = wfm_spec_to_json (segs, 2, 0, 0);
  CHECK (json, "to_json");
  CHECK (strstr (json, "\"tone\"") && strstr (json, "\"qpsk\""), "type names");
  CHECK (strstr (json, "wfmgen-1"), "version tag");
  wfm_compose_state_t *jc = wfm_compose_from_json (json);
  CHECK (jc, "from_json");
  static float complex jall[8192];
  size_t               jtotal = 0;
  while ((n = wfm_compose_execute (jc, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        jall[jtotal + i] = buf[i];
      jtotal += n;
    }
  CHECK (jtotal == total, "round-trip sample count");
  /* re-collect the direct stream for a byte comparison */
  wfm_compose_state_t *d      = wfm_compose_create (segs, 2, 0, 0);
  size_t               dtotal = 0;
  while ((n = wfm_compose_execute (d, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        all[dtotal + i] = buf[i];
      dtotal += n;
    }
  for (size_t i = 0; i < total; i++)
    CHECK (jall[i] == all[i], "JSON round-trip must be sample-identical");
  wfm_compose_destroy (jc);
  wfm_compose_destroy (d);

  /* bad spec → NULL (unknown type, empty segments) */
  CHECK (!wfm_compose_from_json ("{\"segments\":[{\"type\":\"bogus\"}]}"),
         "unknown type rejected");
  CHECK (!wfm_compose_from_json ("{\"segments\":[]}"), "empty rejected");

  free (json);
  printf ("test_wfm_compose: OK (total=%zu, json round-trip ok)\n", total);
  return 0;
}
