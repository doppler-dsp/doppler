/* bench_ccsds_tm_core.c — the CCSDS transmit chain, stage by stage.
 *
 * `ccsds_tm_frame_encode` runs four stages over a transfer frame: the outer
 * RS code, the section-10 randomiser, the ASM, and the section-3 inner
 * convolutional code. A single "frames per second" number would hide which
 * of them costs anything, and the answer is not guessable -- the randomiser
 * is an XOR per bit, the ASM is 32 bits per frame, and the two codes are
 * doing real arithmetic in different fields.
 *
 * So the encode row is measured against its own stages:
 *
 *   frame_encode[full]     everything on: RS + randomise + ASM + conv
 *   frame_encode[rs+rand]  the inner code off -- the difference is what
 *                          convolutional encoding costs in this chain
 *   frame_encode[rand]     no outer code either -- the difference is RS
 *   randomise              the section-10 sequence alone, over the frame
 *   rs_encode_block        the interleaved outer code alone, depth 5
 *
 * and the receive side gets the two that a demodulator actually runs per
 * captured second:
 *
 *   asm_find               the CADU sync search. This is the one that runs
 *                          over EVERY received bit, not per frame, so its
 *                          per-bit cost is the number that scales with
 *                          sample rate
 *   rs_decode_block        the outer decode, depth 5, clean codewords
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "ccsds_tm/ccsds_tm.h"
#include "ccsds_tm/ccsds_tm_frame.h"
#include "ccsds_tm/ccsds_tm_rs.h"
#include "conv/conv_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Depth 5 is the CCSDS default interleave, and 223*5 = 1115 bytes is the
   transfer frame that goes with it. */
#define DEPTH 5
#define FRAME_LEN ((size_t)CCSDS_TM_RS_K * DEPTH)
#define BLOCK_LEN ((size_t)CCSDS_TM_RS_N * DEPTH)
#define FRAMES 64
#define ITERATIONS 50

/* The sync search is per received BIT, so it gets its own, larger block:
   one CADU's worth of bits many times over, which is what a demodulator
   hands it. */
#define SEARCH_BITS 262144

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

