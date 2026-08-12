/*
 * C-level tests for the lossless capture
 * (dp_tlm_capture/dp_tlm_capture_core.h).
 *
 * The headline test is `saturation`: emit EXACTLY the per-block bound, every
 * block, for many blocks, and assert nothing is dropped. That is the whole
 * claim the design makes, so it is the one that must be impossible to pass by
 * accident -- shrinking the ring one record below the bound turns it red.
 *
 * Everything else pins the machinery around it: the boundary reached through
 * set_now, the ping-pong handoff and its backpressure, the file round-trip and
 * its sidecar, the self-heal when probes appear late, and the loud failure
 * when the caller breaks the block contract.
 */
#include "dp_test.h"
#include "dp_tlm_capture/dp_tlm_capture_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A scratch path in the build tree; every test that writes one removes it
   first, so a failing earlier block cannot poison a later assertion. */
static const char *
scratch (const char *leaf)
{
  static char buf[256];
  snprintf (buf, sizeof buf, "tlm_capture_%s.tlm", leaf);
  remove (buf);
  char meta[288];
  snprintf (meta, sizeof meta, "%s-meta", buf);
  remove (meta);
  return buf;
}

/* Whole-file slurp, for checking the raw records and the JSON sidecar. */
static char *
slurp (const char *path, size_t *len)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  fseek (f, 0, SEEK_END);
  long n = ftell (f);
  fseek (f, 0, SEEK_SET);
  char *b = (char *)malloc ((size_t)n + 1);
  if (!b)
    {
      fclose (f);
      return NULL;
    }
  size_t got = fread (b, 1, (size_t)n, f);
  fclose (f);
  b[got] = '\0';
  if (len)
    *len = got;
  return b;
}

/* The smallest ring this platform will hand out. buffer.h rounds a sub-page
   request up to the page minimum, so it is 256 records on a 4 KiB page and
   1024 on a 16 KiB one (macOS). Several blocks below need a bound that
   genuinely EXCEEDS the floor to be testing anything at all, so derive it
   rather than hard-coding a number that is only true on one platform. */
static size_t
ring_floor (void)
{
  dp_tlm_t *t = dp_tlm_create (1);
  size_t    n = dp_tlm_capacity (t);
  dp_tlm_destroy (t);
  return n;
}

