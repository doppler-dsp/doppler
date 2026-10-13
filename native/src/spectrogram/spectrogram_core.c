/*
 * spectrogram_core.c — the streaming spectrogram: the ring's framed face
 * feeding PSD's per-frame kernel, and nothing of its own between them.
 *
 * The contract is the header's; the design is docs/design/spectrogram.md.
 * Everything that makes a frame a spectrum is PSD's per-frame kernel --
 * dp_psd_frame_linear() for power rows, dp_psd_frame_db() for dB rows --
 * and everything that makes a stream frames is the framer. What is here is the
 * loop that joins them and the room check that keeps a short output buffer
 * from losing input. Rows are DC-centred exactly as the kernel emits them.
 */
#include "doppler/spectrogram/spectrogram_core.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The ring holds the carry (< nfft samples once a push returns) plus what one
 * feed admits. Twice a frame lets a feed take at least one whole frame's worth
 * past the carry, so a push loops once per few rows rather than once per
 * sample. A larger ring can win little: the whole carry, its copy included,
 * costs 0.2-3.3% of a sample against a loop with none (the design's U1,
 * docs/design/spectrogram-measurements.md §5.6). */
#define SPECTROGRAM_RING_FRAMES 2u

/* One frame -> one row, through the one kernel, DC-centred as it emits it.
 * The two modes are the kernel's two readings of one normalised power, so
 * they share the window, the FFT and the dBFS reference: a power row is the
 * quotient, a dB row its 10*log10 with the -200 dB floor. dB is the case
 * tested for, because it is the one a caller has to ask for. */
static void
spectrogram_row (dp_spectrogram_state_t *s, const float _Complex *frame,
                 float *row)
{
  if (s->mode == DP_SPECTROGRAM_DB)
    dp_psd_frame_db (&s->k, frame, row);
  else
    dp_psd_frame_linear (&s->k, frame, row);
}

dp_spectrogram_state_t *
dp_spectrogram_create (size_t nfft, size_t hop, int window, float beta,
                       int mode)
{
  /* window is validated by dp_psd_kernel_init, the one owner of the index.
     A hop of 0 means nfft, PSD's own rule (dp_psd_create_hop); nfft 0
     leaves it 0, and that is refused with the rest of the range. */
  if (hop == 0)
    hop = nfft;
  if (hop == 0 || hop > nfft)
    return NULL;
  if (mode != DP_SPECTROGRAM_POWER && mode != DP_SPECTROGRAM_DB)
    return NULL;
  if (nfft > SIZE_MAX / SPECTROGRAM_RING_FRAMES)
    return NULL;

  /* The kernel alone, not a PSD: a row is one frame's spectrum, so PSD's
   * averager, dB buffer, ring and framer would be dead weight here. n = nfft
   * and pad = 1, full scale 1: rows are against the full scale of a
   * unit-amplitude cf32 stream (1.0 linear, 0 dBFS). A row has the kernel's
   * transform length of bins, so that has to BE nfft: the kernel owns the
   * transform length (it zero-pads n to a power of two), and an nfft it
   * would pad is refused here rather than given rows wider than its
   * frames. */
  dp_psd_kernel_t k;
  if (dp_psd_kernel_init (&k, nfft, window, beta, 1, 1.0) != DP_OK)
    return NULL;
  if (k.nfft != nfft)
    {
      dp_psd_kernel_free (&k);
      return NULL;
    }

  dp_spectrogram_state_t *s
      = (dp_spectrogram_state_t *)dp_xcalloc (1, sizeof *s);
  s->k = k;
  s->ring
      = (dp_f32_t *)dp_xnn (dp_f32_create (SPECTROGRAM_RING_FRAMES * nfft));
  s->last   = (float _Complex *)dp_xmalloc (nfft * sizeof *s->last);
  s->nfft   = nfft;
  s->hop    = hop;
  s->window = window;
  s->beta   = beta;
  s->mode   = mode;
  /* Cannot refuse: the ring is new (so empty), its capacity is past nfft,
   * and 1 <= hop <= nfft was checked above. The status is still read, as
   * the framer requires. */
  if (dp_f32_framer_init (&s->fr, s->ring, nfft, hop) != DP_OK)
    {
      dp_spectrogram_destroy (s);
      return NULL;
    }
  return s;
}

void
dp_spectrogram_destroy (dp_spectrogram_state_t *s)
{
  if (!s)
    return;
  dp_psd_kernel_free (&s->k);
  if (s->ring)
    dp_f32_destroy (s->ring);
  free (s->last);
  free (s);
}

void
dp_spectrogram_reset (dp_spectrogram_state_t *s)
{
  dp_f32_framer_reset (&s->fr);
  s->consumed = 0;
}