static void
report_frame (const char *name, const double *t)
{
  double s = min_sec (t, ITERATIONS) / (double)FRAMES;
  printf ("  %-24s %8.2f us/frame  %8.2f Mbit/s (info)\n", name, s * 1e6,
          (double)FRAME_LEN * 8.0 / s / 1e6);
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  uint8_t *frame = malloc (FRAME_LEN);
  if (!frame)
    return 1;

  uint32_t lfsr = 0x7FFFu;
  for (size_t i = 0; i < FRAME_LEN; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      frame[i] = (uint8_t)(lfsr & 0xFFu);
    }

  printf ("=== ccsds_tm benchmark ===\n");
  printf ("frame = %zu bytes (RS(255,223) x depth %d), %d frames/round, "
          "%d rounds\n\n",
          FRAME_LEN, DEPTH, FRAMES, ITERATIONS);

  /* Three configurations, peeled one stage at a time. Each needs its own
     output buffer size, so ask the layout rather than computing it here --
     that is what ccsds_tm_frame_layout is for, and a hand-computed size is
     the kind of thing that silently goes wrong when the standard's
     defaults move. */
  const ccsds_tm_frame_cfg_t cfgs[3] = {
    { .rs_depth = DEPTH, .randomise = 1, .attach_asm = 1, .convolutional = 1 },
    { .rs_depth = DEPTH, .randomise = 1, .attach_asm = 1, .convolutional = 0 },
    { .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0 },
  };
  const char *const names[3] = { "frame_encode[full]", "frame_encode[rs+rand]",
                                 "frame_encode[rand]" };
  static double     t_enc[3][ITERATIONS];

  for (int c = 0; c < 3; c++)
    {
      size_t   max_out = ccsds_tm_frame_layout (&cfgs[c], FRAME_LEN, NULL);
      uint8_t *out     = malloc (max_out);
      if (!out)
        return 1;

      conv_enc_t conv;
      conv_enc_init (&conv);
      if (ccsds_tm_frame_encode (&cfgs[c], &conv, frame, FRAME_LEN, out,
                                 max_out)
          == 0)
        {
          (void)fprintf (stderr, "bench_ccsds_tm: %s encoded nothing\n",
                         names[c]);
          return 1;
        }

      for (int r = 0; r < ITERATIONS; r++)
        {
          conv_enc_init (&conv);
          clock_gettime (CLOCK_MONOTONIC, &t0);
          for (int f = 0; f < FRAMES; f++)
            sink += ccsds_tm_frame_encode (&cfgs[c], &conv, frame, FRAME_LEN,
                                           out, max_out);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_enc[c][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, names[c], t_enc[c], ITERATIONS, FRAMES);
      report_frame (names[c], t_enc[c]);
      free (out);
    }

  /* The randomiser alone, over the frame's bits. */
  uint8_t      *bits = malloc (FRAME_LEN * 8);
  static double t_rand[ITERATIONS];
  if (!bits)
    return 1;
  for (size_t i = 0; i < FRAME_LEN * 8; i++)
    bits[i] = (uint8_t)((frame[i / 8] >> (7 - (i % 8))) & 1u);
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int f = 0; f < FRAMES; f++)
        ccsds_tm_randomise (bits, FRAME_LEN * 8);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_rand[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "randomise", t_rand, ITERATIONS, FRAMES);
  report_frame ("randomise", t_rand);

  /* The outer code alone, both directions, at the same depth. */
  uint8_t      *block = malloc (BLOCK_LEN);
  uint8_t      *rxblk = malloc (BLOCK_LEN);
  static double t_rse[ITERATIONS], t_rsd[ITERATIONS];
  if (!block || !rxblk)
    return 1;

  ccsds_tm_rs_encode_block (frame, DEPTH, block);
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int f = 0; f < FRAMES; f++)
        sink += ccsds_tm_rs_encode_block (frame, DEPTH, block);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_rse[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "rs_encode_block", t_rse, ITERATIONS, FRAMES);
  report_frame ("rs_encode_block", t_rse);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int f = 0; f < FRAMES; f++)
        {
          memcpy (rxblk, block, BLOCK_LEN);
          sink += ccsds_tm_rs_decode_block (rxblk, DEPTH, NULL);
        }
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_rsd[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "rs_decode_block", t_rsd, ITERATIONS, FRAMES);
  report_frame ("rs_decode_block", t_rsd);

  /* The sync search, per received bit. */
  uint8_t      *stream = malloc (SEARCH_BITS);
  static double t_asm[ITERATIONS];
  if (!stream)
    return 1;
  lfsr = 0x1234u;
  for (size_t i = 0; i < SEARCH_BITS; i++)
    {
      lfsr      = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      stream[i] = (uint8_t)(lfsr & 1u);
    }
  /* Plant a real marker so the search does the work of finding one rather
     than the work of scanning to the end and giving up. */
  ccsds_tm_asm_bits (stream + SEARCH_BITS / 2);

  ccsds_tm_asm_hit_t hit;
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += (size_t)ccsds_tm_asm_find (stream, SEARCH_BITS, 2u, &hit);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_asm[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "asm_find", t_asm, ITERATIONS, SEARCH_BITS);
  printf ("  %-24s %8.2f ns/bit   %8.2f Mbit/s scanned\n", "asm_find",
          min_sec (t_asm, ITERATIONS) / (double)SEARCH_BITS * 1e9,
          (double)SEARCH_BITS / min_sec (t_asm, ITERATIONS) / 1e6);

  printf (
      "\n  conv encoding is %.0f%% of a full frame_encode; the outer\n"
      "  code is %.0f%%. asm_find is the only row that scales with the\n"
      "  SAMPLE rate rather than the frame rate -- everything else is\n"
      "  paid once per %zu-byte frame.\n",
      100.0
          * (1.0
             - min_sec (t_enc[1], ITERATIONS)
                   / min_sec (t_enc[0], ITERATIONS)),
      100.0 * (min_sec (t_enc[1], ITERATIONS) - min_sec (t_enc[2], ITERATIONS))
          / min_sec (t_enc[0], ITERATIONS),
      FRAME_LEN);

  (void)sink;
  free (frame);
  free (bits);
  free (block);
  free (rxblk);
  free (stream);
  jm_bench_write_json (&_bench, "ccsds_tm");
  return 0;
}
