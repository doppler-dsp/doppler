/**
 * spectrogram_demo.c — a waterfall from a stream that arrives in pieces.
 *
 * A receiver hands a spectrogram whatever it has: a socket's datagrams, a
 * file reader's blocks, a ring's frames. Here the stream is a tone that hops
 * between four frequencies, made by the library's own LO, and it arrives in
 * chunks of 1, 37, 700 and 5 samples -- none of them a frame. The spectrogram
 * turns it into rows of NFFT bins, one every HOP samples, and the rows are
 * the same however the stream was cut.
 *
 * Who owns the output buffer is the caller's choice, and there are two
 * honest ones; this program does both and requires them to agree bit for bit:
 *
 *   1. size every push with dp_spectrogram_push_max_out(): the push then
 *      takes its whole chunk, every time (this is what a binding does);
 *   2. keep one fixed buffer with room for a few rows: a push stops at a
 *      whole row when it is full, and dp_spectrogram_consumed() says where
 *      to resume -- nothing is ever dropped.
 *
 * The rows are linear power, the default: a full-scale tone on a bin reads
 * 1.0. A display converts to dB only the bins it draws, and so does this
 * program: every row that lies inside one frequency segment is checked
 * physically, its peak on the bin of that segment's tone at 0 dBFS, and
 * that one bin is the only one it converts. Converting every bin of every
 * row is most of a dB row's cost (docs/design/spectrogram-measurements.md,
 * entry 5.8), which is why dB rows are asked for by name. Then
 * dp_spectrogram_flush() ends the stream with the one zero-padded row the
 * last samples still owe.
 *
 * Build:
 *   make build
 *   ./build/native/examples/spectrogram_demo
 */
#include "doppler/lo/lo_core.h"
#include "doppler/spectrogram/spectrogram_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

enum
{
  NFFT  = 256,  /* bins per row, and samples per frame */
  HOP   = 64,   /* a row every 64 samples: 75% overlap */
  SEG   = 2048, /* samples at each frequency           */
  NSEG  = 4,    /* frequencies the tone hops between    */
  TAIL  = 100,  /* a ragged end, so the stream owes a last row */
  TOTAL = NSEG * SEG + TAIL,
  ROOM  = 3 /* rows the fixed buffer of way 2 can hold */
};

/* The tone's bin in each segment (negative: below the carrier). */
static const int BIN[NSEG] = { 20, -50, 90, 5 };

/* The chunk sizes the "socket" delivers, cycled. */
static const size_t CHUNK[] = { 1, 37, 700, 5 };
#define NCHUNK (sizeof CHUNK / sizeof *CHUNK)

/* A power bin in dBFS: the conversion a display makes for what it draws. */
static float
to_db (float power)
{
  return 10.0f * log10f (power);
}

/* Peak index of one row (power and its dB rank the bins alike). */
static size_t
peak (const float *row)
{
  size_t p = 0;
  for (size_t i = 1; i < NFFT; i++)
    if (row[i] > row[p])
      p = i;
  return p;
}

