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
        memcpy ((char *)blob + sizeof (dp_state_hdr_t) + sizeof (uint64_t)
                    + (N - 1) * 2 * sizeof (float),
                &bad, sizeof bad);
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

  free (x);
  DP_TEST_END ("test_framer_core");
}
