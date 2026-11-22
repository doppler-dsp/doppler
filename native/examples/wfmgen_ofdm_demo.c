/**
 * @file wfmgen_ofdm_demo.c
 * @brief 802.11 PPDUs from a data source.
 *
 * The C twin of `src/doppler/examples/wfmgen_ofdm_demo.py`. `type =
 * WFM_SYNTH_FRAMES` (the `ofdm` type) sends one PPDU per frame of a data
 * source, built through `wlan`: the frame is the PSDU's octets (the first bit
 * of each eight the most significant, as a file's bytes unpack), `wlan = 36`
 * is the 20 MHz data rate in Mb/s, and `mac = WLAN_MAC_DATA` puts a MAC header
 * (`mac_header`) in front and the FCS behind.
 *
 * What it shows, in order:
 *
 *  1. one source is the PPDU `dp_wlan_ppdu` builds from the same octets, the
 *     header, the body and the FCS from `dp_wlan_mpdu`;
 *  2. a source with two frames sends two PPDUs, and with `scrambler_init` 0
 *     each draws its scrambler's initial state from `seed` and its index;
 *  3. `fs` twice the bandwidth is a larger transform, not a resampler: with
 *     no window every second sample is the 20 MHz PPDU's.
 *
 * Every claim is a check; the program exits non-zero if any fails.
 *
 * Build and run (from the repo root, after `make build`):
 *
 *     ./build/native/examples/wfmgen_ofdm_demo
 */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h"
#include "doppler/wlan/wlan_core.h"

typedef float _Complex cf;

static int failures = 0;

static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

/* The source's samples, run to its end: `n` of them, or 0 if it was refused.
 */
static size_t
run (wfm_source_t *src, double fs, cf *out, size_t cap)
{
  wfm_segment_t seg = { .sources = src, .n_sources = 1, .fs = fs };
  const char   *why = dp_wfm_scene_error (&seg, 1, 0, 0);
  if (why)
    {
      fprintf (stderr, "refused: %s\n", why);
      return 0;
    }
  dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
  if (!c)
    return 0;
  const size_t n = dp_wfm_compose_execute (c, out, cap);
  dp_wfm_compose_destroy (c);
  return n;
}

int
main (void)
{
  /* 100 octets of body behind a 24-octet header, as bits, the first bit of
     each eight the most significant. The data is two copies: two frames. */
  uint8_t octets[100], header[24];
  uint8_t bits[1600], hbits[8 * 24];
  for (size_t k = 0; k < 100; k++)
    octets[k] = (uint8_t)(7u * k + 3u);
  for (size_t k = 0; k < 24; k++)
    header[k] = (uint8_t)(0x08u + 11u * k);
  for (size_t k = 0; k < 100; k++)
    for (unsigned j = 0; j < 8u; j++)
      bits[8u * k + j] = bits[800u + 8u * k + j]
          = (octets[k] >> (7u - j)) & 1u;
  for (size_t k = 0; k < 24; k++)
    for (unsigned j = 0; j < 8u; j++)
      hbits[8u * k + j] = (header[k] >> (7u - j)) & 1u;

  wfm_source_t src   = WFM_SOURCE_DEFAULTS;
  src.type           = WFM_SYNTH_FRAMES;
  src.wlan           = 36; /* 16-QAM, rate 3/4 */
  src.mac            = WLAN_MAC_DATA;
  src.mac_header     = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL,
                                    .bits = hbits,
                                    .len  = sizeof hbits };
  src.data           = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL,
                                    .bits = bits,
                                    .len  = sizeof bits };
  src.data_len       = 800; /* one PPDU per 800 bits */
  src.scrambler_init = 93;
  src.snr            = 100.0;

  /* 1. the PPDU wlan builds from header + body + FCS */
  uint8_t      psdu[WLAN_PSDU_MAX];
  const size_t len = dp_wlan_mpdu (WLAN_MAC_DATA, header, 24, octets, 100,
                                   psdu, sizeof psdu);
  const dp_wlan_rate_t rate = { 4u, WLAN_CODE_3_4 };
  /* the source's window is unset: the standard's, which wlan resolves the
     same way (2 samples at 20 MHz) */
  const uint32_t win = dp_wlan_window_resolve (WLAN_WINDOW_STANDARD, 1u);
  const size_t n = dp_wlan_ppdu_len (dp_wlan_rate_n_sym (&rate, len), win, 1u);
  cf          *want = malloc (n * sizeof *want);
  cf          *x    = malloc ((2 * n + 64u) * sizeof *x);
  if (!want || !x
      || dp_wlan_ppdu (dp_wlan_tables_standard (), &rate, psdu, len, 93, win,
                       1u, want, n)
             != n)
    return 1;
  const size_t got = run (&src, 20e6, x, 2 * n + 64u);
  printf ("PSDU %zu octets -> a PPDU of %zu samples; the run is %zu\n", len, n,
          got);
  check (got == 2 * n, "1. two frames are two PPDUs, one each");
  double num = 0, den = 0;
  for (size_t i = 0; i < n; i++)
    {
      const double d = cabs ((double _Complex)x[i] - want[i]);
      num += d * d;
      den += cabs (want[i]) * cabs (want[i]);
    }
  check (den > 0.0 && sqrt (num / den) < 1e-6,
         "1. the first PPDU is dp_wlan_ppdu of header + body + FCS");

  /* 2. a drawn scrambler: each PPDU its own state, reproducibly */
  src.scrambler_init = 0;
  src.seed           = 5;
  cf *y              = malloc ((2 * n + 64u) * sizeof *y);
  if (!y || run (&src, 20e6, y, 2 * n + 64u) != 2 * n)
    return 1;
  check (memcmp (y, y + n, n * sizeof *y) != 0,
         "2. a drawn scrambler gives each PPDU its own state");

  /* 3. twice the bandwidth: a larger transform. A window cross-fades each
     symbol boundary on a finer grid at 40 MHz, so the two rates agree
     outside it; with window 0 every second sample is the 20 MHz one. */
  src.scrambler_init = 93;
  src.seed           = 0;
  src.window         = 0;
  const size_t n0    = n - win; /* a PPDU without its window */
  cf          *r     = malloc ((2 * n0 + 64u) * sizeof *r);
  cf          *z     = malloc ((4 * n0 + 64u) * sizeof *z);
  if (!r || !z || run (&src, 20e6, r, 2 * n0 + 64u) != 2 * n0
      || run (&src, 40e6, z, 4 * n0 + 64u) != 4 * n0)
    return 1;
  num = 0;
  den = 0;
  for (size_t i = 0; i < n0; i++)
    {
      const double d = cabs ((double _Complex)z[2 * i] - r[i]);
      num += d * d;
      den += creal (r[i]) * creal (r[i]) + cimag (r[i]) * cimag (r[i]);
    }
  check (sqrt (num / den) < 1.5e-3,
         "3. with no window every second sample of the 40 MHz PPDU is the "
         "20 MHz one");

  free (want);
  free (x);
  free (y);
  free (z);
  free (r);
  printf ("%s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