size_t
dp_spectrogram_push (dp_spectrogram_state_t *s, const float _Complex *in,
                     size_t n_in, float *out, size_t max_out)
{
  const size_t nfft  = s->nfft;
  const size_t room  = max_out / nfft; /* whole rows only */
  size_t       taken = 0, rows = 0;

  /* feed admits only what completes at most the rows still free -- once the
   * room is used, only what completes none, which is the carry -- and the
   * drain takes every row it completed. So the framer is drained whenever
   * this loop looks (the carry is < nfft, and flush / get_state are always
   * valid), and input is left untaken only when the next row is due and
   * there is no room for it: a feed then takes nothing, and the loop ends.
   * Taking the carry even with no room left is what makes push_max_out a
   * capacity that takes ALL of n_in: an input that completes no row needs
   * none. */
  while (taken < n_in)
    {
      size_t took = dp_f32_framer_feed_view (&s->fr, in + taken, n_in - taken,
                                             room - rows);
      taken += took;
      const float _Complex *frame;
      while ((frame = dp_f32_framer_next_view (&s->fr)) != NULL)
        {
          spectrogram_row (s, frame, out + rows * nfft);
          rows++;
        }
      if (!took)
        break;
    }
  s->consumed = taken;
  return rows * nfft;
}

size_t
dp_spectrogram_rows_for (const dp_spectrogram_state_t *s, size_t n_in)
{
  return dp_f32_framer_frames_in (&s->fr, n_in);
}

size_t
dp_spectrogram_push_max_out (const dp_spectrogram_state_t *s, size_t n_in)
{
  size_t rows = dp_spectrogram_rows_for (s, n_in);
  return rows > SIZE_MAX / s->nfft ? SIZE_MAX : rows * s->nfft;
}

size_t
dp_spectrogram_consumed (const dp_spectrogram_state_t *s)
{
  return s->consumed;
}

int64_t
dp_spectrogram_flush (dp_spectrogram_state_t *s, float *row, size_t row_len)
{
  /* A row buffer that is not exactly one row is refused before anything
   * moves: -1, so a wrong-shape row cannot read as "nothing owed" (0). The
   * owed frame stays owed for a retry. */
  if (row_len != s->nfft)
    return -1;
  /* push leaves the framer drained, so the framer's flush cannot refuse
   * here; it either copies the owed, zero-padded frame or owes none. Both
   * restart the stream. */
  int owed    = dp_f32_framer_flush_view (&s->fr, s->last);
  s->consumed = 0;
  if (owed != 1)
    return 0;
  spectrogram_row (s, s->last, row);
  return (int64_t)s->nfft;
}

size_t
dp_spectrogram_flush_max_out (const dp_spectrogram_state_t *s)
{
  return s->nfft;
}

size_t
dp_spectrogram_pending (const dp_spectrogram_state_t *s)
{
  return dp_f32_framer_pending (&s->fr);
}

/* Serializable state: the configuration the rows depend on, then the carry,
 * which is the framer's snapshot, nested in the spectrogram's own envelope.
 * The configuration (mode, nfft, hop, window, beta) is written so a restore
 * can REFUSE a blob whose rows would mean something else (#2102): a power
 * blob never loads into a dB object, and nothing is written on a refusal.
 * The kernel's plan and scratch are not state and are rebuilt by create(). */
#define SPECTROGRAM_CONFIG_BYTES (4u + 8u + 8u + 4u + 4u)

size_t
dp_spectrogram_state_bytes (const dp_spectrogram_state_t *s)
{
  return sizeof (dp_state_hdr_t) + SPECTROGRAM_CONFIG_BYTES
         + dp_f32_framer_state_bytes (&s->fr);
}

void
dp_spectrogram_get_state (const dp_spectrogram_state_t *s, void *blob)
{
  DP_GET_OPEN (SPECTROGRAM_STATE_MAGIC, SPECTROGRAM_STATE_VERSION,
               dp_spectrogram_state_bytes (s));
  dp_w_u32 (&_w, (uint32_t)s->mode);
  dp_w_u64 (&_w, (uint64_t)s->nfft);
  dp_w_u64 (&_w, (uint64_t)s->hop);
  dp_w_u32 (&_w, (uint32_t)s->window);
  dp_w_f32 (&_w, &s->beta, 1);
  DP_W_CHILD (&_w, dp_f32_framer, &s->fr);
}

int
dp_spectrogram_set_state (dp_spectrogram_state_t *s, const void *blob)
{
  DP_SET_OPEN (SPECTROGRAM_STATE_MAGIC, SPECTROGRAM_STATE_VERSION,
               dp_spectrogram_state_bytes (s));
  /* Read the configuration and refuse a mismatch before anything is written:
   * the object is unchanged on a refusal. */
  const uint32_t mode   = dp_r_u32 (&_r);
  const uint64_t nfft   = dp_r_u64 (&_r);
  const uint64_t hop    = dp_r_u64 (&_r);
  const uint32_t window = dp_r_u32 (&_r);
  float          beta   = 0.0f;
  dp_r_f32 (&_r, &beta, 1);
  /* beta only shapes the Kaiser window; elsewhere it does not change a row,
   * so it is not compared there. */
  const int beta_matters = s->window == DP_PSD_WINDOW_KAISER;
  if ((int)mode != s->mode || nfft != (uint64_t)s->nfft
      || hop != (uint64_t)s->hop || (int)window != s->window
      || (beta_matters && beta != s->beta))
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, dp_f32_framer, &s->fr);
  s->consumed = 0;
  return DP_OK;
}
