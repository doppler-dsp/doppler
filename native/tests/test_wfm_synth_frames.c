/**
 * @file test_wfm_synth_frames.c
 * @brief The frame renderer: a synth of type WFM_SYNTH_FRAMES plays finished
 * frames of samples, each rendered from a frame of bits the bit refill drew
 * (dp_wfm_synth_attach_frames, docs/design/wlan.md, "The frame callback").
 *
 * The fixtures are two small things the test owns: a bit source whose bits
 * are a function of a counter (with its state triplet), and a renderer whose
 * samples are a function of its configuration key, the bits and the FRAME
 * INDEX, so a frame rendered with the wrong index, or the wrong bits, is a
 * different frame. The synth is clean (no noise, no carrier), so each output
 * sample is the renderer's.
 */
#include "doppler/awgn/awgn_core.h"
#include "doppler/dp_state.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_state_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
  N_BITS = 16,
  N_OUT  = 100
};

/* ── The bit source and its state triplet ───────────────────────────────── */

typedef struct
{
  uint64_t counter;   /* bits delivered so far */
  uint64_t draws;     /* frames started */
  uint64_t end_after; /* the source ends once this many frames were drawn; 0
                         never */
  int dirty;          /* a one is written as 2 or 0xFF, not 1 */
  int refuse_set;     /* its set_state refuses even a well-formed blob */
  int dirty_end;      /* at its end it writes 0xFF bytes, then reports it */
} bit_src_t;

#define SRC_MAGIC DP_FOURCC ('T', 'S', 'R', 'C')

static uint8_t
src_bit (uint64_t k)
{
  return (uint8_t)(((k * 2654435761u) >> 7) & 1u);
}

static int
src_refill (void *u, uint8_t *bits, size_t n)
{
  bit_src_t *s = u;
  if (s->end_after && s->draws >= s->end_after)
    {
      if (s->dirty_end)
        memset (bits, 0xFF, n);
      return 1;
    }
  for (size_t k = 0; k < n; k++)
    {
      const uint8_t b = src_bit (s->counter + k);
      bits[k]         = s->dirty ? (b ? (k & 1 ? 2 : 0xFF) : 0) : b;
    }
  s->counter += n;
  s->draws++;
  return 0;
}

static size_t
src_bytes (const void *u)
{
  (void)u;
  return sizeof (dp_state_hdr_t) + 2 * sizeof (uint64_t);
}

static void
src_get (const void *u, void *blob)
{
  const bit_src_t *s = u;
  DP_GET_OPEN (SRC_MAGIC, 1u, src_bytes (u));
  dp_w_u64 (&_w, s->counter);
  dp_w_u64 (&_w, s->draws);
}

static int
src_set (void *u, const void *blob)
{
  bit_src_t *s = u;
  DP_SET_OPEN (SRC_MAGIC, 1u, src_bytes (u));
  if (s->refuse_set)
    return DP_ERR_INVALID;
  const uint64_t counter = dp_r_u64 (&_r);
  const uint64_t draws   = dp_r_u64 (&_r);
  if (_r.err)
    return DP_ERR_INVALID;
  s->counter = counter;
  s->draws   = draws;
  return DP_OK;
}

static const wfm_synth_refill_state_t SRC_OPS
    = { src_bytes, src_get, src_set, NULL };

/* ── The renderer and its configuration triplet ─────────────────────────── */

typedef struct
{
  uint32_t key;       /* the renderer's configuration */
  int      refuse_at; /* the frame index it refuses, or -1 */
  uint64_t calls;     /* how many times it was called */
  uint64_t seen[64];  /* the frame index of each call, in order */
  uint8_t  max_bit;   /* the largest bit byte any call was given */
} rend_t;

#define REND_MAGIC DP_FOURCC ('T', 'R', 'N', 'D')

static int
render (void *u, uint64_t frame, const uint8_t *bits, size_t n_bits,
        float _Complex *out, size_t n_out)
{
  rend_t *r = u;
  if (r->calls < 64)
    r->seen[r->calls] = frame;
  r->calls++;
  for (size_t k = 0; k < n_bits; k++)
    if (bits[k] > r->max_bit)
      r->max_bit = bits[k];
  if (r->refuse_at >= 0 && frame == (uint64_t)r->refuse_at)
    return 1;
  for (size_t i = 0; i < n_out; i++)
    out[i] = (bits[i % n_bits] ? -1.0f : 1.0f) * 0.1f
                 * (float)(frame + 1 + r->key)
             + I * (0.001f * (float)i + (float)frame);
  return 0;
}

static size_t
rend_bytes (const void *u)
{
  (void)u;
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t);
}

static void
rend_get (const void *u, void *blob)
{
  const rend_t *r = u;
  DP_GET_OPEN (REND_MAGIC, 1u, rend_bytes (u));
  dp_w_u32 (&_w, r->key);
}

/* The synth never calls a renderer's set_state (it compares the bytes of
   get_state itself), so this counts every call and the tests require none. It
   still refuses another configuration, so a synth that called it in place of
   the comparison would not be caught by the key test alone. */
static int rend_set_calls;
static int
rend_set (void *u, const void *blob)
{
  rend_t *r = u;
  rend_set_calls++;
  DP_SET_OPEN (REND_MAGIC, 1u, rend_bytes (u));
  const uint32_t key = dp_r_u32 (&_r);
  if (_r.err || key != r->key)
    return DP_ERR_INVALID;
  return DP_OK;
}

static const wfm_synth_refill_state_t REND_OPS
    = { rend_bytes, rend_get, rend_set, NULL };

/* ── Helpers ───────────────────────────────────────────────────────────────
 */

/* A fixture that cannot be built stops the test (DP_REQUIRE returns int, and
   this returns a pointer). */
#define NEED(c)                                                               \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          DP_TEST_ERR ("FAIL fixture: %s\n", #c);                             \
          exit (1);                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

static dp_wfm_synth_state_t *
make_synth_cfg (rend_t *r, bit_src_t *src, size_t n_bits, size_t n_out,
                double freq, double snr, int snr_mode)
{
  dp_wfm_synth_state_t *s = dp_wfm_synth_create (
      WFM_SYNTH_FRAMES, 1e6, freq, snr, snr_mode, 1, 1, 7, 0, 0, 0.0);
  NEED (s != NULL);
  NEED (dp_wfm_synth_attach_frames (s, render, r, NULL, n_bits, n_out) == 0);
  NEED (dp_wfm_synth_attach_frames_state (s, &REND_OPS) == 0);
  NEED (dp_wfm_synth_set_refill (s, src_refill, src, NULL) == 0);
  NEED (dp_wfm_synth_set_refill_state (s, &SRC_OPS) == 0);
  return s;
}

static dp_wfm_synth_state_t *
make_synth_n (rend_t *r, bit_src_t *src, size_t n_bits, size_t n_out)
{
  return make_synth_cfg (r, src, n_bits, n_out, 0.0, 200.0, 0);
}

static dp_wfm_synth_state_t *
make_synth (rend_t *r, bit_src_t *src)
{
  return make_synth_n (r, src, N_BITS, N_OUT);
}

/* The expected samples of frame f, from the fixtures alone. */
static void
expect_frame (uint64_t f, uint32_t key, float _Complex *out)
{
  uint8_t bits[N_BITS];
  for (size_t k = 0; k < N_BITS; k++)
    bits[k] = src_bit (f * N_BITS + k);
  rend_t r = { .key = key, .refuse_at = -1 };
  render (&r, f, bits, N_BITS, out, N_OUT);
}

static int
frames_equal (const float _Complex *got, size_t first_frame, size_t n_frames,
              uint32_t key)
{
  float _Complex want[N_OUT];
  for (size_t f = 0; f < n_frames; f++)
    {
      expect_frame (first_frame + f, key, want);
      if (memcmp (got + f * N_OUT, want, sizeof want) != 0)
        return 0;
    }
  return 1;
}

static int freed; /* free_user was called */
static void
free_flag (void *u)
{
  (void)u;
  freed++;
}

/* ── The tests ─────────────────────────────────────────────────────────────
 */

