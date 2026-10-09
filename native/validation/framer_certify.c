/**
 * @file framer_certify.c
 * @brief The measurements the ring framer's certification report is built
 * from.
 *
 * The framer is a face of the ring (`DECLARE_DP_BUFFER_FRAMES`, buffer.h) and
 * has no Python binding, so its evidence follows
 * `docs/dev/contributing/validation.md` "Certifying a component with no
 * binding": **this file measures, and
 * `src/doppler/tests/validation/framer/validate.py` renders and asserts.**
 * Nothing here decides whether a number is acceptable.
 *
 * Run with no arguments for a readable run (`make validate-c`), or with
 * `--emit` for the CSV blocks the validator parses.
 *
 * ## Against an oracle that shares no code with the framer
 *
 * Frame *k* of a stream `x` is `x[k*hop .. k*hop + n)`, zero beyond the end.
 * Every frame the framer hands out is compared with that slice, sample by
 * sample, as it is produced. The oracle is the specification in the header,
 * not another path through the same code, so a defect the framer's own paths
 * share cannot hide.
 *
 * ## What it adds to `native/tests/test_framer_core.c`
 *
 * The test pins each claim at a size a person can read. This runs the same
 * claims at the scale a statistic needs: a couple of hundred thousand samples
 * through each of seventy-two partitions per shape, every cut point of a
 * snapshot, every length of a flush. A pass is a count of zero, so the
 * validator can assert it and the report can print it.
 */
#include "doppler/buffer/buffer.h"

#include "dp_rng_test.h"

#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stamped here for the same reason test_framer_core.c stamps it: this program
   builds on buffer.h alone. */
DECLARE_DP_BUFFER_FRAMES (f32, float, float _Complex)

typedef float _Complex cf;

/** @brief Sample i of the test stream: distinct per position, never zero. */
static cf
sample (size_t i)
{
  return (float)(i + 1) + 0.5f * I * (float)(i % 7 + 1);
}

/** @brief What row `row` element `j` MUST hold: the slice, zero past the end.
 */
static cf
expect (size_t len, size_t hop, size_t row, size_t j)
{
  const size_t i = row * hop + j;
  return i < len ? sample (i) : (cf)0.0f;
}

typedef struct
{
  size_t rows;      /**< Frames handed out. */
  size_t bad_rows;  /**< Frames that differ from the oracle anywhere. */
  size_t leftover;  /**< Largest samples left buffered after a drain. */
  size_t stalls;    /**< feed() calls that took less than was offered. */
  size_t offered;   /**< Samples offered in all. */
  size_t taken;     /**< Samples accepted in all. */
  size_t over_room; /**< Feeds that yielded more frames than the room given. */
} run_t;

/**
 * A ring, or the harness stops: a failed allocation would otherwise be used
 * as a framer's ring and read as a certification result.
 */
static dp_f32_t *
make_ring (size_t capacity)
{
  dp_f32_t *ring = dp_f32_create (capacity);
  if (!ring)
    {
      fprintf (stderr, "framer_certify: ring allocation failed\n");
      exit (1);
    }
  return ring;
}

/**
 * Initialise a framer, or the harness stops. A refused init leaves `*fr`
 * unwritten, so carrying on would certify a framer nothing configured; it is
 * a harness failure, never a result.
 */
static void
start (dp_f32_framer_t *fr, dp_f32_t *ring, size_t n, size_t hop)
{
  if (dp_f32_framer_init (fr, ring, n, hop) != DP_OK)
    {
      fprintf (stderr, "framer_certify: framer_init refused n=%zu hop=%zu\n",
               n, hop);
      exit (1);
    }
}

/**
 * Feed `len` samples in pieces chosen by `chunk` (0 = random 1..2n+1), with
 * room for `room` frames per feed (0 = unlimited), draining after each feed,
 * and check every frame against the oracle as it comes out.
 */