int
main (void)
{
  const size_t FLOOR = ring_floor ();
  /* ── THE claim: emit the bound every block, forever, and lose nothing ──
     No sleeps, no thread timing, no safety factor -- the ring is sized to
     exactly one block's worth and drained at every boundary, so overflow is
     arithmetically impossible rather than merely unlikely. Shrink the ring
     one record below the bound and this goes red. ─────────────────────────*/
  {
    /* BLOCK is chosen so the bound (3 x FLOOR) EXCEEDS the smallest ring the
       allocator will hand out. Anything smaller and the ring is already big
       enough by accident, and this stops testing the sizing at all -- it
       would stay green with dp_tlm_resize deleted. */
    const size_t BLOCK  = FLOOR;
    const size_t BLOCKS = 50;
    dp_tlm_t    *t      = dp_tlm_create (1);
    int          a      = dp_tlm_probe (t, "a", 1);
    int          b      = dp_tlm_probe (t, "b", 1);
    int          c3     = dp_tlm_probe (t, "c", 1);
    DP_CHECK (t && a == 0 && b == 1 && c3 == 2);

    size_t bound = dp_tlm_block_bound (t, BLOCK);
    DP_CHECK (bound == 3 * BLOCK);

    DP_CHECK (dp_tlm_capacity (t) < bound); /* as handed out: too small */
    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, BLOCK, NULL, NULL);
    DP_CHECK (cap != NULL);
    /* Opening GREW the ring to the bound; the caller never picked a number. */
    DP_CHECK (dp_tlm_capacity (t) >= bound);

    for (size_t blk = 0; blk < BLOCKS; blk++)
      {
        dp_tlm_set_now (t, blk * BLOCK); /* boundary: drains the last block */
        for (size_t i = 0; i < BLOCK; i++)
          {
            dp_tlm_emit (t, a, (double)i);
            dp_tlm_emit (t, b, (double)i);
            dp_tlm_emit (t, c3, (double)i);
          }
      }
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);
    DP_CHECK (dp_tlm_capture_dropped (cap) == 0);
    DP_CHECK (dp_tlm_capture_count (cap) == BLOCKS * bound);
    DP_CHECK (dp_tlm_dropped (t) == 0);

    /* In memory mode the capture IS the emission sequence, contiguous and in
       order, with no concatenation step for the caller. */
    const dp_tlm_rec_t *recs = dp_tlm_capture_records (cap);
    DP_CHECK (recs != NULL);
    int ok = 1;
    /* Bound the walk by what was ACTUALLY captured, not by what should have
       been: a short capture is a failure the CHECKs above already report, and
       walking to the expected length would turn it into a segfault instead of
       a diagnosis. */
    size_t have = dp_tlm_capture_count (cap);
    for (size_t k = 0; k < have && recs; k++)
      {
        size_t blk = k / bound, within = (k % bound) / 3;
        if (recs[k].n != (uint64_t)(blk * BLOCK)
            || recs[k].probe != (uint16_t)(k % 3)
            || recs[k].value != (float)within)
          ok = 0;
      }
    DP_CHECK (ok);

    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
  }

  /* ── set_now IS the boundary: an existing loop becomes lossless with no
     call-site change at all. Without the delegation the ring (sized to one
     block) overflows on block 2. ───────────────────────────────────────────
   */
  {
    const size_t BLK  = 100;
    const size_t NBLK = 50;
    dp_tlm_t    *t    = dp_tlm_create (1);
    int          id   = dp_tlm_probe (t, "x", 1);
    /* The point of this block is that the WHOLE run does not fit in the ring,
       so only a per-block drain can keep it lossless. Assert that rather than
       a specific capacity -- the floor is platform-dependent. */
    DP_CHECK (dp_tlm_capacity (t) < BLK * NBLK);

    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, BLK, NULL, NULL);
    DP_CHECK (cap != NULL);
    for (size_t blk = 0; blk < NBLK; blk++)
      {
        dp_tlm_set_now (t, (uint64_t)blk);
        for (size_t i = 0; i < BLK; i++)
          dp_tlm_emit (t, id, 1.0);
      }
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);
    DP_CHECK (dp_tlm_capture_count (cap) == BLK * NBLK);

    /* And once closed, set_now goes back to being a bare assignment -- it must
       not keep calling into a finished capture. */
    dp_tlm_set_now (t, 99);
    DP_CHECK (dp_tlm_capture_count (cap) == BLK * NBLK);
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
  }

  /* ── file mode: the round-trip is byte-exact, and the writer thread's
     ping-pong never reorders or loses a handoff ────────────────────────────
   */
  {
    const char *path = scratch ("roundtrip");
    dp_tlm_t   *t    = dp_tlm_create (256);
    int         id   = dp_tlm_probe (t, "f", 1);

    /* The time base is BORROWED from the pipeline's clock, never re-declared
       as a private fs/t0 pair. resync=0 leaves the epoch unset, so the sidecar
       must state fs and stay silent about the epoch. */
    dp_sample_clock_t clk;
    dp_sample_clock_init (&clk, 1e6, 0);
    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 128, path, &clk);
    DP_CHECK (cap != NULL);
    const int BLOCKS = 300; /* many stage swaps -> many writer handoffs */
    for (int blk = 0; blk < BLOCKS; blk++)
      {
        dp_tlm_set_now (t, (uint64_t)blk);
        for (int i = 0; i < 128; i++)
          dp_tlm_emit (t, id, (double)(blk * 128 + i));
      }
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);
    DP_CHECK (dp_tlm_capture_count (cap) == (size_t)BLOCKS * 128);
    /* File mode: the file is the capture, so there is nothing in memory. */
    DP_CHECK (dp_tlm_capture_records (cap) == NULL);

    size_t len  = 0;
    char  *blob = slurp (path, &len);
    DP_CHECK (blob != NULL);
    DP_CHECK (len == (size_t)BLOCKS * 128 * sizeof (dp_tlm_rec_t));
    const dp_tlm_rec_t *r  = (const dp_tlm_rec_t *)blob;
    int                 ok = 1;
    /* Walk what the file actually holds, so a short file is diagnosed by the
       length CHECK above rather than by reading off the end of the buffer. */
    size_t nrec = len / sizeof (dp_tlm_rec_t);
    for (size_t k = 0; k < nrec && blob; k++)
      if (r[k].n != (uint64_t)(k / 128) || r[k].value != (float)k)
        ok = 0;
    DP_CHECK (ok);
    free (blob);

    /* The sidecar carries what the records cannot. */
    char meta[288];
    snprintf (meta, sizeof meta, "%s-meta", path);
    char *js = slurp (meta, NULL);
    DP_CHECK (js != NULL);
    if (js)
      {
        DP_CHECK (strstr (js, "\"records\": 38400") != NULL);
        DP_CHECK (strstr (js, "\"dropped\": 0") != NULL);
        DP_CHECK (strstr (js, "\"block_samples\": 128") != NULL);
        DP_CHECK (strstr (js, "\"\\\"f\\\"\"")
                  == NULL); /* no double-escaping */
        DP_CHECK (strstr (js, "\"f\": 0") != NULL);
        /* Self-describing: a reader needs no doppler code to parse the file.
         */
        DP_CHECK (strstr (js, "\"dtype\"") != NULL);
        DP_CHECK (strstr (js, "[\"value\", \"<f4\"]") != NULL);
        /* fs was stated, t0 was not -- an unstated value is OMITTED, never
           fabricated as a plausible-looking zero. */
        DP_CHECK (strstr (js, "\"fs\": 1000000") != NULL);
        DP_CHECK (strstr (js, "\"epoch_real_ns\"") == NULL);
        free (js);
      }
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
    remove (path);
    remove (meta);
  }

  /* ── the sidecar's probe table is COMPLETE: every registered probe, with
     its id, including one that never emitted ───────────────────────────── */
  {
    const char *path = scratch ("probes");
    dp_tlm_t   *t    = dp_tlm_create (256);
    DP_CHECK (dp_tlm_probe (t, "rx.car.e", 1) == 0);
    DP_CHECK (dp_tlm_probe (t, "rx.car.freq", 1) == 1);
    DP_CHECK (dp_tlm_probe (t, "rx.sync.mu", 4) == 2);

    /* A clock that exists but states NO rate -- raw and CSV sources carry
       none. "rate unknown" and "exactly 0 Hz" must not be the same bytes, so
       the key is omitted rather than written as a confident zero. */
    dp_sample_clock_t clk;
    dp_sample_clock_init (&clk, 0.0, 0);
    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 16, path, &clk);
    dp_tlm_set_now (t, 0);
    dp_tlm_emit (t, 0, 1.0); /* only probe 0 ever fires */
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);

    char meta[288];
    snprintf (meta, sizeof meta, "%s-meta", path);
    char *js = slurp (meta, NULL);
    DP_CHECK (js != NULL);
    if (js)
      {
        DP_CHECK (strstr (js, "\"rx.car.e\": 0") != NULL);
        DP_CHECK (strstr (js, "\"rx.car.freq\": 1") != NULL);
        DP_CHECK (strstr (js, "\"rx.sync.mu\": 2") != NULL);
        DP_CHECK (strstr (js, "\"fs\"") == NULL);
        /* An omitted probe would make a record's `probe` field unresolvable,
           so silence about a silent probe is not acceptable. */
        free (js);
      }
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
    remove (path);
    remove (meta);
  }

  /* ── an ANCHORED epoch IS recorded. The other half of the has_anchor gate:
     without this, "always omit the epoch" would pass every other block. ─── */
  {
    const char *path = scratch ("anchored");
    dp_tlm_t   *t    = dp_tlm_create (1);
    dp_tlm_probe (t, "a", 1);

    dp_sample_clock_t clk;
    dp_sample_clock_init (&clk, 1e6, 0);
    /* Ground truth off a stream header -- the first track() always adopts. */
    DP_CHECK (dp_sample_clock_track (&clk, 1234567890000000000ull, 0, 1000)
              != 0);
    DP_CHECK (clk.has_anchor != 0);

    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 16, path, &clk);
    dp_tlm_set_now (t, 0);
    dp_tlm_emit (t, 0, 1.0);
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);

    char meta[288];
    snprintf (meta, sizeof meta, "%s-meta", path);
    char *js = slurp (meta, NULL);
    DP_CHECK (js != NULL);
    if (js)
      {
        DP_CHECK (strstr (js, "\"epoch_real_ns\": 1234567890000000000")
                  != NULL);
        free (js);
      }
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
    remove (path);
    remove (meta);
  }

  /* ── self-heal: probes registered AFTER open raise the bound, and the next
     boundary grows the ring before the wider block can cost a record ────── */
  {
    const size_t BLK = FLOOR / 4;
    dp_tlm_t    *t   = dp_tlm_create (1);
    DP_CHECK (dp_tlm_probe (t, "p0", 1) == 0);

    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, BLK, NULL, NULL);
    DP_CHECK (cap != NULL);
    size_t cap0 = dp_tlm_capacity (t);

    /* A late attach -- a second instrumented object joining the pipeline. */
    for (int i = 1; i < 8; i++)
      {
        char nm[8];
        snprintf (nm, sizeof nm, "p%d", i);
        DP_CHECK (dp_tlm_probe (t, nm, 1) == i);
      }
    dp_tlm_set_now (t, 0); /* boundary: notices the bound moved, grows */
    DP_CHECK (dp_tlm_capacity (t) > cap0);
    DP_CHECK (dp_tlm_capacity (t) >= dp_tlm_block_bound (t, BLK));

    for (int blk = 0; blk < 20; blk++)
      {
        dp_tlm_set_now (t, (uint64_t)blk);
        for (size_t i = 0; i < BLK; i++)
          for (int p = 0; p < 8; p++)
            dp_tlm_emit (t, p, 1.0);
      }
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);
    DP_CHECK (dp_tlm_capture_count (cap) == 20 * BLK * 8);
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
  }

  /* ── growing must not eat what is already in the ring. The resize replaces
     the ring outright, so doing it BEFORE the drain would discard exactly the
     records the boundary exists to collect. ────────────────────────────────
   */
  {
    const size_t BLK = FLOOR / 4;
    dp_tlm_t    *t   = dp_tlm_create (1);
    DP_CHECK (dp_tlm_probe (t, "q0", 1) == 0);
    dp_tlm_capture_t *cap  = dp_tlm_capture_open (t, BLK, NULL, NULL);
    size_t            cap0 = dp_tlm_capacity (t);

    dp_tlm_set_now (t, 0);
    for (size_t i = 0; i < BLK; i++)
      dp_tlm_emit (t, 0, (double)i); /* in the ring, not yet drained */

    /* Register enough probes that the new bound (8 x BLK = 2 x FLOOR) EXCEEDS
       the current ring -- otherwise the resize is a no-op and this block
       cannot tell drain-then-grow from grow-then-drain. */
    for (int i = 1; i < 8; i++)
      {
        char nm[8];
        snprintf (nm, sizeof nm, "q%d", i);
        dp_tlm_probe (t, nm, 1);
      }
    DP_CHECK (dp_tlm_block_bound (t, BLK) > cap0);
    dp_tlm_set_now (t, BLK); /* boundary: must drain FIRST, then grow */
    DP_CHECK (dp_tlm_capacity (t) > cap0);

    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);
    DP_CHECK (dp_tlm_capture_count (cap) == BLK); /* not 0 */
    const dp_tlm_rec_t *recs = dp_tlm_capture_records (cap);
    DP_CHECK (recs && recs[0].value == 0.0f
              && recs[BLK - 1].value == (float)(BLK - 1));
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
  }

  /* ── breaking the block contract FAILS LOUDLY. A hole is not a smaller
     capture, it is a wrong one -- close() must refuse to call it fine. ──── */
  {
    dp_tlm_t *t = dp_tlm_create (256);
    DP_CHECK (dp_tlm_probe (t, "over", 1) == 0);
    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 8, NULL, NULL);
    DP_CHECK (cap != NULL);

    /* Declared block = 8, actual step = far more, with no boundary between:
       the ring was sized for 8 and overflows. */
    dp_tlm_set_now (t, 0);
    for (int i = 0; i < 100000; i++)
      dp_tlm_emit (t, 0, 1.0);

    DP_CHECK (dp_tlm_capture_close (cap) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_capture_dropped (cap) > 0);
    /* close() is idempotent and replays its verdict rather than "succeeding"
       the second time round. */
    DP_CHECK (dp_tlm_capture_close (cap) == DP_ERR_INVALID);
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
  }

  /* ── dropped is latched PER CAPTURE, not read raw off the monotonic
     context counter -- otherwise a fresh capture inherits an old hole ───── */
  {
    dp_tlm_t *t = dp_tlm_create (256);
    DP_CHECK (dp_tlm_probe (t, "flood", 1) == 0);
    for (size_t i = 0; i < dp_tlm_capacity (t) * 4; i++)
      dp_tlm_emit (t, 0, 1.0); /* no consumer: guaranteed overrun */
    DP_CHECK (dp_tlm_dropped (t) > 0);

    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 8, NULL, NULL);
    DP_CHECK (cap != NULL);
    dp_tlm_set_now (t, 0);
    dp_tlm_emit (t, 0, 1.0);
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK); /* THIS capture is clean */
    DP_CHECK (dp_tlm_capture_dropped (cap) == 0);
    dp_tlm_capture_destroy (cap);
    dp_tlm_destroy (t);
  }

  /* ── open() edges ─────────────────────────────────────────────────────── */
  {
    DP_CHECK (dp_tlm_capture_open (NULL, 16, NULL, NULL) == NULL);

    dp_tlm_t *t = dp_tlm_create (256);
    /* No probes -> no bound -> nothing meaningful to size against. */
    DP_CHECK (dp_tlm_capture_open (t, 16, NULL, NULL) == NULL);
    dp_tlm_probe (t, "z", 1);
    /* A zero block is the one number a caller must actually supply. */
    DP_CHECK (dp_tlm_capture_open (t, 0, NULL, NULL) == NULL);

    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 16, NULL, NULL);
    DP_CHECK (cap != NULL);
    /* A second capture would be a second consumer on an SPSC ring: silent
       corruption, not a slow path. Refuse it. */
    DP_CHECK (dp_tlm_capture_open (t, 16, NULL, NULL) == NULL);
    DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);
    /* Once closed the context is free again. */
    dp_tlm_capture_t *cap2 = dp_tlm_capture_open (t, 16, NULL, NULL);
    DP_CHECK (cap2 != NULL);
    dp_tlm_capture_destroy (cap2);
    dp_tlm_capture_destroy (cap);

    /* An unopenable path fails at open, not silently at close. */
    DP_CHECK (dp_tlm_capture_open (t, 16, "/nonexistent-dir-xyz/f.tlm", NULL)
              == NULL);
    DP_CHECK (t->capture == NULL); /* and leaves the context unarmed */
    dp_tlm_destroy (t);
  }

  /* ── NULL-safety across the surface ───────────────────────────────────── */
  {
    DP_CHECK (dp_tlm_capture_block (NULL) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_capture_close (NULL) == DP_ERR_INVALID);
    DP_CHECK (dp_tlm_capture_count (NULL) == 0);
    DP_CHECK (dp_tlm_capture_records (NULL) == NULL);
    DP_CHECK (dp_tlm_capture_dropped (NULL) == 0);
    dp_tlm_capture_destroy (NULL); /* must not crash */

    /* destroy() on an OPEN capture closes it first rather than leaking the
       writer thread onto a freed struct. */
    const char *path = scratch ("destroy_open");
    dp_tlm_t   *t    = dp_tlm_create (256);
    dp_tlm_probe (t, "d", 1);
    dp_tlm_capture_t *cap = dp_tlm_capture_open (t, 16, path, NULL);
    DP_CHECK (cap != NULL);
    dp_tlm_set_now (t, 0);
    dp_tlm_emit (t, 0, 1.0);
    dp_tlm_capture_destroy (cap);
    DP_CHECK (t->capture == NULL); /* and un-arms set_now on the way out */
    dp_tlm_set_now (t, 1);         /* must not touch the freed capture */
    dp_tlm_destroy (t);

    size_t len  = 0;
    char  *blob = slurp (path, &len);
    DP_CHECK (blob && len == sizeof (dp_tlm_rec_t)); /* flushed, not lost */
    free (blob);
    remove (path);
    char meta[288];
    snprintf (meta, sizeof meta, "%s-meta", path);
    remove (meta);
  }

  /* ── the copy-out drain (dp_tlm_capture_read) ──────────────────────────
     The zero-copy dp_tlm_capture_records() cannot be handed to a binding --
     it would be a pointer the capture frees underneath the caller -- so the
     copying twin is what the Python face binds. Same records, same order;
     that equivalence is the thing to pin. */
  {
    dp_tlm_t         *t  = dp_tlm_create (1 << 12);
    int               id = dp_tlm_probe (t, "x", 1);
    dp_tlm_capture_t *c  = dp_tlm_capture_open_memory (t, 8, NULL);
    DP_CHECK (c != NULL);

    for (int b = 0; b < 5; b++)
      {
        dp_tlm_set_now (t, (uint64_t)b * 8);
        dp_tlm_emit (t, id, (double)b);
      }
    DP_CHECK (dp_tlm_capture_close (c) == DP_OK);
    DP_CHECK (dp_tlm_capture_count (c) == 5);
    DP_CHECK (dp_tlm_capture_read_max_out (c) == 5);

    dp_tlm_rec_t got[8];
    /* n == 0 means "everything accumulated". */
    DP_CHECK (dp_tlm_capture_read (c, 0, got, 8) == 5);
    const dp_tlm_rec_t *view = dp_tlm_capture_records (c);
    DP_CHECK (view != NULL);
    DP_CHECK (memcmp (got, view, 5 * sizeof *got) == 0);
    for (int b = 0; b < 5; b++)
      {
        DP_CHECK (got[b].n == (uint64_t)b * 8);
        DP_CHECK (got[b].value == (float)b);
      }

    /* Clamped to the smaller of the request and the destination -- both
       directions, since either alone would look correct on the other's
       test. */
    DP_CHECK (dp_tlm_capture_read (c, 3, got, 8) == 3);
    DP_CHECK (dp_tlm_capture_read (c, 0, got, 2) == 2);
    DP_CHECK (dp_tlm_capture_read (NULL, 0, got, 8) == 0);

    DP_CHECK (dp_tlm_capture_destroy (c) == DP_OK);
    dp_tlm_destroy (t);
  }

  /* ── destroy REPORTS the verdict, it does not swallow it ───────────────
     A `with` block's exit and a garbage collection both land on destroy, so
     a hole that only close() could report would be unreachable from
     idiomatic Python. Both arms matter: a clean capture must stay quiet, or
     the loud one is just noise. */
  {
    dp_tlm_t         *t  = dp_tlm_create (1 << 12);
    int               id = dp_tlm_probe (t, "x", 1);
    dp_tlm_capture_t *c  = dp_tlm_capture_open_memory (t, 8, NULL);
    for (int b = 0; b < 4; b++)
      {
        dp_tlm_set_now (t, (uint64_t)b * 8);
        dp_tlm_emit (t, id, (double)b);
      }
    DP_CHECK (dp_tlm_capture_destroy (c) == DP_OK); /* clean: silent */
    dp_tlm_destroy (t);
  }
  {
    dp_tlm_t         *t  = dp_tlm_create (1 << 12);
    int               id = dp_tlm_probe (t, "x", 1);
    dp_tlm_capture_t *c  = dp_tlm_capture_open_memory (t, 8, NULL);
    /* Break the block contract: no boundary at all, far past the bound. */
    for (int i = 0; i < 20000; i++)
      dp_tlm_emit (t, id, (double)i);
    DP_CHECK (dp_tlm_dropped (t) > 0);
    /* Destroying WITHOUT closing first must still surface the hole. */
    DP_CHECK (dp_tlm_capture_destroy (c) == DP_ERR_INVALID);
    dp_tlm_destroy (t);
  }
  DP_CHECK (dp_tlm_capture_destroy (NULL) == DP_OK); /* NULL-safe, and quiet */

  DP_TEST_END ("test_dp_tlm_capture_core");
}
