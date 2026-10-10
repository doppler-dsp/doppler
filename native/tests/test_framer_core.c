/**
 * @file test_framer_core.c
 * @brief The framed face of the ring (DECLARE_DP_BUFFER_FRAMES).
 *
 * What the framer promises, and the test that would fail without it:
 *
 *   1. frame k is stream samples [k*hop, k*hop + N), for every (N, hop)
 *      -- checked against SLICING THE INPUT, an oracle that shares no code
 *      with the framer;
 *   2. the output does not depend on how the input was chunked
 *      (dp_chunk_inv.h), including chunk sizes N-1, N, N+1;
 *   3. once drained, fewer than N samples are left, however the input is
 *      split and however little output room the caller offers;
 *   4. a short output room loses nothing: feed() takes less, the caller
 *      offers the rest again;
 *   5. flush() ends the stream with the one row it owes, ON THE HOP GRID,
 *      and only if that row holds an uncovered sample;
 *   6. the carry snapshot has a size that depends on the SHAPE only, so a
 *      fresh framer accepts it, and resuming from it is bit-for-bit.
 */
#include <complex.h>
#include <math.h>

#include "doppler/buffer/buffer.h"

#include "dp_chunk_inv.h"
#include "dp_state_test.h"

/* The typed headers stamp these beside their VIEW; this test builds from
   buffer.h alone (like test_buffer_core), so it stamps the same two. */
DECLARE_DP_BUFFER_FRAMES (f32, float, float _Complex)
DECLARE_DP_BUFFER_FRAMES (f64, double, double _Complex)

/* The i16 instantiation, stamped here for the same reason: i16_buffer_core.h
   declares the same thing beside its VIEW. */
typedef struct
{
  int16_t re, im;
} iq16_t;
DECLARE_DP_BUFFER_FRAMES (i16, int16_t, iq16_t)

typedef struct
{
  size_t N, H;
} shape_t;

typedef struct
{
  dp_f32_t       *ring;
  dp_f32_framer_t fr;
  size_t          N;
} fx_t;

static void *
fx_create (void *arg)
{
  const shape_t *s = arg;
  fx_t          *f = calloc (1, sizeof *f);
  if (!f)
    return NULL;
  f->N    = s->N;
  f->ring = dp_f32_create (4 * s->N);
  if (!f->ring || dp_f32_framer_init (&f->fr, f->ring, s->N, s->H) != DP_OK)
    {
      free (f);
      return NULL;
    }
  return f;
}

static void
fx_destroy (void *o)
{
  fx_t *f = o;
  dp_f32_destroy (f->ring);
  free (f);
}

/* Push n samples through, writing every frame that becomes available as N
   consecutive output elements, never more frames than out_cap has room for. */
static size_t
fx_process (void *o, const void *in, size_t n, void *out, size_t out_cap)
{
  fx_t                 *f   = o;
  const float _Complex *x   = in;
  float _Complex       *y   = out;
  size_t                off = 0, w = 0;
  while (off < n)
    {
      size_t room = (out_cap - w) / f->N;
      size_t took = dp_f32_framer_feed_view (&f->fr, x + off, n - off, room);
      off += took;
      const float _Complex *fr;
      size_t                drained = 0;
      while ((fr = dp_f32_framer_next_view (&f->fr)))
        {
          memcpy (y + w, fr, f->N * sizeof *y);
          w += f->N;
          drained++;
        }
      if (took == 0 && drained == 0)
        break; /* no room and nothing to drain: caller must come back */
    }
  dp_f32_framer_settle (&f->fr);
  return w;
}

static void
ramp (float _Complex *x, size_t n)
{
  for (size_t i = 0; i < n; i++)
    x[i] = (float)i + 0.5f * I * (float)(i % 7);
}

static size_t
rows_for (size_t L, size_t N, size_t H)
{
  return L >= N ? (L - N) / H + 1 : 0;
}

/* The oracle: row r is x[r*H + j], zero past the end of the input. */
static int
rows_match (const float _Complex *y, size_t rows, const float _Complex *x,
            size_t L, size_t N, size_t H, size_t first_row)
{
  for (size_t r = 0; r < rows; r++)
    for (size_t j = 0; j < N; j++)
      {
        size_t i            = (first_row + r) * H + j;
        float _Complex want = i < L ? x[i] : 0.0f;
        if (y[r * N + j] != want)
          return 0;
      }
  return 1;
}

/* A NEGATIVE CONTROL for the harness itself. This object drops the first
   sample of every call, so its output depends on where the calls fall -- the
   exact defect the property exists to catch. If the harness passes it, every
   "invariant" verdict in the suite is worthless. */
static void *
leaky_create (void *arg)
{
  (void)arg;
  return calloc (1, 1);
}

static void
leaky_destroy (void *o)
{
  free (o);
}

static size_t
leaky_process (void *o, const void *in, size_t n, void *out, size_t out_cap)
{
  (void)o;
  (void)out_cap;
  if (n == 0)
    return 0;
  memcpy (out, (const float _Complex *)in + 1,
          (n - 1) * sizeof (float _Complex));
  return n - 1;
}

/* A SECOND negative control: an object that emits nothing at all. Every
   partition of "no output" equals the one-shot "no output", so a harness that
   only compares what was produced passes it. */
static size_t
silent_process (void *o, const void *in, size_t n, void *out, size_t out_cap)
{
  (void)o;
  (void)in;
  (void)n;
  (void)out;
  (void)out_cap;
  return 0;
}

static const shape_t SHAPES[] = {
  { 8, 8 }, { 8, 3 }, { 8, 1 }, { 1, 1 }, { 16, 5 }, { 7, 7 }, { 64, 16 },
};
#define N_SHAPES (sizeof SHAPES / sizeof *SHAPES)