static run_t
run (size_t n, size_t hop, size_t len, size_t chunk, size_t room,
     uint32_t *rng)
{
  run_t           r    = { 0 };
  dp_f32_t       *ring = make_ring (4 * n + 64);
  dp_f32_framer_t fr;
  cf             *in = malloc (len * sizeof *in);
  if (!in)
    {
      fprintf (stderr, "framer_certify: setup failed\n");
      exit (1);
    }
  start (&fr, ring, n, hop);
  for (size_t i = 0; i < len; i++)
    in[i] = sample (i);

  const size_t max_frames = room ? room : (size_t)-1;
  for (size_t at = 0; at < len;)
    {
      size_t m = chunk ? chunk : 1 + dp_xs32 (rng) % (2 * n + 1);
      if (m > len - at)
        m = len - at;
      for (size_t done = 0; done < m;)
        {
          const size_t took = dp_f32_framer_feed_view (&fr, in + at + done,
                                                       m - done, max_frames);
          r.offered += m - done;
          r.taken += took;
          if (took < m - done)
            r.stalls++;
          done += took;
          const cf *f;
          size_t    yielded = 0;
          while ((f = dp_f32_framer_next_view (&fr)) != NULL)
            {
              int ok = 1;
              for (size_t j = 0; j < n && ok; j++)
                if (f[j] != expect (len, hop, r.rows, j))
                  ok = 0;
              r.bad_rows += !ok;
              r.rows++;
              yielded++;
            }
          /* The room is a PROMISE: feed admits no more input than yields
             this many frames. Draining everything would hide a breach. */
          if (room && yielded > room)
            r.over_room++;
          dp_f32_framer_settle (&fr);
          const size_t left = dp_f32_available (ring);
          if (left > r.leftover)
            r.leftover = left;
          if (took == 0 && room == 0)
            break; /* cannot happen with unlimited room; do not spin */
        }
      at += m;
    }
  free (in);
  dp_f32_destroy (ring);
  return r;
}

/* ── §A  chunk invariance, and the carry bound ──────────────────────────── */

typedef struct
{
  size_t n, hop, len;
} shape_t;

static void
sweep_invariance (int emit)
{
  static const shape_t shapes[] = {
    { 1024, 256, 200003 },  { 1000, 250, 200003 }, { 1024, 1024, 200003 },
    { 4096, 1000, 200003 }, { 257, 1, 20011 },     { 64, 63, 100003 },
    { 7, 3, 100003 },       { 1, 1, 50021 },
  };
  uint32_t rng = 20261008u;
  if (emit)
    printf ("# invariance\nn,hop,length,partitions,rows,bad_partitions,"
            "max_leftover\n");
  else
    printf ("  invariance  n   hop  partitions  rows  bad  max leftover\n");
  for (size_t s = 0; s < sizeof shapes / sizeof *shapes; s++)
    {
      const shape_t sh = shapes[s];
      size_t        sizes[16], ns = 0;
      sizes[ns++] = 1;
      sizes[ns++] = 7;
      if (sh.n > 1)
        sizes[ns++] = sh.n - 1;
      sizes[ns++]  = sh.n;
      sizes[ns++]  = sh.n + 1;
      sizes[ns++]  = 3 * sh.n + 5;
      sizes[ns++]  = 65537;
      sizes[ns++]  = sh.len; /* one chunk */
      size_t parts = 0, bad = 0, leftover = 0, rows = 0;
      for (size_t p = 0; p < ns + 64; p++)
        {
          const run_t r
              = run (sh.n, sh.hop, sh.len, p < ns ? sizes[p] : 0, 0, &rng);
          parts++;
          rows = r.rows;
          const size_t want
              = sh.len >= sh.n ? (sh.len - sh.n) / sh.hop + 1 : 0;
          bad += (r.bad_rows != 0 || r.rows != want);
          if (r.leftover > leftover)
            leftover = r.leftover;
        }
      if (emit)
        printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", sh.n, sh.hop, sh.len, parts,
                rows, bad, leftover);
      else
        printf ("  %10s %4zu %5zu  %9zu  %5zu  %3zu  %zu (< n = %zu)\n", "",
                sh.n, sh.hop, parts, rows, bad, leftover, sh.n);
    }
}

/* ── §B  backpressure loses nothing ─────────────────────────────────────── */

static void
sweep_backpressure (int emit)
{
  static const size_t chunks[] = { 1, 7, 1000, 5000 };
  static const size_t rooms[]  = { 1, 2, 7 };
  const size_t        n = 1024, hop = 256, len = 100003;
  uint32_t            rng = 1u;
  if (emit)
    printf ("\n# backpressure\nn,hop,chunk,room,offered,taken,stalls,rows,"
            "bad_rows,over_room,max_leftover\n");
  else
    printf ("\n  backpressure  chunk room  offered   taken  stalls  rows  bad "
            " over-room  carry\n");
  for (size_t c = 0; c < sizeof chunks / sizeof *chunks; c++)
    for (size_t k = 0; k < sizeof rooms / sizeof *rooms; k++)
      {
        const run_t r = run (n, hop, len, chunks[c], rooms[k], &rng);
        if (emit)
          printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", n, hop,
                  chunks[c], rooms[k], r.offered, r.taken, r.stalls, r.rows,
                  r.bad_rows, r.over_room, r.leftover);
        else
          printf ("  %12s %5zu %4zu  %7zu %7zu  %6zu  %4zu  %zu  %9zu  %zu\n",
                  "", chunks[c], rooms[k], r.offered, r.taken, r.stalls,
                  r.rows, r.bad_rows, r.over_room, r.leftover);
      }
}

/* ── §C  flush: on the grid, once, only if owed, and it restarts ────────── */

