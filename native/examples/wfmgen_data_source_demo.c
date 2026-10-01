/**
 * @file wfmgen_data_source_demo.c
 * @brief A payload drawn from a data source, frame by frame.
 *
 * The C twin of `src/doppler/examples/wfmgen_data_source_demo.py`. A message
 * is split into `data_len`-bit frames, ONE chunk per frame, each with a
 * CRC-16 over its own chunk -- not one frame cycled. The last chunk is short,
 * so it is padded from `fill`, and the run's length is the frames', derived
 * rather than given (docs/design/payload-data-source.md). The source's
 * `data` is the message's bits; `data_from_file` would read them from a
 * file, or `-` for stdin, instead.
 *
 * What it shows, in order:
 *
 *  1. the message comes back frame by frame, each frame the next chunk, the
 *     last padded with the fill;
 *  2. each frame's CRC-16 covers ITS chunk;
 *  3. the run is exactly the frames, `ceil(bits / data_len)` of them, with
 *     no `num_samples` given.
 *
 * Every claim is a check; the program exits non-zero if any fails.
 *
 * Build and run (from the repo root, after `make build`):
 *
 *     ./build/native/examples/wfmgen_data_source_demo
 */
#include "doppler/dp_complex.h"
#include "doppler/dp_crc16.h"
#include <doppler/cvt/cvt_core.h>
#include <doppler/wfm/wfm_compose.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPS 4
#define LEN 96           /* data bits per frame */
#define FRAME (LEN + 16) /* then a CRC-16 over them */

static int failures = 0;

static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

int
main (void)
{
  static const char msg[] = "a payload drawn from a data source, frame by "
                            "frame";
  const size_t      nmsg  = sizeof msg - 1u;
  uint8_t          *bits  = malloc (8u * nmsg);
  if (!bits)
    return 1;
  (void)dp_bytes_to_bin ((const uint8_t *)msg, nmsg, bits, 8u * nmsg,
                         DP_BITORDER_BIG);
  static const uint8_t f01[2] = { 0, 1 };

  wfm_source_t src = { 0 }; /* named members, never positional */
  src.type         = WFM_SYNTH_BPSK;
  src.sps          = SPS;
  src.snr          = 100.0;
  src.seed         = 1;
  src.pn_length    = 7;
  src.crc          = 1;
  src.data
      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = bits, .len = 8u * nmsg };
  src.data_len = LEN;
  src.fill     = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = f01, .len = 2 };
  wfm_segment_t seg = { .sources = &src, .n_sources = 1, .fs = 1e6 };

  const char *why = dp_wfm_scene_error (&seg, 1, 0, 0);
  if (why)
    {
      fprintf (stderr, "refused: %s\n", why);
      return 1;
    }
  dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
  if (!c)
    return 1;

  const size_t   frames = (8u * nmsg + LEN - 1u) / LEN;
  const size_t   total  = frames * FRAME * SPS;
  float complex *x      = malloc ((total + 64u) * sizeof *x);
  if (!x)
    return 1;
  const size_t n = dp_wfm_compose_execute (c, x, total + 64u);
  printf ("%zu bits -> %zu frames of %d bits, %zu samples\n", 8u * nmsg,
          frames, FRAME, n);
  check (n == total, "3. the run is exactly the frames, no num_samples");

  /* BPSK at the symbol centre: bit 1 is -1. */
  uint8_t *rx = malloc (frames * FRAME);
  if (!rx)
    return 1;
  for (size_t i = 0; i < frames * FRAME; i++)
    rx[i] = crealf (x[i * SPS + SPS / 2]) < 0.0f;

  int chunks = 1, crcs = 1, padded = 1;
  for (size_t f = 0; f < frames; f++)
    {
      const uint8_t *frame = rx + f * FRAME;
      uint8_t        want[LEN];
      for (size_t i = 0; i < LEN; i++)
        {
          const size_t k = f * LEN + i;
          want[i]        = k < 8u * nmsg ? bits[k] : f01[(k - 8u * nmsg) % 2];
          if (k >= 8u * nmsg && frame[i] != want[i])
            padded = 0;
        }
      if (memcmp (frame, want, LEN) != 0)
        chunks = 0;
      unsigned got = 0;
      for (size_t i = 0; i < 16; i++)
        got = (got << 1) | frame[LEN + i];
      if (got != dp_crc16_ccitt (want, LEN))
        crcs = 0;
    }
  check (chunks, "1. every frame carries the next chunk, in order");
  check (padded, "1. the last frame's spare bits are the fill");
  check (crcs, "2. each frame's CRC-16 covers its own chunk");

  dp_wfm_compose_destroy (c);
  free (rx);
  free (x);
  free (bits);
  printf ("%s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