int
main (void)
{
  enum
  {
    L = 3000
  };
  float _Complex *x = malloc (L * sizeof *x);
  DP_REQUIRE (x != NULL);
  ramp (x, L);

  /* ---- 0. init refuses what it cannot honour -------------------------- */
  {
    dp_f32_t       *r = dp_f32_create (16);
    dp_f32_framer_t fr;
    DP_CHECK (dp_f32_framer_init (&fr, r, 8, 0) == DP_ERR_INVALID); /* hop 0 */
    DP_CHECK (dp_f32_framer_init (&fr, r, 8, 9)
              == DP_ERR_INVALID); /* hop > N */
    DP_CHECK (dp_f32_framer_init (&fr, r, 17, 4)
              == DP_ERR_INVALID); /* N > cap */
    DP_CHECK (dp_f32_framer_init (&fr, NULL, 8, 4) == DP_ERR_INVALID);
    DP_CHECK (dp_f32_framer_init (&fr, r, 8, 4) == DP_OK);
    float _Complex one = 1.0f;
    dp_f32_write_some (r, (const float *)&one, 1);
    DP_CHECK (dp_f32_framer_init (&fr, r, 8, 4)
              == DP_ERR_INVALID); /* not empty */
    dp_f32_destroy (r);
  }

  /* ---- the harness fails a chunk-dependent object --------------------- */
  {
    dp_ci_spec_t leaky = { .name     = "leaky (negative control)",
                           .create   = leaky_create,
                           .destroy  = leaky_destroy,
                           .process  = leaky_process,
                           .in_size  = sizeof (float _Complex),
                           .out_size = sizeof (float _Complex),
                           .out_cap  = L,
                           .quiet    = 1 };
    DP_CHECK (dp_chunk_invariance (&leaky, x, L) > 0);

    /* An object that produces nothing is not "invariant". */
    dp_ci_spec_t silent = leaky;
    silent.name         = "silent (negative control)";
    silent.process      = silent_process;
    DP_CHECK (dp_chunk_invariance (&silent, x, L) > 0);

    /* Nor is the real framer over an input of no elements. */
    shape_t      shape0 = SHAPES[0];
    dp_ci_spec_t empty  = { .name     = "framer, empty input (control)",
                            .create   = fx_create,
                            .destroy  = fx_destroy,
                            .process  = fx_process,
                            .arg      = &shape0,
                            .in_size  = sizeof (float _Complex),
                            .out_size = sizeof (float _Complex),
                            .out_cap  = 4 * shape0.N,
                            .frame_n  = shape0.N,
                            .quiet    = 1 };
    DP_CHECK (dp_chunk_invariance (&empty, x, 0) > 0);
  }

  for (size_t s = 0; s < N_SHAPES; s++)
    {
      const size_t    N = SHAPES[s].N, H = SHAPES[s].H;
      shape_t         shape = SHAPES[s];
      const size_t    R     = rows_for (L, N, H);
      float _Complex *y     = malloc ((R + 1) * N * sizeof *y);
      DP_REQUIRE (y != NULL);

      /* ---- 1. frame k is x[k*H, k*H+N), against slicing --------------- */
      {
        void *o = fx_create (&shape);
        DP_REQUIRE (o != NULL);
        size_t w = fx_process (o, x, L, y, (R + 1) * N);
        DP_CHECK (w == R * N);
        DP_CHECK (rows_match (y, R, x, L, N, H, 0));
        fx_destroy (o);
      }

      /* ---- 2. chunk-invariance: sizes 1, 7, N-1, N, N+1, 3N+5, random - */
      {
        dp_ci_spec_t spec = { .name     = "framer",
                              .create   = fx_create,
                              .destroy  = fx_destroy,
                              .process  = fx_process,
                              .arg      = &shape,
                              .in_size  = sizeof (float _Complex),
                              .out_size = sizeof (float _Complex),
                              .out_cap  = (R + 1) * N,
                              .frame_n  = N };
        DP_CHECK (dp_chunk_invariance (&spec, x, L) == 0);
      }

      /* ---- 3 & 4. the carry bound, and a 1-frame output room ---------- */
      {
        static const size_t chunks[] = { 1, 5, 7, 64, 1000 };
        for (size_t c = 0; c < sizeof chunks / sizeof *chunks; c++)
          {
            dp_f32_t       *ring = dp_f32_create (4 * N);
            dp_f32_framer_t fr;
            DP_REQUIRE (dp_f32_framer_init (&fr, ring, N, H) == DP_OK);
            size_t rows = 0, taken = 0;
            int    bound_ok = 1;
            while (taken < L)
              {
                size_t m    = chunks[c] < L - taken ? chunks[c] : L - taken;
                size_t done = 0;
                while (done < m)
                  {
                    /* room for ONE frame: feed may take less than offered. */
                    size_t t = dp_f32_framer_feed_view (&fr, x + taken + done,
                                                        m - done, 1);
                    done += t;
                    const float _Complex *f;
                    size_t                got = 0;
                    while ((f = dp_f32_framer_next_view (&fr)))
                      {
                        DP_CHECK (rows_match (f, 1, x, L, N, H, rows));
                        rows++;
                        got++;
                      }
                    dp_f32_framer_settle (&fr);
                    DP_CHECK (got <= 1);
                    if (dp_f32_available (ring) >= N)
                      bound_ok = 0;
                    if (t == 0 && got == 0)
                      break;
                  }
                DP_CHECK (done == m);
                taken += m;
              }
            DP_CHECK (bound_ok);
            DP_CHECK (rows == R);
            DP_CHECK (dp_f32_framer_pending (&fr)
                      == L - (R ? (R - 1) * H + N : 0));
            dp_f32_destroy (ring);
          }
      }

      /* ---- 5. flush: on the hop grid, once, only if it owes a row ----- */
      for (size_t cut = N; cut <= 4 * N + 3 && cut <= L; cut++)
        {
          void  *o    = fx_create (&shape);
          fx_t  *f    = o;
          size_t w    = fx_process (o, x, cut, y, (R + 1) * N);
          size_t full = rows_for (cut, N, H);
          DP_CHECK (w == full * N);
          size_t          covered = full ? (full - 1) * H + N : 0;
          float _Complex *last    = malloc (N * sizeof *last);
          size_t          owed    = dp_f32_framer_flush_view (&f->fr, last);
          DP_CHECK (owed == (cut > covered));
          if (owed) /* the row a one-shot run over the zero-padded input has */
            DP_CHECK (rows_match (last, 1, x, cut, N, H, full));
          DP_CHECK (dp_f32_framer_flush_view (&f->fr, last) == 0);
          DP_CHECK (dp_f32_framer_pending (&f->fr) == 0);
          /* the stream restarted: the next row begins at the new sample 0 */
          size_t w2 = fx_process (o, x + 10, N, y, (R + 1) * N);
          DP_CHECK (w2 == N);
          DP_CHECK (memcmp (y, x + 10, N * sizeof *y) == 0);
          free (last);
          fx_destroy (o);
        }

      /* ---- 6. the snapshot: shape-sized, fresh-restorable, bit-exact -- */
      {
        dp_f32_t *ra = dp_f32_create (4 * N), *rb = dp_f32_create (4 * N);
        dp_f32_framer_t a, b;
        DP_REQUIRE (dp_f32_framer_init (&a, ra, N, H) == DP_OK);
        DP_REQUIRE (dp_f32_framer_init (&b, rb, N, H) == DP_OK);
        size_t empty_bytes = dp_f32_framer_state_bytes (&a);
        /* mid-frame: a drained prefix that leaves a partial frame behind */
        const size_t cut = 2 * N + N / 2 + 1;
        size_t       off = 0;
        while (off < cut)
          {
            off += dp_f32_framer_feed_view (&a, x + off, cut - off, 4);
            while (dp_f32_framer_next_view (&a))
              ;
            dp_f32_framer_settle (&a);
          }
        /* a fill-sized snapshot would differ here and be refused by b */
        DP_CHECK (dp_f32_framer_state_bytes (&a) == empty_bytes);
        DP_STATE_ROUNDTRIP_TEST (dp_f32_framer, &a, &b);

        /* b is now a's twin: finish the stream on both, compare rows */
        void *blob = malloc (empty_bytes);
        dp_f32_framer_get_state (&a, blob);
        DP_CHECK (dp_f32_framer_set_state (&b, blob) == DP_OK);
        size_t          cap = (rows_for (L, N, H) + 1) * N;
        float _Complex *ya  = calloc (cap, sizeof *ya);
        float _Complex *yb  = calloc (cap, sizeof *yb);
        size_t          wa = 0, wb = 0;
        for (size_t o2 = cut; o2 < L; o2 += 11)
          {
            size_t m = 11 < L - o2 ? 11 : L - o2;
            /* a and b see DIFFERENT chunkings of the same remainder */
            size_t d = 0;
            while (d < m)
              {
                d += dp_f32_framer_feed_view (&a, x + o2 + d, m - d, 8);
                const float _Complex *f;
                while ((f = dp_f32_framer_next_view (&a)))
                  {
                    memcpy (ya + wa, f, N * sizeof *ya);
                    wa += N;
                  }
                dp_f32_framer_settle (&a);
              }
            d = 0;
            while (d < m)
              {
                size_t t = m - d < 3 ? m - d : 3;
                d += dp_f32_framer_feed_view (&b, x + o2 + d, t, 8);
                const float _Complex *f;
                while ((f = dp_f32_framer_next_view (&b)))
                  {
                    memcpy (yb + wb, f, N * sizeof *yb);
                    wb += N;
                  }
                dp_f32_framer_settle (&b);
              }
          }
        DP_CHECK (wa == wb && memcmp (ya, yb, wa * sizeof *ya) == 0);

        /* counters that disagree with the samples are refused, untouched */
        dp_f32_t       *rc = dp_f32_create (4 * N);
        dp_f32_framer_t c;
        DP_REQUIRE (dp_f32_framer_init (&c, rc, N, H) == DP_OK);
        dp_f32_framer_get_state (&a, blob);
        uint64_t bad = 0xFFFFFFFFull;
        memcpy ((char *)blob + DP_FRAMER_STATE_WRITTEN_OFFSET (float, N), &bad,
                sizeof bad);
        DP_CHECK (dp_f32_framer_set_state (&c, blob) == DP_ERR_INVALID);
        DP_CHECK (dp_f32_available (rc) == 0 && c.written == 0);
        dp_f32_destroy (rc);
        free (ya);
        free (yb);
        free (blob);
        dp_f32_destroy (ra);
        dp_f32_destroy (rb);
      }
      free (y);
    }

  /* ---- 8. an UNDRAINED framer is refused, not overflowed ---------------- */
  /* feed() hands back frames the caller has not taken. flush() would then
     copy live >= N samples into an N-sample row, and the snapshot would not
     fit its fixed size; both are refused. Under ASan the unguarded flush is a
     stack overflow of `row`, which is how this case proves the guard. */
  {
    dp_f32_t       *r = dp_f32_create (64);
    dp_f32_framer_t fr;
    DP_REQUIRE (dp_f32_framer_init (&fr, r, 8, 4) == DP_OK);
    DP_CHECK (dp_f32_framer_feed_view (&fr, x, 40, 1000) == 40);
    DP_CHECK (!dp_f32_framer_drained (&fr));
    float _Complex row[8];
    DP_CHECK (dp_f32_framer_flush_view (&fr, row) < 0);
    DP_CHECK (dp_f32_available (r) == 40); /* refused: nothing changed */

    /* ...and a snapshot of it is a blob no framer accepts, loudly */
    size_t bytes = dp_f32_framer_state_bytes (&fr);
    void  *blob  = malloc (bytes);
    DP_REQUIRE (blob != NULL);
    memset (blob, 0xA5, bytes);
    dp_f32_framer_get_state (&fr, blob);
    /* refused means ZEROED: no byte of the caller's buffer survives into a
       blob that is shipped (the doppler#1471 defect class). */
    int all_zero = 1;
    for (size_t i = 0; i < bytes; i++)
      if (((const unsigned char *)blob)[i] != 0)
        all_zero = 0;
    DP_CHECK (all_zero);
    dp_f32_t       *r2 = dp_f32_create (64);
    dp_f32_framer_t twin;
    DP_REQUIRE (dp_f32_framer_init (&twin, r2, 8, 4) == DP_OK);
    DP_CHECK (dp_f32_framer_set_state (&twin, blob) == DP_ERR_INVALID);

    /* drained, both work, and the rows were not lost */
    int rows = 0;
    while (dp_f32_framer_next_view (&fr))
      rows++;
    DP_CHECK (rows == 9); /* (40 - 8) / 4 + 1 */
    DP_CHECK (dp_f32_framer_drained (&fr));
    dp_f32_framer_get_state (&fr, blob);
    DP_CHECK (dp_f32_framer_set_state (&twin, blob) == DP_OK);
    DP_CHECK (dp_f32_framer_flush_view (&fr, row) >= 0);
    free (blob);
    dp_f32_destroy (r);
    dp_f32_destroy (r2);
  }

  /* ---- 9. a snapshot names its hop ------------------------------------- */
  {
    /* Same frame_n, so the SAME size; only the hop differs, and with
       frames == 0 every counter is consistent with either. */
    dp_f32_t *ra = dp_f32_create (32), *rb = dp_f32_create (32),
             *rc = dp_f32_create (32);
    dp_f32_framer_t a, b, c;
    DP_REQUIRE (dp_f32_framer_init (&a, ra, 8, 4) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&b, rb, 8, 8) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&c, rc, 8, 4) == DP_OK);
    DP_CHECK (dp_f32_framer_state_bytes (&a)
              == dp_f32_framer_state_bytes (&b));
    DP_CHECK (dp_f32_framer_feed_view (&a, x, 3, 8) == 3);
    void *blob = malloc (dp_f32_framer_state_bytes (&a));
    DP_REQUIRE (blob != NULL);
    dp_f32_framer_get_state (&a, blob);
    DP_CHECK (dp_f32_framer_set_state (&b, blob) == DP_ERR_INVALID);
    DP_CHECK (dp_f32_available (rb) == 0); /* untouched */
    DP_CHECK (dp_f32_framer_set_state (&c, blob) == DP_OK);
    free (blob);
    dp_f32_destroy (ra);
    dp_f32_destroy (rb);
    dp_f32_destroy (rc);
  }

  /* ---- 7. the double-precision stamp is the same framer ---------------- */
  {
    dp_f64_t       *r = dp_f64_create (16);
    dp_f64_framer_t fr;
    DP_REQUIRE (dp_f64_framer_init (&fr, r, 4, 2) == DP_OK);
    double _Complex v[10];
    for (int i = 0; i < 10; i++)
      v[i] = i;
    DP_CHECK (dp_f64_framer_feed_view (&fr, v, 10, 8) == 10);
    int                    rows = 0;
    const double _Complex *f;
    while ((f = dp_f64_framer_next_view (&fr)))
      {
        DP_CHECK (creal (f[0]) == 2.0 * rows);
        rows++;
      }
    DP_CHECK (rows == 4);
    dp_f64_destroy (r);
  }

  /* ---- 10. the claims the inventory found with no pin ------------------ */
  /* The claim inventory (src/doppler/tests/validation/framer/validate.py §1)
     read the header against this file: framer_frames_for and framer_reset had
     ZERO mentions, framer_drained had two, and "zero-copy" was prose. */
  {
    dp_f32_t       *r = dp_f32_create (64);
    dp_f32_framer_t fr;
    DP_REQUIRE (dp_f32_framer_init (&fr, r, 8, 3) == DP_OK);

    /* zero-copy: the frame IS the ring's own memory, not a copy of it */
    DP_CHECK (dp_f32_framer_feed_view (&fr, x, 20, 1000) == 20);
    const float _Complex *f0 = dp_f32_framer_next_view (&fr);
    DP_REQUIRE (f0 != NULL);
    DP_CHECK ((const float *)f0 == &r->data[(r->tail & r->mask) * 2]);
    DP_CHECK (f0[0] == x[0] && f0[7] == x[7]);

    /* the hop is owed, not yet retired: the pointer holds until the next
       framer call, so the ring still reads as holding the whole frame */
    DP_CHECK (dp_f32_available (r) == 20);
    dp_f32_framer_settle (&fr);
    DP_CHECK (dp_f32_available (r) == 17); /* exactly one hop retired */
    dp_f32_framer_settle (&fr); /* and settling twice retires none */
    DP_CHECK (dp_f32_available (r) == 17);

    /* drained is exactly "next() would return NULL" */
    for (int i = 0; i < 40; i++)
      {
        int                   would_yield = dp_f32_framer_drained (&fr) == 0;
        const float _Complex *f           = dp_f32_framer_next_view (&fr);
        DP_CHECK (would_yield == (f != NULL));
        if (!f)
          break;
      }
    DP_CHECK (dp_f32_framer_drained (&fr));

    /* reset: empty, restarted at sample 0, nothing pending */
    dp_f32_framer_reset (&fr);
    DP_CHECK (dp_f32_available (r) == 0);
    DP_CHECK (dp_f32_framer_pending (&fr) == 0);
    DP_CHECK (fr.written == 0 && fr.frames == 0 && fr.owed == 0);
    DP_CHECK (dp_f32_framer_feed_view (&fr, x + 5, 8, 1) == 8);
    const float _Complex *g = dp_f32_framer_next_view (&fr);
    DP_REQUIRE (g != NULL);
    DP_CHECK (g[0] == x[5]); /* row 0 starts at the NEW sample 0 */
    dp_f32_destroy (r);
  }
  {
    /* max_frames == 0 admits at most frame_n - 1 samples and never a frame */
    dp_f32_t       *r = dp_f32_create (64);
    dp_f32_framer_t fr;
    DP_REQUIRE (dp_f32_framer_init (&fr, r, 8, 3) == DP_OK);
    DP_CHECK (dp_f32_framer_feed_view (&fr, x, 100, 0) == 7);
    DP_CHECK (dp_f32_framer_next_view (&fr) == NULL);
    DP_CHECK (dp_f32_framer_feed_view (&fr, x, 100, 0)
              == 0); /* full of carry */
    dp_f32_destroy (r);
  }
  {
    /* frames_for(n) is EXACTLY what feeding n more then draining yields,
       from every fill, at every size, for several shapes */
    static const shape_t shp[] = { { 8, 3 }, { 8, 8 }, { 5, 1 }, { 1, 1 } };
    int                  exact = 1;
    for (size_t k = 0; k < sizeof shp / sizeof *shp; k++)
      for (size_t pre = 0; pre < 2 * shp[k].N; pre++)
        for (size_t n = 0; n < 4 * shp[k].N + 3; n++)
          {
            dp_f32_t       *r = dp_f32_create (16 * shp[k].N + 64);
            dp_f32_framer_t fr;
            DP_REQUIRE (dp_f32_framer_init (&fr, r, shp[k].N, shp[k].H)
                        == DP_OK);
            /* build the fill `pre` by feeding and draining (so owed is 0) */
            dp_f32_framer_feed_view (&fr, x, pre, 1000);
            while (dp_f32_framer_next_view (&fr))
              ;
            dp_f32_framer_settle (&fr);
            size_t predicted = dp_f32_framer_frames_for (&fr, n);
            dp_f32_framer_feed_view (&fr, x + pre, n, 1000);
            size_t got = 0;
            while (dp_f32_framer_next_view (&fr))
              got++;
            if (got != predicted)
              exact = 0;
            dp_f32_destroy (r);

            /* ...and with a frame handed out and its hop still OWED: the
               prediction must discount the hop the next call will retire. */
            if (pre >= shp[k].N)
              {
                r = dp_f32_create (16 * shp[k].N + 64);
                DP_REQUIRE (dp_f32_framer_init (&fr, r, shp[k].N, shp[k].H)
                            == DP_OK);
                dp_f32_framer_feed_view (&fr, x, pre, 1000);
                DP_REQUIRE (dp_f32_framer_next_view (&fr) != NULL); /* owed */
                predicted = dp_f32_framer_frames_for (&fr, n);
                dp_f32_framer_feed_view (&fr, x + pre, n, 1000);
                got = 0;
                while (dp_f32_framer_next_view (&fr))
                  got++;
                if (got != predicted)
                  exact = 0;
                dp_f32_destroy (r);
              }
          }
    DP_CHECK (exact);
  }
  {
    /* ...and the claim holds where the RING is the limit: capacity == N and
       a little over, n far past the room, and every max_frames. frames_for()
       is feed(n, SIZE_MAX) then a drain, and feed(n, m) yields the lesser of
       m and that. The roomy ring above cannot see either. */
    static const shape_t shp[]   = { { 8, 4 }, { 8, 8 }, { 5, 1 }, { 1, 1 } };
    static const size_t  extra[] = { 0, 1, 7 };
    int                  exact = 1, capped = 1;
    for (size_t k = 0; k < sizeof shp / sizeof *shp; k++)
      for (size_t e = 0; e < sizeof extra / sizeof *extra; e++)
        for (size_t pre = 0; pre < shp[k].N; pre++)
          for (size_t n = 0; n < 4 * shp[k].N + 3; n += 1 + n / 7)
            for (size_t m = 0; m < 4; m++)
              {
                dp_f32_t       *r = dp_f32_create (shp[k].N + extra[e]);
                dp_f32_framer_t fr;
                DP_REQUIRE (r != NULL);
                DP_REQUIRE (dp_f32_framer_init (&fr, r, shp[k].N, shp[k].H)
                            == DP_OK);
                dp_f32_framer_feed_view (&fr, x, pre, 0);
                size_t predicted = dp_f32_framer_frames_for (&fr, n);
                dp_f32_framer_feed_view (&fr, x + pre, n, m);
                size_t got = 0;
                while (dp_f32_framer_next_view (&fr))
                  got++;
                if (got != (m < predicted ? m : predicted))
                  capped = 0;
                dp_f32_destroy (r);

                r = dp_f32_create (shp[k].N + extra[e]);
                DP_REQUIRE (dp_f32_framer_init (&fr, r, shp[k].N, shp[k].H)
                            == DP_OK);
                dp_f32_framer_feed_view (&fr, x, pre, 0);
                dp_f32_framer_feed_view (&fr, x + pre, n, SIZE_MAX);
                got = 0;
                while (dp_f32_framer_next_view (&fr))
                  got++;
                if (got != predicted)
                  exact = 0;
                dp_f32_destroy (r);
              }
    DP_CHECK (exact);
    DP_CHECK (capped);

    /* the reviewer's literal case, and n near SIZE_MAX must not wrap */
    dp_f32_t       *r = dp_f32_create (8);
    dp_f32_framer_t fr;
    DP_REQUIRE (r != NULL && r->capacity == 8);
    DP_REQUIRE (dp_f32_framer_init (&fr, r, 8, 4) == DP_OK);
    DP_CHECK (dp_f32_framer_frames_for (&fr, 100) == 1);
    DP_CHECK (dp_f32_framer_feed_view (&fr, x, 100, SIZE_MAX) == 8);
    /* ring full, one frame buffered and undrained: more input adds nothing */
    DP_CHECK (dp_f32_framer_frames_for (&fr, SIZE_MAX) == 1);
    DP_CHECK (dp_f32_framer_next_view (&fr) != NULL);
    dp_f32_framer_t fresh;
    dp_f32_t       *r2 = dp_f32_create (8);
    DP_REQUIRE (dp_f32_framer_init (&fresh, r2, 8, 4) == DP_OK);
    DP_CHECK (dp_f32_framer_frames_for (&fresh, SIZE_MAX) == 1);
    DP_CHECK (dp_f32_framer_frames_for (&fresh, SIZE_MAX - 3) == 1);
    dp_f32_destroy (r);
    dp_f32_destroy (r2);
  }
  {
    /* a snapshot of another SHAPE is refused: the size differs */
    dp_f32_t       *ra = dp_f32_create (64), *rb = dp_f32_create (64);
    dp_f32_framer_t a, b;
    DP_REQUIRE (dp_f32_framer_init (&a, ra, 8, 4) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&b, rb, 9, 4) == DP_OK);
    void *blob = malloc (dp_f32_framer_state_bytes (&a));
    DP_REQUIRE (blob != NULL);
    dp_f32_framer_get_state (&a, blob);
    DP_CHECK (dp_f32_framer_state_bytes (&a)
              != dp_f32_framer_state_bytes (&b));
    DP_CHECK (dp_f32_framer_set_state (&b, blob) == DP_ERR_INVALID);
    free (blob);
    dp_f32_destroy (ra);
    dp_f32_destroy (rb);
  }

  {
    /* A snapshot of another SAMPLE TYPE is refused. The magic is shared by
       every instantiation and the size depends on (frame_n, sizeof sample),
       so f64 at N=5, f32 at N=9 and i16 at N=17 are the SAME number of bytes:
       with the hop equal too, only the stored sample size can tell them
       apart. Each blob restores into its own type (the control), and into
       neither of the others. */
    dp_f64_t       *r64 = dp_f64_create (64);
    dp_f32_t       *r32 = dp_f32_create (64);
    dp_i16_t       *r16 = dp_i16_create (64);
    dp_f64_framer_t f64f;
    dp_f32_framer_t f32f;
    dp_i16_framer_t i16f;
    DP_REQUIRE (r64 && r32 && r16);
    DP_REQUIRE (dp_f64_framer_init (&f64f, r64, 5, 2) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&f32f, r32, 9, 2) == DP_OK);
    DP_REQUIRE (dp_i16_framer_init (&i16f, r16, 17, 2) == DP_OK);
    const double _Complex v64[3] = { 1.0, 2.0 + I, 3.0 };
    const float _Complex v32[3]  = { 1.0f, 2.0f + I, 3.0f };
    const iq16_t v16[3]          = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
    DP_REQUIRE (dp_f64_framer_feed_view (&f64f, v64, 3, 8) == 3);
    DP_REQUIRE (dp_f32_framer_feed_view (&f32f, v32, 3, 8) == 3);
    DP_REQUIRE (dp_i16_framer_feed_view (&i16f, v16, 3, 8) == 3);

    const size_t bytes = dp_f64_framer_state_bytes (&f64f);
    DP_REQUIRE (dp_f32_framer_state_bytes (&f32f) == bytes);
    DP_REQUIRE (dp_i16_framer_state_bytes (&i16f) == bytes); /* the premise */
    void *b64 = malloc (bytes), *b32 = malloc (bytes), *b16 = malloc (bytes);
    DP_REQUIRE (b64 && b32 && b16);
    dp_f64_framer_get_state (&f64f, b64);
    dp_f32_framer_get_state (&f32f, b32);
    dp_i16_framer_get_state (&i16f, b16);

    DP_CHECK (dp_f64_framer_set_state (&f64f, b64) == DP_OK);
    DP_CHECK (dp_f32_framer_set_state (&f32f, b32) == DP_OK);
    DP_CHECK (dp_i16_framer_set_state (&i16f, b16) == DP_OK);

    DP_CHECK (dp_f32_framer_set_state (&f32f, b64) == DP_ERR_INVALID);
    DP_CHECK (dp_i16_framer_set_state (&i16f, b64) == DP_ERR_INVALID);
    DP_CHECK (dp_f64_framer_set_state (&f64f, b32) == DP_ERR_INVALID);
    DP_CHECK (dp_i16_framer_set_state (&i16f, b32) == DP_ERR_INVALID);
    DP_CHECK (dp_f64_framer_set_state (&f64f, b16) == DP_ERR_INVALID);
    DP_CHECK (dp_f32_framer_set_state (&f32f, b16) == DP_ERR_INVALID);
    /* a refusal changes nothing */
    DP_CHECK (f32f.written == 3 && f32f.frames == 0);

    free (b64);
    free (b32);
    free (b16);
    dp_f64_destroy (r64);
    dp_f32_destroy (r32);
    dp_i16_destroy (r16);
  }
  {
    /* The smallest case: at frame_n == 1 the carry is empty, so every type's
       blob is the same size whatever the type. Only the stored sample size
       tells them apart. */
    dp_f64_t       *r64 = dp_f64_create (8);
    dp_f32_t       *r32 = dp_f32_create (8);
    dp_f64_framer_t f64f;
    dp_f32_framer_t f32f;
    DP_REQUIRE (r64 && r32);
    DP_REQUIRE (dp_f64_framer_init (&f64f, r64, 1, 1) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&f32f, r32, 1, 1) == DP_OK);
    const size_t bytes = dp_f64_framer_state_bytes (&f64f);
    DP_REQUIRE (dp_f32_framer_state_bytes (&f32f) == bytes);
    void *b64 = malloc (bytes), *b32 = malloc (bytes);
    DP_REQUIRE (b64 && b32);
    dp_f64_framer_get_state (&f64f, b64);
    dp_f32_framer_get_state (&f32f, b32);
    DP_CHECK (dp_f64_framer_set_state (&f64f, b64) == DP_OK);
    DP_CHECK (dp_f32_framer_set_state (&f32f, b32) == DP_OK);
    DP_CHECK (dp_f32_framer_set_state (&f32f, b64) == DP_ERR_INVALID);
    DP_CHECK (dp_f64_framer_set_state (&f64f, b32) == DP_ERR_INVALID);
    free (b64);
    free (b32);
    dp_f64_destroy (r64);
    dp_f32_destroy (r32);
  }
  {
    /* The i16 instantiation, end to end (C17 was pinned for f32 and f64): the
       element view is a cast of the scalar face, so frames come out as the
       samples that went in. */
    dp_i16_t       *r = dp_i16_create (16);
    dp_i16_framer_t fr;
    DP_REQUIRE (r != NULL);
    DP_REQUIRE (dp_i16_framer_init (&fr, r, 4, 2) == DP_OK);
    iq16_t v[10];
    for (int i = 0; i < 10; i++)
      v[i] = (iq16_t){ (int16_t)i, (int16_t)(-i) };
    DP_CHECK (dp_i16_framer_feed_view (&fr, v, 10, 8) == 10);
    int           rows = 0, ok = 1;
    const iq16_t *f;
    while ((f = dp_i16_framer_next_view (&fr)))
      {
        for (int j = 0; j < 4; j++)
          ok &= f[j].re == 2 * rows + j && f[j].im == -(2 * rows + j);
        rows++;
      }
    DP_CHECK (rows == 4); /* frames at 0, 2, 4, 6 */
    DP_CHECK (ok);
    dp_i16_destroy (r);
  }

  /* ---- 11. whole frames, the carry, a snapshot's frames (#2042) ------- */
  {
    /* feed_frames: what completes at most k frames and nothing of the
       next. From every drained fill, at every size and k: k 0 takes
       nothing; a take short of n, and not empty, stops ON a frame
       boundary having completed exactly k frames; a take of all of n yields
       what feed() would. feed() itself takes more whenever it can, so the cap
       is what is pinned, not the frame count alone. */
    static const shape_t shp[] = { { 8, 3 }, { 8, 8 }, { 5, 1 }, { 1, 1 } };
    int                  zero = 1, boundary = 1, same = 1, past = 0;
    for (size_t s = 0; s < sizeof shp / sizeof *shp; s++)
      for (size_t pre = 0; pre < 2 * shp[s].N; pre++)
        for (size_t n = 0; n < 4 * shp[s].N + 3; n++)
          for (size_t k = 0; k < 5; k++)
            {
              const size_t    N = shp[s].N, H = shp[s].H;
              dp_f32_t       *r = dp_f32_create (16 * N + 64);
              dp_f32_t       *q = dp_f32_create (16 * N + 64);
              dp_f32_framer_t fr, twin;
              DP_REQUIRE (r && q);
              DP_REQUIRE (dp_f32_framer_init (&fr, r, N, H) == DP_OK);
              DP_REQUIRE (dp_f32_framer_init (&twin, q, N, H) == DP_OK);
              dp_f32_framer_feed_view (&fr, x, pre, 1000);
              dp_f32_framer_feed_view (&twin, x, pre, 1000);
              while (dp_f32_framer_next_view (&fr))
                ;
              while (dp_f32_framer_next_view (&twin))
                ;
              const size_t took
                  = dp_f32_framer_feed_frames_view (&fr, x + pre, n, k);
              const size_t plain
                  = dp_f32_framer_feed_view (&twin, x + pre, n, k);
              size_t got = 0, got_plain = 0;
              while (dp_f32_framer_next_view (&fr))
                got++;
              while (dp_f32_framer_next_view (&twin))
                got_plain++;
              if (k == 0 && took != 0)
                zero = 0;
              /* Short of n, and taking something: it ends on a frame
                 boundary, k frames done. (k 0 takes nothing, above.) */
              if (took < n && took > 0
                  && (got != k || dp_f32_framer_pending (&fr) != 0))
                boundary = 0;
              if (took == n && got != got_plain)
                same = 0;
              if (plain > took)
                past = 1; /* the premise: feed() alone takes more */
              dp_f32_destroy (r);
              dp_f32_destroy (q);
            }
    DP_CHECK (zero);
    DP_CHECK (boundary);
    DP_CHECK (same);
    DP_CHECK (past);

    /* An undrained framer already holding k frames takes nothing more, and
       a k whose sample count would overflow is uncapped: feed(). */
    dp_f32_t       *r = dp_f32_create (64), *q = dp_f32_create (64);
    dp_f32_framer_t fr, twin;
    DP_REQUIRE (r && q);
    DP_REQUIRE (dp_f32_framer_init (&fr, r, 8, 3) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&twin, q, 8, 3) == DP_OK);
    DP_CHECK (dp_f32_framer_feed_view (&fr, x, 11, 1000) == 11); /* 2 */
    DP_CHECK (dp_f32_framer_feed_frames_view (&fr, x + 11, 9, 2) == 0);
    dp_f32_framer_reset (&fr);
    DP_CHECK (dp_f32_framer_feed_frames_view (&fr, x, 20, SIZE_MAX)
              == dp_f32_framer_feed_view (&twin, x, 20, SIZE_MAX));
    dp_f32_framer_reset (&fr);
    dp_f32_framer_reset (&twin);
    const size_t big = (SIZE_MAX - 8) / 3 + 2; /* (big-1)*3 + 8 overflows */
    DP_CHECK (dp_f32_framer_feed_frames_view (&fr, x, 20, big)
              == dp_f32_framer_feed_view (&twin, x, 20, big));
    dp_f32_destroy (r);
    dp_f32_destroy (q);
  }
  {
    /* feed_carry: all of n if it completes no frame, counting the carry
       held, else none -- and a refusal leaves the framer as it was. */
    static const shape_t shp[] = { { 8, 3 }, { 8, 8 }, { 5, 1 }, { 1, 1 } };
    int                  exact = 1, untouched = 1;
    for (size_t s = 0; s < sizeof shp / sizeof *shp; s++)
      for (size_t pre = 0; pre < 2 * shp[s].N; pre++)
        for (size_t n = 0; n < 2 * shp[s].N + 3; n++)
          {
            dp_f32_t       *r = dp_f32_create (16 * shp[s].N + 64);
            dp_f32_framer_t fr;
            DP_REQUIRE (r != NULL);
            DP_REQUIRE (dp_f32_framer_init (&fr, r, shp[s].N, shp[s].H)
                        == DP_OK);
            dp_f32_framer_feed_view (&fr, x, pre, 1000);
            while (dp_f32_framer_next_view (&fr))
              ;
            const size_t   frames  = dp_f32_framer_frames_for (&fr, n);
            const uint64_t written = fr.written;
            const size_t   took    = dp_f32_framer_feed_carry_view (&fr, x, n);
            if (took != (frames == 0 ? n : 0))
              exact = 0;
            if (took == 0 && fr.written != written)
              untouched = 0;
            if (dp_f32_framer_next_view (&fr) != NULL)
              exact = 0; /* carry never completes a frame */
            dp_f32_destroy (r);
          }
    DP_CHECK (exact);
    DP_CHECK (untouched);
  }
  {
    /* state_frames reads a blob through set_state's own check: for every
       single-byte corruption of a real snapshot (and the snapshot itself),
       it accepts exactly when set_state does, reports the frames set_state
       restores, and on a refusal leaves `frames` alone. */
    dp_f32_t       *ra = dp_f32_create (64), *rb = dp_f32_create (64);
    dp_f32_framer_t a, b;
    DP_REQUIRE (ra && rb);
    DP_REQUIRE (dp_f32_framer_init (&a, ra, 8, 3) == DP_OK);
    DP_REQUIRE (dp_f32_framer_init (&b, rb, 8, 3) == DP_OK);
    dp_f32_framer_feed_view (&a, x, 20, 1000);
    while (dp_f32_framer_next_view (&a))
      ;
    DP_REQUIRE (a.frames == 5); /* (20 - 8) / 3 + 1 */
    const size_t   bytes = dp_f32_framer_state_bytes (&a);
    unsigned char *blob = malloc (bytes), *bad = malloc (bytes);
    DP_REQUIRE (blob && bad);
    dp_f32_framer_get_state (&a, blob);
    int agree = 1, reports = 1, kept = 1, refused = 0;
    for (size_t i = 0; i <= 2 * bytes; i++)
      {
        memcpy (bad, blob, bytes);
        if (i < 2 * bytes) /* the last pass is the snapshot itself */
          bad[i / 2] ^= (unsigned char)(i % 2 ? 0x80 : 0x01);
        uint64_t f = 0xD0D0D0D0u;
        dp_f32_framer_reset (&b);
        const int seen = dp_f32_framer_state_frames (&b, bad, &f);
        const int took = dp_f32_framer_set_state (&b, bad);
        if ((seen == DP_OK) != (took == DP_OK))
          agree = 0;
        if (seen == DP_OK && f != b.frames)
          reports = 0;
        if (seen != DP_OK)
          {
            refused++;
            if (f != 0xD0D0D0D0u)
              kept = 0;
          }
      }
    DP_CHECK (agree);
    DP_CHECK (reports);
    DP_CHECK (kept);
    DP_CHECK (refused > 0); /* the premise: corruptions are refused */
    uint64_t f = 0;
    DP_CHECK (dp_f32_framer_state_frames (&b, blob, &f) == DP_OK && f == 5);
    free (blob);
    free (bad);
    dp_f32_destroy (ra);
    dp_f32_destroy (rb);
  }

  free (x);
  DP_TEST_END ("test_framer_core");
}