static void
sweep_flush (int emit)
{
  static const shape_t shapes[]
      = { { 8, 3, 0 },   { 8, 8, 0 }, { 8, 1, 0 }, { 16, 5, 0 },
          { 64, 16, 0 }, { 7, 7, 0 }, { 1, 1, 0 } };
  if (emit)
    printf ("\n# flush\nn,hop,lengths,emitted,wrong_decision,off_grid,"
            "second_nonzero,bad_restart\n");
  else
    printf ("\n  flush  n  hop  lengths  emitted  wrong  off-grid  2nd  "
            "restart\n");
  for (size_t s = 0; s < sizeof shapes / sizeof *shapes; s++)
    {
      const size_t n = shapes[s].n, hop = shapes[s].hop;
      size_t lengths = 0, emitted = 0, wrong = 0, off = 0, second = 0, rs = 0;
      for (size_t len = 0; len <= 4 * n + 3; len++)
        {
          dp_f32_t       *ring = make_ring (4 * n + 8);
          dp_f32_framer_t fr;
          start (&fr, ring, n, hop);
          cf *in = malloc ((len + n) * sizeof *in);
          for (size_t i = 0; i < len + n; i++)
            in[i] = sample (i);
          dp_f32_framer_feed_view (&fr, in, len, (size_t)-1);
          size_t rows = 0;
          while (dp_f32_framer_next_view (&fr))
            rows++;
          dp_f32_framer_settle (&fr);
          const size_t covered = rows ? (rows - 1) * hop + n : 0;
          cf           row[64];
          const int    got = dp_f32_framer_flush_view (&fr, row);
          lengths++;
          emitted += got == 1;
          wrong += (got == 1) != (len > covered);
          if (got == 1)
            for (size_t j = 0; j < n; j++)
              if (row[j] != expect (len, hop, rows, j))
                {
                  off++;
                  break;
                }
          second += dp_f32_framer_flush_view (&fr, row) != 0;
          /* restarted: the next n samples are row 0 of a new stream */
          dp_f32_framer_feed_view (&fr, in, n, (size_t)-1);
          const cf *f = dp_f32_framer_next_view (&fr);
          if (!f || memcmp (f, in, n * sizeof *f) != 0)
            rs++;
          free (in);
          dp_f32_destroy (ring);
        }
      if (emit)
        printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", n, hop, lengths, emitted,
                wrong, off, second, rs);
      else
        printf ("  %5s %2zu %4zu  %7zu  %7zu  %5zu  %8zu  %3zu  %zu\n", "", n,
                hop, lengths, emitted, wrong, off, second, rs);
    }
}

/* ── §D  the snapshot: shape-sized, restorable anywhere, bit-exact ──────── */

static void
sweep_snapshot (int emit)
{
  static const shape_t shapes[] = { { 8, 3, 0 }, { 16, 5, 0 }, { 64, 16, 0 },
                                    { 7, 7, 0 }, { 1, 1, 0 },  { 33, 1, 0 } };
  const size_t         total    = 700;
  if (emit)
    printf ("\n# snapshot\nn,hop,cuts,distinct_sizes,resume_bad,corrupt_tried,"
            "corrupt_refused\n");
  else
    printf ("\n  snapshot  n  hop  cuts  sizes  resume-bad  corrupt "
            "tried/refused\n");
  for (size_t s = 0; s < sizeof shapes / sizeof *shapes; s++)
    {
      const size_t n = shapes[s].n, hop = shapes[s].hop;
      size_t       cuts = 0, resume_bad = 0, tried = 0, refused = 0;
      size_t       first_size = 0, distinct = 0;
      cf           in[1000];
      for (size_t i = 0; i < total; i++)
        in[i] = sample (i);
      for (size_t cut = 0; cut <= 3 * n + 3 && cut <= total; cut++)
        {
          dp_f32_t *ra = make_ring (4 * n + 8), *rb = make_ring (4 * n + 8);
          dp_f32_framer_t a, b;
          start (&a, ra, n, hop);
          start (&b, rb, n, hop);
          size_t rows = 0, bad = 0;
          /* run `a` to the cut, draining as it goes */
          dp_f32_framer_feed_view (&a, in, cut, (size_t)-1);
          const cf *f;
          while ((f = dp_f32_framer_next_view (&a)) != NULL)
            {
              for (size_t j = 0; j < n; j++)
                bad += f[j] != expect (total, hop, rows, j);
              rows++;
            }
          dp_f32_framer_settle (&a);
          const size_t bytes = dp_f32_framer_state_bytes (&a);
          if (cut == 0)
            first_size = bytes;
          if (bytes != first_size)
            distinct++;
          void *blob = malloc (bytes);
          dp_f32_framer_get_state (&a, blob);
          /* hand the state to a FRESH framer and finish the stream on it in
             pieces of 5, while the rest of the stream is the oracle */
          dp_f32_framer_set_state (&b, blob);
          const size_t rows_at_cut = rows;
          for (size_t at = cut; at < total; at += 5)
            {
              const size_t m = total - at < 5 ? total - at : 5;
              dp_f32_framer_feed_view (&b, in + at, m, (size_t)-1);
              while ((f = dp_f32_framer_next_view (&b)) != NULL)
                {
                  for (size_t j = 0; j < n; j++)
                    bad += f[j] != expect (total, hop, rows, j);
                  rows++;
                }
              dp_f32_framer_settle (&b);
            }
          const size_t want = (total - n) / hop + 1;
          resume_bad += (bad != 0 || rows != want || rows_at_cut > want);
          /* a snapshot whose written counter is wrong is refused */
          ((uint64_t *)((char *)blob + sizeof (dp_state_hdr_t)
                        + sizeof (uint64_t) + (n - 1) * 2 * sizeof (float)))[0]
              ^= 0x5555u;
          tried++;
          refused += dp_f32_framer_set_state (&b, blob) == DP_ERR_INVALID;
          cuts++;
          free (blob);
          dp_f32_destroy (ra);
          dp_f32_destroy (rb);
        }
      if (emit)
        printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", n, hop, cuts, distinct + 1,
                resume_bad, tried, refused);
      else
        printf ("  %8s %2zu %4zu  %4zu  %5zu  %9zu  %zu / %zu\n", "", n, hop,
                cuts, distinct + 1, resume_bad, tried, refused);
    }
}

