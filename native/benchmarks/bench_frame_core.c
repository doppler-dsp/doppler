/* bench_frame_core.c — materialising a frame, and checking one.
 *
 * This file was a jm scaffold until now: a `TODO: benchmark this component`
 * and no `jm_bench_add` call, so it built, ran, and wrote
 * `"benchmarks": []`. `frame` IS one of jm's components, so `jm bench` was
 * running it faithfully every time and collecting nothing -- which is why
 * scripts/check_bench_coverage.py now fails a benchmark that records no
 * measurement, and not only a component that has no benchmark file.
 *
 * What a caller pays:
 *
 *   bits[1]        materialise one frame's bits -- preamble repeats, the
 *                  sync word, the payload, the CRC
 *   bits[16]       sixteen frames in one call, which is how a waveform
 *                  generator asks. Divided by 16 in the report, so the two
 *                  rows are directly comparable and the difference is the
 *                  per-call overhead a caller avoids by batching
 *   crc_ok         the receive-side check, per frame
 *
 * The frame here is a 1024-bit payload behind a 64-bit sync word with a
 * 32-bit preamble repeated four times and a CRC -- a plausible small
 * telemetry frame rather than a degenerate one, because a frame with an
 * empty payload would measure the call and not the copy.
 *
 * The data-source rows (docs/design/payload-data-source.md §6) time the
 * per-frame path a `data:LEN` frame takes in wfmgen: one chunk pulled from
 * a data source (dp_wfm_data_frame, paced), then the frame assembled
 * around it (dp_wfm_frame_assemble_data) -- what the synth's refill does at
 * every frame boundary (wfm_synth_bridge.c, data_pull_refill). LEN is 1784
 * bits, a CADU's 223-octet payload, in two frames:
 *
 *   plain   asm | data:1784 | crc16                       1832 bits
 *   cadu    asm | data:1784 | rs_parity, RS(255,223) I=1,
 *           randomised (10.4.1), conv r1/2 over all       4144 bits
 *
 *   data pn:0       one chunk from an endless pn:0 stream
 *   data pipe       one chunk from a pipe already holding it: poll(0), one
 *                   read of 223 octets, unpack, hash -- the read cost
 *   assemble plain  the plain frame around a fixed chunk
 *   assemble cadu   the cadu frame around a fixed chunk (RS + conv)
 *   pipe+cadu       the two together: the whole per-frame cost of a coded
 *                   frame fed from stdin
 *
 * Each round is FRAMES frames, so the per-frame number sits far above the
 * clock's resolution. The pipe is refilled between rounds, untimed.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "doppler/ccsds_tm/ccsds_tm.h"
#include "doppler/ccsds_tm/ccsds_tm_frame.h"
#include "doppler/ccsds_tm/ccsds_tm_rs.h"
#include "doppler/frame/frame_core.h"
#include "doppler/wfm/wfm_data.h"
#include "doppler/wfm/wfm_frame.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The pipe calls the data-source rows make, per platform: the source reads
   a Windows anonymous pipe through PeekNamedPipe, and test_wfm_data.c
   drives it the same way. */
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define pipe(p) _pipe ((p), 65536, _O_BINARY)
static long long
write_shim (int f, const void *b, size_t n)
{
  return _write (f, b, (unsigned)n);
}
#define write write_shim
#define close(f) _close (f)
#else
#include <unistd.h>
#endif

#define N_PRE 32
#define PRE_REPS 4
#define N_SYNC 64
#define N_PAY 1024
#define BATCH 16
#define ITERATIONS 200

#define LEN 1784u          /* a CADU's payload, 223 octets     */
#define LEN_OCT (LEN / 8u) /* octets a pipe delivers per frame */
#define FRAMES 32          /* frames per data-source round     */

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/* asm | data:LEN | rs_parity, RS I=1, randomised, conv r1/2 over all: the
   CADU of CCSDS 131.0-B, its payload a data source's chunk -- built the way
   test_wfm_compose.c's coded_frame builds one. */
static int
cadu_frame (wfm_frame_desc_t *d)
{
  static uint8_t  marker[CCSDS_TM_ASM_BITS];
  const wfm_seq_t m
      = { .kind = WFM_SEQ_LITERAL, .bits = marker, .len = CCSDS_TM_ASM_BITS };
  const wfm_seq_t q = { .kind = WFM_SEQ_DATA, .len = LEN };
  int             st;
  dp_ccsds_tm_asm_bits (marker);
  memset (d, 0, sizeof *d);
  if (dp_wfm_frame_add_field (d, "asm", &m, 0u) < 0
      || dp_wfm_frame_add_field (d, "payload", &q, 0u) < 0
      || dp_wfm_frame_add_derived (d, "rs_parity", CCSDS_TM_RS_2E * 8u) < 0)
    return -1;
  st = dp_wfm_frame_add_stage (d, WFM_STAGE_RS, "payload", "rs_parity");
  if (st < 0)
    return -1;
  d->stage[st].depth = 1u;
  st = dp_wfm_frame_add_stage (d, WFM_STAGE_RANDOMISE, "payload", "rs_parity");
  if (st < 0)
    return -1;
  d->stage[st].depth = 1u; /* 10.4.1 */
  st = dp_wfm_frame_add_stage (d, WFM_STAGE_CONV, "asm", "rs_parity");
  if (st < 0)
    return -1;
  d->stage[st].emit_num = 2u; /* rate 1/2 */
  d->stage[st].emit_den = 1u;
  return 0;
}