int
main (void)
{
  /* The stream: the LO at each segment's frequency in turn. */
  float _Complex *x  = (float _Complex *)malloc (TOTAL * sizeof *x);
  dp_lo_state_t  *lo = dp_lo_create (0.0);
  CHECK (x != NULL && lo != NULL);
  for (int s = 0; s < NSEG; s++)
    {
      dp_lo_set_norm_freq (lo, (double)BIN[s] / NFFT);
      dp_lo_steps (lo, SEG, x + s * SEG, SEG);
    }
  dp_lo_steps (lo, TAIL, x + NSEG * SEG, TAIL);
  dp_lo_destroy (lo);

  /* Hann window, power rows (the default), DC-centred (bin k at
     NFFT/2 + k). */
  dp_spectrogram_state_t *a
      = dp_spectrogram_create (NFFT, HOP, 0, 0.0f, DP_SPECTROGRAM_POWER);
  dp_spectrogram_state_t *b
      = dp_spectrogram_create (NFFT, HOP, 0, 0.0f, DP_SPECTROGRAM_POWER);
  CHECK (a != NULL && b != NULL);

  /* rows_for is exact: how many rows the whole stream makes, before a
     sample of it has been pushed. */
  const size_t rows = dp_spectrogram_rows_for (a, TOTAL);
  CHECK (rows == (TOTAL - NFFT) / HOP + 1);
  float *fall_a = (float *)malloc (rows * NFFT * sizeof *fall_a);
  float *fall_b = (float *)malloc (rows * NFFT * sizeof *fall_b);
  CHECK (fall_a != NULL && fall_b != NULL);

  /* Way 1: room for exactly what this chunk completes. Often that is no
     room at all -- a 1-sample chunk rarely completes a row -- and the push
     still takes the whole chunk, into the carry. */
  size_t made_a = 0, off = 0;
  for (size_t k = 0; off < TOTAL; k++)
    {
      size_t n = CHUNK[k % NCHUNK];
      if (n > TOTAL - off)
        n = TOTAL - off;
      size_t room = dp_spectrogram_push_max_out (a, n);
      made_a += dp_spectrogram_push (a, x + off, n, fall_a + made_a, room);
      CHECK (dp_spectrogram_consumed (a) == n);
      off += n;
    }

  /* Way 2: one buffer, ROOM rows. A 700-sample chunk completes about ten
     rows, so the push stops when the buffer is full; the caller copies the
     rows out and offers the rest of the chunk again. */
  float  buf[ROOM * NFFT];
  size_t made_b = 0, short_pushes = 0;
  off = 0;
  for (size_t k = 0; off < TOTAL; k++)
    {
      size_t n = CHUNK[k % NCHUNK];
      if (n > TOTAL - off)
        n = TOTAL - off;
      for (size_t done = 0; done < n;)
        {
          size_t w = dp_spectrogram_push (b, x + off + done, n - done, buf,
                                          sizeof buf / sizeof *buf);
          memcpy (fall_b + made_b, buf, w * sizeof *buf);
          made_b += w;
          size_t took = dp_spectrogram_consumed (b);
          short_pushes += took < n - done;
          done += took;
        }
      off += n;
    }

  /* The same rows either way, and every one of them. */
  CHECK (made_a == rows * NFFT && made_b == rows * NFFT);
  CHECK (memcmp (fall_a, fall_b, rows * NFFT * sizeof *fall_a) == 0);
  CHECK (short_pushes > 0); /* way 2 really did run out of room */

  /* Physics: a row inside one segment peaks on that segment's tone, at the
     tone's true level: 1.0 in power, 0 dBFS once that one bin is converted.
     A row straddling a hop holds two tones; skip it. */
  size_t checked = 0;
  for (size_t r = 0; r < rows; r++)
    {
      size_t first = r * HOP, last = first + NFFT - 1;
      if (first / SEG != last / SEG || last >= NSEG * SEG)
        continue;
      const float *row = fall_a + r * NFFT;
      size_t       p   = peak (row);
      CHECK (p == (size_t)(NFFT / 2 + BIN[first / SEG]));
      CHECK (fabsf (to_db (row[p])) < 0.01f); /* 0 dBFS */
      checked++;
    }
  CHECK (checked > rows / 2);

  /* End of stream: the samples after the last full row are still owed a
     row, on the hop grid (it starts at rows * HOP, zero-padded past the
     end). Both ways owe the same one. */
  float last_a[NFFT], last_b[NFFT];
  CHECK (dp_spectrogram_pending (a) > 0);
  CHECK (dp_spectrogram_flush (a, last_a) == NFFT);
  CHECK (dp_spectrogram_flush (b, last_b) == NFFT);
  CHECK (memcmp (last_a, last_b, sizeof last_a) == 0);
  /* Physics for the flushed row too: it starts at rows * HOP, inside the
     last segment, whose tone runs on to the end of the stream; the zeros
     past the end fall under the window's last NFFT - (TOTAL - rows * HOP)
     taps, Hann's smallest. So it peaks on that tone, just under 0 dBFS. */
  CHECK (rows * HOP >= (NSEG - 1) * SEG);
  size_t lp = peak (last_a);
  CHECK (lp == (size_t)(NFFT / 2 + BIN[NSEG - 1]));
  CHECK (to_db (last_a[lp]) <= 0.0f && to_db (last_a[lp]) > -0.5f);
  CHECK (dp_spectrogram_flush (a, last_a) == 0); /* the stream is over */

  printf ("spectrogram: %d samples in chunks of 1/37/700/5 -> %zu rows of "
          "%d bins (hop %d) + 1 flushed\n",
          TOTAL, rows, NFFT, HOP);
  printf ("  %zu single-tone rows peak on their tone at 0 dBFS; a %d-row "
          "buffer that ran out of room %zu times gave the same rows\n",
          checked, ROOM, short_pushes);

  dp_spectrogram_destroy (a);
  dp_spectrogram_destroy (b);
  free (fall_a);
  free (fall_b);
  free (x);
  return 0;
}