/* ── §E  an undrained framer is refused, never overflowed ───────────────── */

static void
sweep_refusal (int emit)
{
  static const shape_t shapes[]
      = { { 8, 3, 0 }, { 8, 8, 0 }, { 16, 5, 0 }, { 64, 16, 0 }, { 7, 1, 0 } };
  if (emit)
    printf ("\n# refusal\nn,hop,attempts,flush_refused,state_changed,"
            "snapshot_nonzero,snapshot_accepted\n");
  else
    printf ("\n  refusal  n  hop  tried  flush-refused  changed  snap!=0  "
            "snap-ok\n");
  for (size_t s = 0; s < sizeof shapes / sizeof *shapes; s++)
    {
      const size_t n = shapes[s].n, hop = shapes[s].hop;
      size_t tried = 0, refused = 0, changed = 0, nonzero = 0, accepted = 0;
      for (size_t extra = 0; extra < 6 * n; extra++)
        {
          dp_f32_t *ra = make_ring (16 * n + 8), *rb = make_ring (16 * n + 8);
          dp_f32_framer_t a, b;
          start (&a, ra, n, hop);
          start (&b, rb, n, hop);
          cf in[1024];
          for (size_t i = 0; i < 1024; i++)
            in[i] = sample (i);
          /* feed WITHOUT draining: whole frames stay buffered */
          dp_f32_framer_feed_view (&a, in, n + extra, (size_t)-1);
          if (dp_f32_framer_drained (&a))
            {
              dp_f32_destroy (ra);
              dp_f32_destroy (rb);
              continue; /* fewer than a frame: nothing undrained to refuse */
            }
          tried++;
          cf           row[64];
          const size_t before = dp_f32_available (ra);
          refused += dp_f32_framer_flush_view (&a, row) < 0;
          changed += dp_f32_available (ra) != before;
          const size_t   bytes = dp_f32_framer_state_bytes (&a);
          unsigned char *blob  = malloc (bytes);
          memset (blob, 0xA5, bytes);
          dp_f32_framer_get_state (&a, blob);
          for (size_t i = 0; i < bytes; i++)
            if (blob[i] != 0)
              {
                nonzero++;
                break;
              }
          accepted += dp_f32_framer_set_state (&b, blob) == DP_OK;
          free (blob);
          dp_f32_destroy (ra);
          dp_f32_destroy (rb);
        }
      if (emit)
        printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", n, hop, tried, refused,
                changed, nonzero, accepted);
      else
        printf ("  %7s %2zu %4zu  %5zu  %13zu  %7zu  %7zu  %zu\n", "", n, hop,
                tried, refused, changed, nonzero, accepted);
    }
}

int
main (int argc, char **argv)
{
  const int emit = argc > 1 && strcmp (argv[1], "--emit") == 0;
  if (!emit)
    printf ("framer -- certification runs (oracle: slicing the input)\n\n");
  sweep_invariance (emit);
  sweep_backpressure (emit);
  sweep_flush (emit);
  sweep_snapshot (emit);
  sweep_refusal (emit);
  if (!emit)
    printf ("\nRun with --emit for the CSV the validator parses.\n");
  return 0;
}