static int
test_plays_frames_lazily (void)
{
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[350];

  /* nothing is drawn until the first sample is due */
  DP_CHECK (src.draws == 0 && r.calls == 0);
  dp_wfm_synth_steps (s, x, 1);
  DP_CHECK (src.draws == 1 && r.calls == 1);
  /* a frame is drawn when its first sample is due, not before: 99 more samples
     finish frame 0 and draw nothing */
  dp_wfm_synth_steps (s, x + 1, 99);
  DP_CHECK (src.draws == 1 && r.calls == 1);
  dp_wfm_synth_steps (s, x + 100, 1);
  DP_CHECK (src.draws == 2 && r.calls == 2);
  dp_wfm_synth_steps (s, x + 101, 249);
  DP_CHECK (src.draws == 4 && r.calls == 4);
  /* the callback saw the index of the frame it rendered, from 0 */
  for (uint64_t i = 0; i < 4; i++)
    DP_CHECK (r.seen[i] == i);
  DP_CHECK (frames_equal (x, 0, 3, 7));
  /* control: the check fails for another configuration key */
  DP_CHECK (!frames_equal (x, 0, 3, 8));
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_step_and_steps_agree_and_chunks_do_not_matter (void)
{
  rend_t                r1 = { .key = 7, .refuse_at = -1 }, r2 = r1, r3 = r1;
  bit_src_t             a = { .counter = 0 }, b = a, c = a;
  dp_wfm_synth_state_t *s1 = make_synth (&r1, &a), *s2 = make_synth (&r2, &b),
                       *s3 = make_synth (&r3, &c);
  float _Complex big[450], one[450], chunked[450];
  dp_wfm_synth_steps (s1, big, 450);
  for (size_t i = 0; i < 450; i++)
    one[i] = dp_wfm_synth_step (s2);
  static const size_t sizes[] = { 1, 7, 13, 99, 100, 101, 3 };
  size_t              pos = 0, k = 0;
  while (pos < 450)
    {
      size_t m = sizes[k++ % (sizeof sizes / sizeof *sizes)];
      if (m > 450 - pos)
        m = 450 - pos;
      dp_wfm_synth_steps (s3, chunked + pos, m);
      pos += m;
    }
  DP_CHECK (memcmp (big, one, sizeof big) == 0);
  DP_CHECK (memcmp (big, chunked, sizeof big) == 0);
  dp_wfm_synth_destroy (s1);
  dp_wfm_synth_destroy (s2);
  dp_wfm_synth_destroy (s3);
  return 0;
}

static int
test_the_data_ends_with_the_source (void)
{
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .end_after = 3 }; /* ends after three frames */
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[400];
  dp_wfm_synth_steps (s, x, 400);
  DP_CHECK (frames_equal (x, 0, 3, 7));
  int silent = 1;
  for (size_t i = 300; i < 400; i++)
    silent &= (x[i] == 0.0f);
  DP_CHECK (silent);
  DP_CHECK (dp_wfm_synth_data_ended (s) && !dp_wfm_synth_frames_refused (s));
  DP_CHECK (r.calls == 3);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_a_refusing_renderer_ends_the_data_and_says_so (void)
{
  rend_t    r   = { .key = 7, .refuse_at = 2 }; /* refuses the third frame */
  bit_src_t src = { .counter = 0 };
  dp_wfm_synth_state_t *s = make_synth (&r, &src);
  float _Complex x[350];
  dp_wfm_synth_steps (s, x, 350);
  DP_CHECK (frames_equal (x, 0, 2, 7)); /* the first two are intact */
  int silent = 1;
  for (size_t i = 200; i < 350; i++)
    silent &= (x[i] == 0.0f); /* silence from the third frame's first sample */
  DP_CHECK (silent);
  DP_CHECK (dp_wfm_synth_data_ended (s) && dp_wfm_synth_frames_refused (s));
  /* a source that merely ended is not a refusal (test above); reset keeps
     the flag, as data_ended survives it with a refill attached */
  dp_wfm_synth_reset (s);
  DP_CHECK (dp_wfm_synth_frames_refused (s) && dp_wfm_synth_data_ended (s));
  dp_wfm_synth_destroy (s);
  return 0;
}

/* Run `a` for `n_before` samples, snapshot, restore into a fresh `b` of the
   same configuration, and require both to produce the same next `n_after`
   samples and the same blob. */
static int
split_resumes_end (size_t n_before, size_t n_after, uint64_t end_after)
{
  rend_t                ra = { .key = 7, .refuse_at = -1 }, rb = ra;
  bit_src_t             sa = { .end_after = end_after }, sb = sa;
  dp_wfm_synth_state_t *a = make_synth (&ra, &sa);
  dp_wfm_synth_state_t *b = make_synth (&rb, &sb);
  float _Complex       *x = malloc ((n_before + n_after) * sizeof *x);
  float _Complex       *y = malloc (n_after * sizeof *y);
  DP_REQUIRE (x && y);
  dp_wfm_synth_steps (a, x, n_before);
  const size_t nbytes = dp_wfm_synth_state_bytes (a);
  DP_REQUIRE (nbytes > 0);
  void *blob = malloc (nbytes);
  DP_REQUIRE (blob != NULL);
  dp_wfm_synth_get_state (a, blob);
  DP_REQUIRE (dp_wfm_synth_set_state (b, blob) == DP_OK);
  dp_wfm_synth_steps (a, x + n_before, n_after);
  dp_wfm_synth_steps (b, y, n_after);
  const int same = memcmp (x + n_before, y, n_after * sizeof *y) == 0;
  /* the restored synth serializes to the same bytes */
  void *blob2 = malloc (nbytes);
  DP_REQUIRE (blob2 != NULL);
  dp_wfm_synth_get_state (a, blob);
  dp_wfm_synth_get_state (b, blob2);
  const int same_blob = memcmp (blob, blob2, nbytes) == 0;
  free (blob);
  free (blob2);
  free (x);
  free (y);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  return same && same_blob;
}

static int
split_resumes (size_t n_before, size_t n_after)
{
  return split_resumes_end (n_before, n_after, 0);
}

static int
test_a_split_resumes_bit_exactly (void)
{
  /* before the first sample, inside frame 0, exactly at a frame boundary, and
     inside frame 2 (index 2: a counter that differs from the first frame's,
     so a re-render with the wrong index would be a different frame) */
  DP_CHECK (split_resumes (0, 330));
  DP_CHECK (split_resumes (37, 330));
  DP_CHECK (split_resumes (100, 330));
  DP_CHECK (split_resumes (250, 330));
  DP_CHECK (split_resumes (299, 330));
  /* the source ends exactly at a frame boundary: 300 samples are three frames,
     the end is found when the fourth is due, and resuming finds it too */
  DP_CHECK (split_resumes_end (300, 50, 3));
  return 0;
}

static int
test_the_state_round_trip_and_rejects (void)
{
  rend_t                ra = { .key = 7, .refuse_at = -1 }, rb = ra;
  bit_src_t             sa = { .counter = 0 }, sb = sa;
  dp_wfm_synth_state_t *a = make_synth (&ra, &sa);
  dp_wfm_synth_state_t *b = make_synth (&rb, &sb);
  float _Complex x[250];
  dp_wfm_synth_steps (a, x, 250);
  DP_STATE_ROUNDTRIP_TEST (dp_wfm_synth, a, b);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  return 0;
}

static int
test_a_blob_restored_into_another_configuration_is_refused (void)
{
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a  = make_synth (&ra, &sa);
  float _Complex x[250];
  dp_wfm_synth_steps (a, x, 250);
  const size_t nbytes = dp_wfm_synth_state_bytes (a);
  void        *blob   = malloc (nbytes);
  DP_REQUIRE (blob != NULL);
  dp_wfm_synth_get_state (a, blob);

  /* a source whose renderer's configuration key differs refuses the blob and
     is left as it was */
  rend_t                rb = { .key = 8, .refuse_at = -1 };
  bit_src_t             sb = { .counter = 0 };
  dp_wfm_synth_state_t *b  = make_synth (&rb, &sb);
  float _Complex y[10];
  dp_wfm_synth_steps (b, y, 10);
  const size_t before = dp_wfm_synth_state_bytes (b);
  void        *keep   = malloc (before);
  void        *after  = malloc (before);
  DP_REQUIRE (keep && after);
  dp_wfm_synth_get_state (b, keep);
  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
  dp_wfm_synth_get_state (b, after);
  DP_CHECK (memcmp (keep, after, before) == 0);
  /* control: the same blob into the same configuration is accepted */
  rend_t                rc = { .key = 7, .refuse_at = -1 };
  bit_src_t             sc = { .counter = 0 };
  dp_wfm_synth_state_t *c  = make_synth (&rc, &sc);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_OK);

  /* a blob from a synth of another type is refused */
  dp_wfm_synth_state_t *bits = dp_wfm_synth_create (
      WFM_SYNTH_BITS, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_CHECK (dp_wfm_synth_set_state (bits, blob) == DP_ERR_INVALID);
  dp_wfm_synth_destroy (bits);
  free (blob);
  free (keep);
  free (after);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  dp_wfm_synth_destroy (c);
  return 0;
}

static int
test_a_blob_restores_only_into_the_same_frame_geometry (void)
{
  /* the samples are not in the blob, so the geometry is: a blob from a frame
     of 100 samples into one of 101 (or from 16 bits into 17) would otherwise
     re-render a different frame with no refusal */
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a  = make_synth (&ra, &sa);
  float _Complex x[250];
  dp_wfm_synth_steps (a, x, 250);
  const size_t nbytes = dp_wfm_synth_state_bytes (a);
  void        *blob   = malloc (nbytes);
  DP_REQUIRE (blob != NULL);
  dp_wfm_synth_get_state (a, blob);
  static const struct
  {
    size_t n_bits, n_out;
    int    ok;
  } geo[] = { { N_BITS, N_OUT, 1 },
              { N_BITS, N_OUT + 1, 0 },
              { N_BITS, N_OUT - 1, 0 },
              { N_BITS + 1, N_OUT, 0 },
              { N_BITS - 1, N_OUT, 0 } };
  for (size_t i = 0; i < sizeof geo / sizeof *geo; i++)
    {
      rend_t                rb = { .key = 7, .refuse_at = -1 };
      bit_src_t             sb = { .counter = 0 };
      dp_wfm_synth_state_t *b
          = make_synth_n (&rb, &sb, geo[i].n_bits, geo[i].n_out);
      float _Complex y[10];
      dp_wfm_synth_steps (b, y, 10);
      const size_t n    = dp_wfm_synth_state_bytes (b);
      void        *keep = malloc (n), *after = malloc (n);
      DP_REQUIRE (keep && after);
      dp_wfm_synth_get_state (b, keep);
      const int rc = dp_wfm_synth_set_state (b, blob);
      DP_CHECK ((rc == DP_OK) == geo[i].ok);
      if (!geo[i].ok)
        {
          dp_wfm_synth_get_state (b, after);
          DP_CHECK (memcmp (keep, after, n) == 0); /* left as it was */
        }
      free (keep);
      free (after);
      dp_wfm_synth_destroy (b);
    }
  free (blob);
  dp_wfm_synth_destroy (a);
  return 0;
}

static int
test_a_forged_cursor_is_refused (void)
{
  /* a blob claiming a frame in play with no frame rendered cannot be re-
     rendered: build it from a fresh synth's blob (cursor at n_out, no frame
     in play) by moving the cursor into the frame. The cursor is the u64 after
     wtype, sym_pos, cur_re and cur_im, bit_idx. */
  rend_t                r    = { .key = 7, .refuse_at = -1 };
  bit_src_t             src  = { .counter = 0 };
  dp_wfm_synth_state_t *s    = make_synth (&r, &src);
  const size_t          n    = dp_wfm_synth_state_bytes (s);
  uint8_t              *blob = malloc (n);
  DP_REQUIRE (blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  const size_t off = sizeof (dp_state_hdr_t) + 4 + 4 + 4 + 4 + 8;
  uint64_t     cur;
  memcpy (&cur, blob + off, sizeof cur);
  DP_REQUIRE (cur == N_OUT); /* no frame in play: the cursor is at n_out */
  const uint64_t forged = 5;
  memcpy (blob + off, &forged, sizeof forged);
  DP_CHECK (dp_wfm_synth_set_state (s, blob) == DP_ERR_INVALID);
  const uint64_t past = N_OUT + 1;
  memcpy (blob + off, &past, sizeof past);
  DP_CHECK (dp_wfm_synth_set_state (s, blob) == DP_ERR_INVALID);
  /* a blob whose type word names another type would re-type the synth: the
     type is the first word of the payload */
  memcpy (blob + off, &cur, sizeof cur);
  const uint32_t other = WFM_SYNTH_BITS;
  memcpy (blob + sizeof (dp_state_hdr_t), &other, sizeof other);
  DP_CHECK (dp_wfm_synth_set_state (s, blob) == DP_ERR_INVALID);
  free (blob);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_the_type_neither_enters_nor_leaves_frames_by_a_setter (void)
{
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  DP_CHECK (dp_wfm_synth_set_wtype (s, WFM_SYNTH_BITS) == DP_ERR_INVALID);
  DP_CHECK (dp_wfm_synth_set_wtype (s, WFM_SYNTH_TONE) == DP_ERR_INVALID);
  DP_CHECK (dp_wfm_synth_set_wtype (s, WFM_SYNTH_FRAMES) == DP_OK);
  DP_CHECK (s->wtype == WFM_SYNTH_FRAMES);
  dp_wfm_synth_destroy (s);
  dp_wfm_synth_state_t *tone = dp_wfm_synth_create (
      WFM_SYNTH_TONE, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_CHECK (dp_wfm_synth_set_wtype (tone, WFM_SYNTH_FRAMES) == DP_ERR_INVALID);
  DP_CHECK (tone->wtype == WFM_SYNTH_TONE);
  dp_wfm_synth_destroy (tone);
  return 0;
}

static int
test_a_shaper_and_the_symbols_setter_are_no_ops (void)
{
  rend_t                r       = { .key = 7, .refuse_at = -1 };
  bit_src_t             src     = { .counter = 0 };
  dp_wfm_synth_state_t *s       = make_synth (&r, &src);
  const float           taps[3] = { 0.25f, 0.5f, 0.25f };
  DP_CHECK (dp_wfm_synth_set_rrc (s, taps, 3) == 0);
  DP_CHECK (s->fir == NULL && s->shaper == NULL);
  const float _Complex sym[2] = { 1.0f, 2.0f };
  const size_t n_before       = s->n_symbols;
  DP_CHECK (dp_wfm_synth_set_symbols (s, sym, 2) == 0);
  DP_CHECK (s->n_symbols == n_before);
  float _Complex x[100];
  dp_wfm_synth_steps (s, x, 100);
  DP_CHECK (frames_equal (x, 0, 1, 7));
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_install_refusals_and_order (void)
{
  rend_t    r   = { .key = 7, .refuse_at = -1 };
  bit_src_t src = { .counter = 0 };
  /* not a frames synth */
  dp_wfm_synth_state_t *bits = dp_wfm_synth_create (
      WFM_SYNTH_BITS, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_CHECK (dp_wfm_synth_attach_frames (bits, render, &r, NULL, N_BITS, N_OUT)
            == -1);
  dp_wfm_synth_destroy (bits);

  dp_wfm_synth_state_t *s = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  /* the bit source is refused until frames are installed */
  DP_CHECK (dp_wfm_synth_set_refill (s, src_refill, &src, NULL) == -1);
  DP_CHECK (dp_wfm_synth_attach_frames (s, render, &r, NULL, 0, N_OUT) == -1);
  DP_CHECK (dp_wfm_synth_attach_frames (s, render, &r, NULL, N_BITS, 0) == -1);
  /* the frame limit is 2^24 samples, and one more is refused. The limit itself
     is not exercised: accepting it allocates 128 MB, which no unit test pays.
   */
  DP_CHECK (dp_wfm_synth_attach_frames (s, render, &r, NULL, N_BITS,
                                        ((size_t)1 << 24) + 1)
            == -1);
  DP_CHECK (dp_wfm_synth_attach_frames (s, render, &r, NULL, N_BITS, N_OUT)
            == 0);
  DP_CHECK (dp_wfm_synth_set_refill (s, src_refill, &src, NULL) == 0);
  /* without a triplet the refill refuses to serialize */
  DP_CHECK (dp_wfm_synth_state_refusal (s) != NULL);
  DP_CHECK (dp_wfm_synth_set_refill_state (s, &SRC_OPS) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (s) == NULL);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_detach_keeps_the_frame_in_play_and_free_user_runs (void)
{
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  freed                   = 0;
  DP_REQUIRE (
      dp_wfm_synth_attach_frames (s, render, &r, free_flag, N_BITS, N_OUT)
      == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill (s, src_refill, &src, NULL) == 0);
  float _Complex x[300];
  dp_wfm_synth_steps (s, x, 40); /* inside frame 0 */
  DP_CHECK (dp_wfm_synth_attach_frames (s, NULL, NULL, NULL, 0, 0) == 0);
  DP_CHECK (freed == 1); /* detaching freed the user pointer */
  dp_wfm_synth_steps (s, x + 40, 260);
  /* the frame plays out, then the data ends */
  DP_CHECK (frames_equal (x, 0, 1, 7));
  int silent = 1;
  for (size_t i = 100; i < 300; i++)
    silent &= (x[i] == 0.0f);
  DP_CHECK (silent && dp_wfm_synth_data_ended (s));
  DP_CHECK (!dp_wfm_synth_frames_refused (s));
  dp_wfm_synth_destroy (s);
  /* destroy frees an attached user pointer */
  freed = 0;
  s = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0,
                           0.0);
  DP_REQUIRE (
      dp_wfm_synth_attach_frames (s, render, &r, free_flag, N_BITS, N_OUT)
      == 0);
  dp_wfm_synth_destroy (s);
  DP_CHECK (freed == 1);
  return 0;
}

static int
test_reset_replays_the_frame_in_play (void)
{
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[250], y[100];
  dp_wfm_synth_steps (s, x, 250); /* inside frame 2 */
  dp_wfm_synth_reset (s);
  dp_wfm_synth_steps (s, y, 100);
  /* the frame in play (frame 2) again from its start; the frame count and the
     source were not rewound, so nothing new was drawn */
  float _Complex want[N_OUT];
  expect_frame (2, 7, want);
  DP_CHECK (memcmp (y, want, sizeof want) == 0);
  DP_CHECK (src.draws == 3 && r.calls == 3);
  /* a fresh synth, reset before any frame, draws frame 0 on its first sample
   */
  rend_t                r2   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src2 = { .counter = 0 };
  dp_wfm_synth_state_t *f    = make_synth (&r2, &src2);
  dp_wfm_synth_reset (f);
  dp_wfm_synth_steps (f, y, 100);
  expect_frame (0, 7, want);
  DP_CHECK (memcmp (y, want, sizeof want) == 0);
  dp_wfm_synth_destroy (f);
  dp_wfm_synth_destroy (s);
  return 0;
}

/* Offsets in the blob of the fields the tests forge (the header, then the
   payload in get_state's order); a synth in a known state is checked against
   them below, so a layout change turns the test red and not silent. */
enum
{
  OFF_DATA_ENDED  = 4 * 4 + 6 * 8 + 2, /* 4 words, six u64, cur_data, primed */
  OFF_FRAMES_DONE = OFF_DATA_ENDED + 1 + 16, /* data_ended, code_only, frame */
  OFF_REFUSED     = OFF_FRAMES_DONE + 8
};
#define BLOB_OFF(o) (sizeof (dp_state_hdr_t) + (size_t)(o))

static int
test_a_synth_never_reaches_a_state_its_own_blob_refuses (void)
{
  /* (a) the renderer detached mid-frame: the blob would carry a frame no
     renderer can re-render, so the synth says it cannot serialize */
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[100];
  dp_wfm_synth_steps (s, x, 40);
  DP_CHECK (dp_wfm_synth_state_refusal (s) == NULL);
  DP_CHECK (dp_wfm_synth_attach_frames (s, NULL, NULL, NULL, 0, 0) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (s) != NULL);
  DP_CHECK (dp_wfm_synth_state_bytes (s) == 0);
  dp_wfm_synth_destroy (s);

  /* control: at a frame boundary nothing is in play, so the detached synth
     serializes and its blob restores */
  rend_t    r2   = { .key = 7, .refuse_at = -1 };
  bit_src_t src2 = { .counter = 0 };
  s              = make_synth (&r2, &src2);
  dp_wfm_synth_steps (s, x, 100);
  DP_CHECK (dp_wfm_synth_attach_frames (s, NULL, NULL, NULL, 0, 0) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (s) == NULL);
  const size_t n    = dp_wfm_synth_state_bytes (s);
  void        *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  DP_CHECK (dp_wfm_synth_set_state (s, blob) == DP_OK);
  free (blob);
  dp_wfm_synth_destroy (s);

  /* (b) the bit source detached mid-frame */
  rend_t    r3   = { .key = 7, .refuse_at = -1 };
  bit_src_t src3 = { .counter = 0 };
  s              = make_synth (&r3, &src3);
  dp_wfm_synth_steps (s, x, 40);
  DP_CHECK (dp_wfm_synth_set_refill (s, NULL, NULL, NULL) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (s) != NULL);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_refused_says_why_data_ended_and_goes_with_it (void)
{
  /* the renderer refuses frame 0, then the bit source is detached: data_ended
     is cleared, so the flag that says why it was set is cleared too, and the
     synth's blob is one it accepts */
  rend_t                r   = { .key = 7, .refuse_at = 0 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[1];
  dp_wfm_synth_steps (s, x, 1);
  DP_CHECK (dp_wfm_synth_frames_refused (s) && dp_wfm_synth_data_ended (s));
  DP_CHECK (dp_wfm_synth_set_refill (s, NULL, NULL, NULL) == 0);
  DP_CHECK (!dp_wfm_synth_data_ended (s) && !dp_wfm_synth_frames_refused (s));
  const size_t n    = dp_wfm_synth_state_bytes (s);
  void        *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  DP_CHECK (dp_wfm_synth_set_state (s, blob) == DP_OK);
  free (blob);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_reset_after_the_end_replays_nothing (void)
{
  /* a renderer that refused its third frame: the buffer holds frame 1 and the
     bits frame 2's. After reset the synth is silent, and its blob restores */
  rend_t                r   = { .key = 7, .refuse_at = 2 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[350], y[120];
  dp_wfm_synth_steps (s, x, 350);
  DP_REQUIRE (dp_wfm_synth_frames_refused (s));
  dp_wfm_synth_reset (s);
  dp_wfm_synth_steps (s, y, 120);
  int silent = 1;
  for (size_t i = 0; i < 120; i++)
    silent &= (y[i] == 0.0f);
  DP_CHECK (silent);
  DP_CHECK (dp_wfm_synth_frames_refused (s) && dp_wfm_synth_data_ended (s));
  const size_t n    = dp_wfm_synth_state_bytes (s);
  uint8_t     *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  /* the layout offsets the forging tests use are what this blob holds */
  DP_REQUIRE (blob[BLOB_OFF (OFF_DATA_ENDED)] == 1
              && blob[BLOB_OFF (OFF_REFUSED)] == 1);
  rend_t                r2   = { .key = 7, .refuse_at = 2 };
  bit_src_t             src2 = { .counter = 0 };
  dp_wfm_synth_state_t *t    = make_synth (&r2, &src2);
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_OK);
  /* a blob with the data ended and a frame in play is not one the synth makes
   */
  const size_t   off = sizeof (dp_state_hdr_t) + 4 + 4 + 4 + 4 + 8;
  const uint64_t cur = 5;
  memcpy (blob + off, &cur, sizeof cur);
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_ERR_INVALID);
  free (blob);
  dp_wfm_synth_destroy (t);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_a_clobbered_child_is_refused_before_the_source_moves (void)
{
  /* with noise the synth has an AWGN child; its envelope is clobbered in the
     blob. The restore must be refused with nothing written: the bit source,
     which commits in the restore, is checked first */
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a
      = make_synth_cfg (&ra, &sa, N_BITS, N_OUT, 0.0, 10.0, 1);
  float _Complex x[250];
  dp_wfm_synth_steps (a, x, 250);
  DP_REQUIRE (a->awgn != NULL);
  const size_t n    = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (a, blob);
  /* the child sub-blobs come before the bit source's frame and sub-blob and
     the renderer's: the AWGN's ends where those begin */
  const size_t tail = N_BITS + src_bytes (NULL) + rend_bytes (NULL);
  const size_t awgn = dp_awgn_state_bytes (a->awgn);
  DP_REQUIRE (n > tail + awgn);
  blob[n - tail - awgn] ^= 0xFF; /* the first byte of the child's magic */

  rend_t                rb = { .key = 7, .refuse_at = -1 };
  bit_src_t             sb = { .counter = 0 };
  dp_wfm_synth_state_t *b
      = make_synth_cfg (&rb, &sb, N_BITS, N_OUT, 0.0, 10.0, 1);
  float _Complex y[10];
  dp_wfm_synth_steps (b, y, 10);
  const bit_src_t src_before = sb;
  void           *keep = malloc (n), *after = malloc (n);
  DP_REQUIRE (keep && after);
  dp_wfm_synth_get_state (b, keep);
  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
  DP_CHECK (sb.counter == src_before.counter && sb.draws == src_before.draws);
  dp_wfm_synth_get_state (b, after);
  DP_CHECK (memcmp (keep, after, n) == 0);
  free (keep);
  free (after);
  free (blob);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  return 0;
}

static int
test_a_child_that_refuses_its_payload_leaves_the_source_and_the_frame (void)
{
  /* the AWGN child's envelope is intact and its generator state zeroed, which
     dp_awgn_set_state refuses only after its envelope check: the refusal
     comes when the restore is already writing. The bit source and the frame
     must be as they were; only the children before the AWGN may have moved
     (#2104) */
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a
      = make_synth_cfg (&ra, &sa, N_BITS, N_OUT, 0.0, 10.0, 1);
  float _Complex x[250];
  dp_wfm_synth_steps (a, x, 250);
  DP_REQUIRE (a->awgn != NULL);
  const size_t n    = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (a, blob);
  const size_t tail = N_BITS + src_bytes (NULL) + rend_bytes (NULL);
  const size_t awgn = dp_awgn_state_bytes (a->awgn);
  DP_REQUIRE (n > tail + awgn);
  uint8_t *gen = blob + (n - tail - awgn) + sizeof (dp_state_hdr_t);
  memset (gen, 0, 4 * sizeof (uint64_t)); /* the generator's 32 bytes */

  /* the target: 10 samples into its frame 0, so a frame is in play */
  rend_t                rb = { .key = 7, .refuse_at = -1 };
  bit_src_t             sb = { .counter = 0 };
  dp_wfm_synth_state_t *b
      = make_synth_cfg (&rb, &sb, N_BITS, N_OUT, 0.0, 10.0, 1);
  float _Complex y[10];
  dp_wfm_synth_steps (b, y, 10);
  const bit_src_t src_before = sb;
  float _Complex frame[N_OUT];
  uint8_t bits[N_BITS];
  memcpy (frame, b->symbols, sizeof frame);
  memcpy (bits, b->bits, sizeof bits);
  const size_t   cursor = b->sym_read_idx;
  const uint64_t done   = b->frames_done;

  /* control: the same blob, unclobbered, restores and moves the source */
  dp_wfm_synth_get_state (a, blob);
  rend_t                rc = { .key = 7, .refuse_at = -1 };
  bit_src_t             sc = { .counter = 0 };
  dp_wfm_synth_state_t *c
      = make_synth_cfg (&rc, &sc, N_BITS, N_OUT, 0.0, 10.0, 1);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_OK);
  DP_CHECK (sc.counter == sa.counter && sc.counter != src_before.counter);
  memset (gen, 0, 4 * sizeof (uint64_t));

  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
  DP_CHECK (sb.counter == src_before.counter && sb.draws == src_before.draws);
  DP_CHECK (memcmp (b->symbols, frame, sizeof frame) == 0);
  DP_CHECK (memcmp (b->bits, bits, sizeof bits) == 0);
  DP_CHECK (b->sym_read_idx == cursor && b->frames_done == done);
  free (blob);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  dp_wfm_synth_destroy (c);
  return 0;
}

static int
test_reset_with_no_frame_in_play_replays_nothing (void)
{
  float _Complex x[N_OUT + 1], y[N_OUT], want[N_OUT];
  /* (a) a frame spent to its last sample: the cursor is at n_out, nothing is
     in play (set_state's definition), so reset leaves it and the next sample
     draws frame 1; frame 0 does not play twice */
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  dp_wfm_synth_steps (s, x, N_OUT);
  dp_wfm_synth_reset (s);
  dp_wfm_synth_steps (s, y, N_OUT);
  expect_frame (1, 7, want);
  DP_CHECK (memcmp (y, want, sizeof want) == 0);
  DP_CHECK (src.draws == 2 && r.calls == 2);
  dp_wfm_synth_destroy (s);

  /* (b) the renderer refuses frame 1 and the bit source is then detached:
     the buffer holds frame 0, spent, and the synth ends at its next draw.
     Reset must not un-end it: a frames synth has no pattern to replay */
  rend_t    r2   = { .key = 7, .refuse_at = 1 };
  bit_src_t src2 = { .counter = 0 };
  s              = make_synth (&r2, &src2);
  dp_wfm_synth_steps (s, x, N_OUT + 1);
  DP_REQUIRE (dp_wfm_synth_frames_refused (s));
  DP_CHECK (dp_wfm_synth_set_refill (s, NULL, NULL, NULL) == 0);
  dp_wfm_synth_steps (s, x, 1);
  DP_CHECK (dp_wfm_synth_data_ended (s));
  dp_wfm_synth_reset (s);
  DP_CHECK (dp_wfm_synth_data_ended (s));
  dp_wfm_synth_steps (s, y, N_OUT);
  int silent = 1;
  for (size_t i = 0; i < N_OUT; i++)
    silent &= (y[i] == 0.0f);
  DP_CHECK (silent);
  DP_CHECK (!dp_wfm_synth_frames_refused (s));
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_a_source_that_writes_then_ends_leaves_bits_of_zero_and_one (void)
{
  /* the source writes 0xFF bytes and then reports its end: the synth's bits
     are 0 or 1 anyway, so its own blob restores */
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0, .end_after = 1, .dirty_end = 1 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[N_OUT + 1];
  dp_wfm_synth_steps (s, x, N_OUT + 1);
  DP_REQUIRE (dp_wfm_synth_data_ended (s) && src.draws == 1);
  uint8_t worst = 0;
  for (size_t k = 0; k < N_BITS; k++)
    worst = s->bits[k] > worst ? s->bits[k] : worst;
  DP_CHECK (worst <= 1);
  const size_t n    = dp_wfm_synth_state_bytes (s);
  void        *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  DP_CHECK (dp_wfm_synth_set_state (s, blob) == DP_OK);
  free (blob);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_the_bit_count_is_a_key_when_no_source_carries_the_bits (void)
{
  /* with no bit source attached the frame in play's bits are not in the blob,
     so a blob from 16 bits into 17 is told apart by the key alone */
  rend_t                ra = { .key = 7, .refuse_at = -1 }, rb = ra, rc = ra;
  dp_wfm_synth_state_t *a = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  dp_wfm_synth_state_t *b = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  dp_wfm_synth_state_t *c = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (a && b && c);
  DP_REQUIRE (dp_wfm_synth_attach_frames (a, render, &ra, NULL, N_BITS, N_OUT)
              == 0);
  DP_REQUIRE (
      dp_wfm_synth_attach_frames (b, render, &rb, NULL, N_BITS + 1, N_OUT)
      == 0);
  DP_REQUIRE (dp_wfm_synth_attach_frames (c, render, &rc, NULL, N_BITS, N_OUT)
              == 0);
  const size_t n    = dp_wfm_synth_state_bytes (a);
  void        *blob = malloc (n);
  DP_REQUIRE (n == dp_wfm_synth_state_bytes (b) && blob != NULL);
  dp_wfm_synth_get_state (a, blob);
  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_OK); /* control */
  free (blob);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  dp_wfm_synth_destroy (c);
  return 0;
}

static int
test_the_renderer_is_given_bits_of_zero_and_one (void)
{
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .dirty = 1 }; /* writes 2 and 0xFF */
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[250];
  dp_wfm_synth_steps (s, x, 250);
  DP_CHECK (r.calls == 3 && r.max_bit == 1);
  /* the frames are those of the same bits written as 0 and 1 */
  DP_CHECK (frames_equal (x, 0, 2, 7));
  /* and a blob carrying a bit byte above 1 is refused */
  const size_t n    = dp_wfm_synth_state_bytes (s);
  uint8_t     *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  const size_t bits_at     = n - rend_bytes (NULL) - src_bytes (NULL) - N_BITS;
  blob[bits_at + 3]        = 2;
  rend_t                r2 = { .key = 7, .refuse_at = -1 };
  bit_src_t             src2 = { .counter = 0 };
  dp_wfm_synth_state_t *t    = make_synth (&r2, &src2);
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_ERR_INVALID);
  blob[bits_at + 3] = 1; /* control: a clean byte there is accepted */
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_OK);
  free (blob);
  dp_wfm_synth_destroy (t);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_the_carrier_and_the_noise_are_applied_after_the_frame (void)
{
  /* a carrier: each sample is the frame's times a unit phasor that advances by
     freq / fs of a turn per sample; step() and steps() agree */
  rend_t                r1 = { .key = 7, .refuse_at = -1 }, r2 = r1, r3 = r1;
  bit_src_t             s1 = { .counter = 0 }, s2 = s1, s3 = s1;
  dp_wfm_synth_state_t *c
      = make_synth_cfg (&r1, &s1, N_BITS, N_OUT, 1e5, 200.0, 0);
  dp_wfm_synth_state_t *cs
      = make_synth_cfg (&r2, &s2, N_BITS, N_OUT, 1e5, 200.0, 0);
  dp_wfm_synth_state_t *clean = make_synth (&r3, &s3);
  float _Complex a[200], b[200], f[200];
  dp_wfm_synth_steps (c, a, 200);
  for (size_t i = 0; i < 200; i++)
    b[i] = dp_wfm_synth_step (cs);
  dp_wfm_synth_steps (clean, f, 200);
  double worst_step = 0, worst_mag = 0, worst_phase = 0;
  for (size_t i = 0; i < 200; i++)
    {
      worst_step = fmax (worst_step, cabsf (a[i] - b[i]));
      worst_mag  = fmax (worst_mag, fabsf (cabsf (a[i]) - cabsf (f[i])));
      if (i)
        {
          const double d = cargf (a[i] / f[i] * conjf (a[i - 1] / f[i - 1]));
          worst_phase    = fmax (worst_phase, fabs (d - 2.0 * M_PI * 0.1));
        }
    }
  DP_CHECK (worst_step < 1e-5);
  DP_CHECK (worst_mag < 1e-5);
  DP_CHECK (worst_phase < 1e-4); /* with no carrier the increment is 0 */
  dp_wfm_synth_destroy (c);
  dp_wfm_synth_destroy (cs);
  dp_wfm_synth_destroy (clean);

  /* noise: the same seed and level on a constant-one tone gives the same AWGN
     stream, so the frames synth's output is the frame plus that noise */
  rend_t                r4 = { .key = 7, .refuse_at = -1 }, r5 = r4, r6 = r4;
  bit_src_t             s4 = { .counter = 0 }, s5 = s4, s6 = s4;
  dp_wfm_synth_state_t *n_blk
      = make_synth_cfg (&r4, &s4, N_BITS, N_OUT, 0.0, 10.0, 1);
  dp_wfm_synth_state_t *n_one
      = make_synth_cfg (&r5, &s5, N_BITS, N_OUT, 0.0, 10.0, 1);
  dp_wfm_synth_state_t *quiet = make_synth (&r6, &s6);
  dp_wfm_synth_state_t *tone  = dp_wfm_synth_create (
      WFM_SYNTH_TONE, 1e6, 0.0, 10.0, 1, 1, 1, 7, 0, 0, 0.0);
  float _Complex nb[200], no[200], q[200], tn[200];
  dp_wfm_synth_steps (n_blk, nb, 200);
  for (size_t i = 0; i < 200; i++)
    no[i] = dp_wfm_synth_step (n_one);
  dp_wfm_synth_steps (quiet, q, 200);
  dp_wfm_synth_steps (tone, tn, 200);
  double e_blk = 0, e_one = 0, p = 0;
  for (size_t i = 0; i < 200; i++)
    {
      const float _Complex noise = tn[i] - 1.0f;
      e_blk = fmax (e_blk, cabsf (nb[i] - (q[i] + noise)));
      e_one = fmax (e_one, cabsf (no[i] - (q[i] + noise)));
      p += (double)cabsf (noise) * cabsf (noise);
    }
  DP_CHECK (p > 0.0); /* the reference noise is not zero */
  DP_CHECK (e_blk < 1e-5 && e_one < 1e-5);
  dp_wfm_synth_destroy (n_blk);
  dp_wfm_synth_destroy (n_one);
  dp_wfm_synth_destroy (quiet);
  dp_wfm_synth_destroy (tone);
  return 0;
}

static int
test_attaching_over_an_attached_renderer_starts_over (void)
{
  rend_t                r   = { .key = 7, .refuse_at = 1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  freed                   = 0;
  DP_REQUIRE (
      dp_wfm_synth_attach_frames (s, render, &r, free_flag, N_BITS, N_OUT)
      == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill (s, src_refill, &src, NULL) == 0);
  float _Complex x[250];
  dp_wfm_synth_steps (s, x, 250); /* frame 1 refused: ended and refused */
  DP_REQUIRE (dp_wfm_synth_frames_refused (s));
  rend_t r2 = { .key = 9, .refuse_at = -1 };
  DP_CHECK (dp_wfm_synth_attach_frames (s, render, &r2, NULL, N_BITS, N_OUT)
            == 0);
  DP_CHECK (freed == 1); /* the old user pointer was freed */
  DP_CHECK (s->frames_done == 0 && !dp_wfm_synth_frames_refused (s)
            && !dp_wfm_synth_data_ended (s));
  DP_CHECK (s->refill == NULL); /* the old bit source no longer describes it */
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_forged_blobs_the_synth_never_writes_are_refused (void)
{
  rend_t                r = { .key = 7, .refuse_at = 0 }; /* refuses frame 0 */
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  float _Complex x[2];
  dp_wfm_synth_steps (s, x, 2);
  const size_t n    = dp_wfm_synth_state_bytes (s);
  uint8_t     *blob = malloc (n);
  DP_REQUIRE (n > 0 && blob != NULL);
  dp_wfm_synth_get_state (s, blob);
  DP_REQUIRE (blob[BLOB_OFF (OFF_DATA_ENDED)] == 1
              && blob[BLOB_OFF (OFF_REFUSED)] == 1);
  rend_t                r2   = { .key = 7, .refuse_at = 0 };
  bit_src_t             src2 = { .counter = 0 };
  dp_wfm_synth_state_t *t    = make_synth (&r2, &src2);
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_OK); /* control */
  blob[BLOB_OFF (OFF_REFUSED)] = 2; /* a flag that is neither 0 nor 1 */
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_ERR_INVALID);
  blob[BLOB_OFF (OFF_REFUSED)]    = 1;
  blob[BLOB_OFF (OFF_DATA_ENDED)] = 0; /* refused with the data not ended */
  DP_CHECK (dp_wfm_synth_set_state (t, blob) == DP_ERR_INVALID);
  free (blob);
  dp_wfm_synth_destroy (t);
  dp_wfm_synth_destroy (s);

  /* a synth that is not a frames synth has rendered no frames and refused
     none */
  dp_wfm_synth_state_t *bits = dp_wfm_synth_create (
      WFM_SYNTH_BITS, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (dp_wfm_synth_set_bits (bits, (const uint8_t[]){ 1, 0 }, 2, 0)
              == 0);
  const size_t nb = dp_wfm_synth_state_bytes (bits);
  uint8_t     *bb = malloc (nb);
  DP_REQUIRE (nb > 0 && bb != NULL);
  dp_wfm_synth_get_state (bits, bb);
  DP_CHECK (dp_wfm_synth_set_state (bits, bb) == DP_OK); /* control */
  const uint64_t one = 1;
  memcpy (bb + BLOB_OFF (OFF_FRAMES_DONE), &one, sizeof one);
  DP_CHECK (dp_wfm_synth_set_state (bits, bb) == DP_ERR_INVALID);
  memset (bb + BLOB_OFF (OFF_FRAMES_DONE), 0, sizeof one);
  /* with the data ended, so only the not-a-frames-synth rule refuses the
     flag (a refusal with the data not ended is refused by another) */
  bb[BLOB_OFF (OFF_DATA_ENDED)] = 1;
  DP_CHECK (dp_wfm_synth_set_state (bits, bb) == DP_OK); /* control */
  bb[BLOB_OFF (OFF_REFUSED)] = 1;
  DP_CHECK (dp_wfm_synth_set_state (bits, bb) == DP_ERR_INVALID);
  free (bb);
  dp_wfm_synth_destroy (bits);
  return 0;
}

static size_t
no_bytes (const void *u)
{
  (void)u;
  return 0;
}

static const char *
own_reason (const void *u)
{
  (void)u;
  return "the renderer's own reason";
}

static int
test_a_renderer_triplet_that_refuses_to_serialize_is_reported (void)
{
  static const wfm_synth_refill_state_t refuses
      = { no_bytes, rend_get, rend_set, own_reason };
  static const wfm_synth_refill_state_t refuses_quietly
      = { no_bytes, rend_get, rend_set, NULL };
  rend_t                r   = { .key = 7, .refuse_at = -1 };
  bit_src_t             src = { .counter = 0 };
  dp_wfm_synth_state_t *s   = make_synth (&r, &src);
  DP_CHECK (dp_wfm_synth_attach_frames_state (s, &refuses) == 0);
  DP_CHECK (
      dp_wfm_synth_state_refusal (s) != NULL
      && strcmp (dp_wfm_synth_state_refusal (s), "the renderer's own reason")
             == 0);
  DP_CHECK (dp_wfm_synth_state_bytes (s) == 0);
  DP_CHECK (dp_wfm_synth_attach_frames_state (s, &refuses_quietly) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (s) != NULL);
  DP_CHECK (dp_wfm_synth_attach_frames_state (s, &REND_OPS) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (s) == NULL);
  dp_wfm_synth_destroy (s);
  return 0;
}

static int
test_a_restore_never_writes_the_renderer (void)
{
  /* The renderer's sub-blob is compared with the live renderer's get_state
     and its set_state is never called, so no restore, accepted or refused,
     can leave the renderer changed: before, its set_state ran first and a
     later refusal (here, a clobbered bit-source sub-blob) came after it. */
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a  = make_synth (&ra, &sa);
  float _Complex x[250];
  dp_wfm_synth_steps (a, x, 250);
  const size_t n    = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (n), *bad = malloc (n);
  DP_REQUIRE (blob && bad);
  dp_wfm_synth_get_state (a, blob);
  /* the bit source's sub-blob is the one before the renderer's, at the end */
  memcpy (bad, blob, n);
  bad[n - rend_bytes (NULL) - src_bytes (NULL)] ^= 0xFF;

  rend_set_calls           = 0;
  rend_t                rb = { .key = 7, .refuse_at = -1 };
  bit_src_t             sb = { .counter = 0 };
  dp_wfm_synth_state_t *b  = make_synth (&rb, &sb);
  DP_CHECK (dp_wfm_synth_set_state (b, bad) == DP_ERR_INVALID);
  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_OK);
  rend_t                rc = { .key = 8, .refuse_at = -1 }; /* another key */
  bit_src_t             sc = { .counter = 0 };
  dp_wfm_synth_state_t *c  = make_synth (&rc, &sc);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_ERR_INVALID);
  DP_CHECK (rend_set_calls == 0);
  DP_CHECK (rb.key == 7 && rc.key == 8);
  /* control: the counter sees a call */
  DP_CHECK (REND_OPS.set_state (&rb, blob + n - rend_bytes (NULL)) == DP_OK);
  DP_CHECK (rend_set_calls == 1);
  free (blob);
  free (bad);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  dp_wfm_synth_destroy (c);
  return 0;
}

/* The restore's edges: each a use a caller reaches, and each leaves the
   synth it was refused by exactly as it was. */
static int
test_the_restore_edges (void)
{
  /* an installed renderer is what attach_frames_state needs */
  dp_wfm_synth_state_t *bare = dp_wfm_synth_create (
      WFM_SYNTH_FRAMES, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (bare != NULL);
  DP_CHECK (dp_wfm_synth_attach_frames_state (bare, &REND_OPS) == -1);
  /* a frame of more bits than the cap (2^24) is refused, as one of more
     samples is, and leaves the synth with no renderer */
  DP_CHECK (dp_wfm_synth_attach_frames (bare, render, NULL, NULL,
                                        ((size_t)1 << 24) + 1u, 4)
            == -1);
  DP_CHECK (dp_wfm_synth_attach_frames (bare, render, NULL, NULL, 8,
                                        ((size_t)1 << 24) + 1u)
            == -1);
  DP_CHECK (dp_wfm_synth_attach_frames_state (bare, &REND_OPS) == -1);

  /* a blob with a frame in play, taken mid frame 1 (two frames drawn) */
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a  = make_synth (&ra, &sa);
  float _Complex x[N_OUT + 10];
  dp_wfm_synth_steps (a, x, N_OUT + 10);
  const size_t nb   = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (nb), *after = malloc (nb);
  DP_REQUIRE (blob && after);
  dp_wfm_synth_get_state (a, blob);

  /* into a synth with no renderer installed: refused, nothing written */
  DP_CHECK (dp_wfm_synth_set_state (bare, blob) == DP_ERR_INVALID);
  dp_wfm_synth_destroy (bare);

  /* a renderer that refuses the frame the restore re-renders (index 1) */
  rend_t                rb = { .key = 7, .refuse_at = 1 };
  bit_src_t             sb = { .counter = 0 };
  dp_wfm_synth_state_t *b  = make_synth (&rb, &sb);
  dp_wfm_synth_get_state (b, after);
  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
  uint8_t *still = malloc (nb);
  DP_REQUIRE (still);
  dp_wfm_synth_get_state (b, still);
  DP_CHECK (memcmp (after, still, nb) == 0);
  /* control: the renderer that does not refuse restores the same blob */
  rend_t                rc = { .key = 7, .refuse_at = -1 };
  bit_src_t             sc = { .counter = 0 };
  dp_wfm_synth_state_t *c  = make_synth (&rc, &sc);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_OK);

  /* a bit source whose set_state refuses a well-formed blob, after the
     frame re-rendered: the synth is as it was */
  rend_t                rd = { .key = 7, .refuse_at = -1 };
  bit_src_t             sd = { .counter = 0, .refuse_set = 1 };
  dp_wfm_synth_state_t *d  = make_synth (&rd, &sd);
  dp_wfm_synth_get_state (d, after);
  DP_CHECK (dp_wfm_synth_set_state (d, blob) == DP_ERR_INVALID);
  dp_wfm_synth_get_state (d, still);
  DP_CHECK (memcmp (after, still, nb) == 0);

  /* a carrier is a child whose envelope the restore reads: it round trips,
     and a blob taken without one is refused by a synth that has one */
  rend_t                re = { .key = 7, .refuse_at = -1 };
  bit_src_t             se = { .counter = 0 };
  dp_wfm_synth_state_t *e
      = make_synth_cfg (&re, &se, N_BITS, N_OUT, 0.1, 200.0, 0);
  float _Complex y[N_OUT + 10], y2[40], y3[40];
  dp_wfm_synth_steps (e, y, N_OUT + 10);
  const size_t ne    = dp_wfm_synth_state_bytes (e);
  uint8_t     *eblob = malloc (ne);
  DP_REQUIRE (eblob);
  dp_wfm_synth_get_state (e, eblob);
  dp_wfm_synth_steps (e, y2, 40);
  rend_t                rf = { .key = 7, .refuse_at = -1 };
  bit_src_t             sf = { .counter = 0 };
  dp_wfm_synth_state_t *f
      = make_synth_cfg (&rf, &sf, N_BITS, N_OUT, 0.1, 200.0, 0);
  DP_CHECK (dp_wfm_synth_set_state (f, eblob) == DP_OK);
  dp_wfm_synth_steps (f, y3, 40);
  DP_CHECK (memcmp (y2, y3, sizeof y2) == 0);
  DP_CHECK (dp_wfm_synth_set_state (f, blob) == DP_ERR_INVALID);
  free (eblob);
  free (blob);
  free (after);
  free (still);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  dp_wfm_synth_destroy (c);
  dp_wfm_synth_destroy (d);
  dp_wfm_synth_destroy (e);
  dp_wfm_synth_destroy (f);
  return 0;
}

/* The restore re-renders the frame in play, so it needs the renderer and the
   bits in hand. Two blobs reach that point without them, and each is
   refused rather than handed a NULL. */
static int
test_a_restore_without_the_renderer_or_the_bits (void)
{
  /* A: a renderer with no triplet, stepped mid frame, its blob taken. B: the
     same installs, then the renderer detached before any step: its cursor
     sits at the frame's end, so it can serialize, and every presence byte
     matches A's. */
  rend_t                ra = { .key = 7, .refuse_at = -1 };
  bit_src_t             sa = { .counter = 0 };
  dp_wfm_synth_state_t *a = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (a != NULL);
  DP_REQUIRE (dp_wfm_synth_attach_frames (a, render, &ra, NULL, N_BITS, N_OUT)
              == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill (a, src_refill, &sa, NULL) == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill_state (a, &SRC_OPS) == 0);
  float _Complex x[N_OUT / 2];
  dp_wfm_synth_steps (a, x, N_OUT / 2);
  const size_t nb   = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (nb);
  DP_REQUIRE (nb != 0 && blob);
  dp_wfm_synth_get_state (a, blob);

  rend_t                rb = { .key = 7, .refuse_at = -1 };
  bit_src_t             sb = { .counter = 0 };
  dp_wfm_synth_state_t *b = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (b != NULL);
  DP_REQUIRE (dp_wfm_synth_attach_frames (b, render, &rb, NULL, N_BITS, N_OUT)
              == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill (b, src_refill, &sb, NULL) == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill_state (b, &SRC_OPS) == 0);
  DP_REQUIRE (dp_wfm_synth_attach_frames (b, NULL, NULL, NULL, 0, 0) == 0);
  DP_CHECK (dp_wfm_synth_state_refusal (b) == NULL);
  DP_CHECK (dp_wfm_synth_state_bytes (b) == nb);
  uint8_t *before = malloc (nb), *after = malloc (nb);
  DP_REQUIRE (before && after);
  dp_wfm_synth_get_state (b, before);
  DP_CHECK (dp_wfm_synth_set_state (b, blob) == DP_ERR_INVALID);
  dp_wfm_synth_get_state (b, after);
  DP_CHECK (memcmp (before, after, nb) == 0);
  /* control: a synth with the renderer attached takes A's blob */
  rend_t                rc = { .key = 7, .refuse_at = -1 };
  bit_src_t             sc = { .counter = 0 };
  dp_wfm_synth_state_t *c = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (c != NULL);
  DP_REQUIRE (dp_wfm_synth_attach_frames (c, render, &rc, NULL, N_BITS, N_OUT)
              == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill (c, src_refill, &sc, NULL) == 0);
  DP_REQUIRE (dp_wfm_synth_set_refill_state (c, &SRC_OPS) == 0);
  DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_OK);

  /* a hand-edited blob: an end-of-frame blob of a synth with a renderer and
     no refill, its cursor and frame count edited to put a frame in play,
     whose presence byte for the bit source is 0 */
  rend_t                rd = { .key = 7, .refuse_at = -1 };
  dp_wfm_synth_state_t *d = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (d != NULL);
  DP_REQUIRE (dp_wfm_synth_attach_frames (d, render, &rd, NULL, N_BITS, N_OUT)
              == 0);
  const size_t nd    = dp_wfm_synth_state_bytes (d);
  uint8_t     *dblob = malloc (nd);
  DP_REQUIRE (nd != 0 && dblob);
  dp_wfm_synth_get_state (d, dblob);
  /* the honest blob has the cursor at N_OUT and no frame drawn: put it mid
     frame with one frame rendered. The fields sit at fixed offsets after the
     header (get_state's order: wtype, sym_pos, cur_re, cur_im, bit_idx,
     sym_read_idx, ...); each is read back first, so a layout change fails
     here and not by editing the wrong bytes. */
  const size_t cursor_at = sizeof (dp_state_hdr_t) + 24u;
  const size_t drawn_at  = sizeof (dp_state_hdr_t) + 83u;
  uint64_t     v;
  memcpy (&v, dblob + cursor_at, 8);
  DP_CHECK (v == N_OUT);
  memcpy (&v, dblob + drawn_at, 8);
  DP_CHECK (v == 0);
  const uint64_t mid = N_OUT / 2, one = 1;
  memcpy (dblob + cursor_at, &mid, 8);
  memcpy (dblob + drawn_at, &one, 8);
  rend_t                re = { .key = 7, .refuse_at = -1 };
  dp_wfm_synth_state_t *e = dp_wfm_synth_create (WFM_SYNTH_FRAMES, 1e6, 0.0,
                                                 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  DP_REQUIRE (e != NULL);
  DP_REQUIRE (dp_wfm_synth_attach_frames (e, render, &re, NULL, N_BITS, N_OUT)
              == 0);
  /* no bits in the blob (no bit source), a frame in play: refused, and the
     renderer was not called with none */
  DP_CHECK (dp_wfm_synth_set_state (e, dblob) == DP_ERR_INVALID);
  DP_CHECK (re.calls == 0);
  dp_wfm_synth_destroy (e);
  free (dblob);
  free (blob);
  free (before);
  free (after);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  dp_wfm_synth_destroy (c);
  dp_wfm_synth_destroy (d);
  return 0;
}

int
main (void)
{
  int rc = 0;
  rc |= test_plays_frames_lazily ();
  rc |= test_step_and_steps_agree_and_chunks_do_not_matter ();
  rc |= test_the_data_ends_with_the_source ();
  rc |= test_a_refusing_renderer_ends_the_data_and_says_so ();
  rc |= test_a_split_resumes_bit_exactly ();
  rc |= test_the_state_round_trip_and_rejects ();
  rc |= test_a_blob_restored_into_another_configuration_is_refused ();
  rc |= test_a_blob_restores_only_into_the_same_frame_geometry ();
  rc |= test_a_forged_cursor_is_refused ();
  rc |= test_the_type_neither_enters_nor_leaves_frames_by_a_setter ();
  rc |= test_a_shaper_and_the_symbols_setter_are_no_ops ();
  rc |= test_install_refusals_and_order ();
  rc |= test_detach_keeps_the_frame_in_play_and_free_user_runs ();
  rc |= test_reset_replays_the_frame_in_play ();
  rc |= test_a_synth_never_reaches_a_state_its_own_blob_refuses ();
  rc |= test_refused_says_why_data_ended_and_goes_with_it ();
  rc |= test_reset_after_the_end_replays_nothing ();
  rc |= test_a_clobbered_child_is_refused_before_the_source_moves ();
  rc |= test_a_child_that_refuses_its_payload_leaves_the_source_and_the_frame ();
  rc |= test_reset_with_no_frame_in_play_replays_nothing ();
  rc |= test_a_source_that_writes_then_ends_leaves_bits_of_zero_and_one ();
  rc |= test_the_bit_count_is_a_key_when_no_source_carries_the_bits ();
  rc |= test_the_renderer_is_given_bits_of_zero_and_one ();
  rc |= test_the_carrier_and_the_noise_are_applied_after_the_frame ();
  rc |= test_attaching_over_an_attached_renderer_starts_over ();
  rc |= test_forged_blobs_the_synth_never_writes_are_refused ();
  rc |= test_a_renderer_triplet_that_refuses_to_serialize_is_reported ();
  rc |= test_a_restore_never_writes_the_renderer ();
  rc |= test_the_restore_edges ();
  rc |= test_a_restore_without_the_renderer_or_the_bits ();
  if (rc)
    return 1;
  DP_TEST_END ("wfm_synth_frames");
}