/* Put FRAMES chunks' octets into the pipe, untimed. 0, or 1 on a short
   write. */
static int
refill_pipe (int fd, const uint8_t *oct)
{
  const size_t n = (size_t)FRAMES * LEN_OCT;
  return write (fd, oct, n) == (long long)n ? 0 : 1;
}

static void
report (const char *name, const double *t)
{
  const double us = min_sec (t, ITERATIONS) / FRAMES * 1e6;
  printf ("  %-16s %9.3f us/frame  %10.0f frames/s\n", name, us, 1e6 / us);
}

/* One data-source row: FRAMES frames a round, each pulled from `src` (when
   given) and assembled into `d` (when given). The pipe, when given, is
   refilled before each round, outside the clock. Returns 0, or 1 when a
   frame was not built -- a frame that is not built is not a measurement. */
static int
time_rows (jm_bench_t *b, const char *name, wfm_data_src_t *src, int pipe_wr,
           const uint8_t *oct, const wfm_frame_desc_t *d,
           const wfm_frame_ops_t *ops, uint8_t *chunk, uint8_t *fr,
           size_t fr_bits)
{
  static double t[ITERATIONS];
  int           bad = 0;
  for (int r = 0; r < ITERATIONS; r++)
    {
      if (pipe_wr >= 0)
        bad |= refill_pipe (pipe_wr, oct);
      const uint64_t t0 = jm_bench_now_ns ();
      for (int f = 0; f < FRAMES; f++)
        {
          if (src)
            bad |= dp_wfm_data_frame (src, WFM_DATA_PACED, 1, chunk, LEN)
                   != WFM_DATA_FRAME;
          if (d)
            bad |= dp_wfm_frame_assemble_data (d, ops, chunk, fr, fr_bits)
                   != fr_bits;
        }
      t[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
    }
  jm_bench_add (b, name, t, ITERATIONS, FRAMES);
  report (name, t);
  return bad;
}

/* The data-source rows; see the header. Returns 0, or 1 on a setup or
   per-frame failure. */
static int
bench_data_source (jm_bench_t *b)
{
  static uint8_t   chunk[LEN], oct[(size_t)FRAMES * LEN_OCT];
  static uint8_t   marker[CCSDS_TM_ASM_BITS];
  wfm_frame_desc_t plain, cadu;
  wfm_frame_ops_t  ops;
  const char      *why = NULL;
  const wfm_seq_t  sync
      = { .kind = WFM_SEQ_LITERAL, .bits = marker, .len = CCSDS_TM_ASM_BITS };
  const wfm_seq_t         q = { .kind = WFM_SEQ_DATA, .len = LEN };
  wfm_frame_desc_layout_t lp, lc;
  int                     p[2], bad = 0;

  dp_ccsds_tm_asm_bits (marker);
  dp_ccsds_tm_frame_ops (&ops, NULL);
  if (dp_wfm_frame_fixed (&plain, NULL, 0, &sync, &q, 1) != 0
      || cadu_frame (&cadu) != 0 || dp_wfm_frame_desc_layout (&plain, &lp)
      || dp_wfm_frame_desc_layout (&cadu, &lc))
    return 1;
  for (size_t i = 0; i < sizeof oct; i++)
    oct[i] = (uint8_t)(i * 151u + 7u);
  for (size_t i = 0; i < LEN; i++)
    chunk[i] = (uint8_t)(oct[i / 8u] >> (7u - i % 8u) & 1u);
  uint8_t *fr = malloc (lc.out_bits);
  if (!fr || lp.out_bits > lc.out_bits)
    return free (fr), 1;

  printf ("\n=== data source -> frame (payload-data-source.md §6) ===\n");
  printf ("LEN = %u bits; plain = %zu bits, cadu = %zu bits on the air; "
          "%d frames x %d rounds\n\n",
          LEN, lp.out_bits, lc.out_bits, FRAMES, ITERATIONS);

  wfm_data_src_t *pn = dp_wfm_data_create ("pn:0:15", NULL, LEN, NULL, &why);
  if (!pn || pipe (p) != 0)
    return dp_wfm_data_destroy (pn), free (fr), 1;
  /* Every pull from the pipe finds its chunk already waiting, so each is
     one poll(0) and one read: the read cost, with no wait in it. */
  wfm_data_src_t *pp = dp_wfm_data_create_fd (p[0], LEN, "01", &why);
  if (!pp)
    bad = 1;
  else
    {
      bad |= time_rows (b, "data pn:0", pn, -1, NULL, NULL, NULL, chunk, fr,
                        0);
      bad |= time_rows (b, "data pipe", pp, p[1], oct, NULL, NULL, chunk, fr,
                        0);
      bad |= time_rows (b, "assemble plain", NULL, -1, NULL, &plain, &ops,
                        chunk, fr, lp.out_bits);
      bad |= time_rows (b, "assemble cadu", NULL, -1, NULL, &cadu, &ops, chunk,
                        fr, lc.out_bits);
      bad |= time_rows (b, "pipe+cadu", pp, p[1], oct, &cadu, &ops, chunk, fr,
                        lc.out_bits);
    }
  dp_wfm_data_destroy (pp);
  dp_wfm_data_destroy (pn);
  close (p[0]);
  close (p[1]);
  free (fr);
  if (bad)
    printf ("  a frame was not built: these rows are not measurements\n");
  return bad;
}

int
main (void)
{
  uint64_t        t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  /* The preamble is repeated IN ITS BITS -- a Frame takes bits, and a
     repetition is part of them (field_bits("…*4") builds the same). */
  static uint8_t pre[N_PRE * PRE_REPS], sync[N_SYNC], pay[N_PAY];
  uint32_t       lfsr = 0x51F0u;
  for (size_t i = 0; i < N_PRE * PRE_REPS; i++)
    pre[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < N_SYNC; i++)
    sync[i] = (uint8_t)((0x1ACFFC1Du >> (i % 32)) & 1u);
  for (size_t i = 0; i < N_PAY; i++)
    {
      lfsr   = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      pay[i] = (uint8_t)(lfsr & 1u);
    }

  dp_frame_state_t *f
      = dp_frame_create (pre, N_PRE * PRE_REPS, sync, N_SYNC, pay, N_PAY, 1);
  if (!f)
    return 1;

  const size_t nb1 = dp_frame_bits_max_out (f, 1);
  const size_t nbB = dp_frame_bits_max_out (f, BATCH);
  uint8_t     *out = malloc (nbB);
  if (!out)
    return 1;

  printf ("=== frame benchmark ===\n");
  printf ("frame = %zu bits (%d-bit preamble x%d, %d-bit sync, %d-bit "
          "payload, CRC), %d rounds\n\n",
          nb1, N_PRE, PRE_REPS, N_SYNC, N_PAY, ITERATIONS);

  static double t_one[ITERATIONS], t_bat[ITERATIONS], t_crc[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      sink += dp_frame_bits (f, 1, out, nb1);
      t1       = jm_bench_now_ns ();
      t_one[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "bits[1]", t_one, ITERATIONS, 1);
  printf ("  %-14s %9.3f us/frame  %8.2f Mbit/s\n", "bits[1]",
          min_sec (t_one, ITERATIONS) * 1e6,
          (double)nb1 / min_sec (t_one, ITERATIONS) / 1e6);

  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      sink += dp_frame_bits (f, BATCH, out, nbB);
      t1       = jm_bench_now_ns ();
      t_bat[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "bits[16]", t_bat, ITERATIONS, BATCH);
  printf ("  %-14s %9.3f us/frame  %8.2f Mbit/s\n", "bits[16]",
          min_sec (t_bat, ITERATIONS) / BATCH * 1e6,
          (double)nbB / min_sec (t_bat, ITERATIONS) / 1e6);

  dp_frame_bits (f, 1, out, nb1);
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      sink += (size_t)dp_frame_crc_ok (f, out, nb1);
      t1       = jm_bench_now_ns ();
      t_crc[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "crc_ok", t_crc, ITERATIONS, 1);
  printf ("  %-14s %9.3f us/frame\n", "crc_ok",
          min_sec (t_crc, ITERATIONS) * 1e6);

  printf ("\n  batching %d frames costs %.2f us/frame against %.2f us for\n"
          "  one -- the gap is per-call overhead, and it is what a generator\n"
          "  asking frame by frame pays for the convenience.\n",
          BATCH, min_sec (t_bat, ITERATIONS) / BATCH * 1e6,
          min_sec (t_one, ITERATIONS) * 1e6);

  const int bad = bench_data_source (&_bench);

  (void)sink;
  free (out);
  dp_frame_destroy (f);
  jm_bench_write_json (&_bench, "frame");
  return bad;
}
