/*
 * test_spectrogram_core.c — the claims of spectrogram_core.h, one section
 * each, against external truths wherever one exists:
 *
 *   1. create refuses what the header says it refuses
 *   2. a row IS dp_psd_frame_db of its frame, bit for bit (G3)
 *   3. a full-scale tone on a bin reads 0 dBFS, every window (G3's level)
 *   4. rows_for is exact; push_max_out(n) is the room that takes all n, and
 *      one row less takes fewer and loses no row (a sweep)
 *   5. chunk invariance through dp_chunk_inv.h (G1)
 *   6. backpressure: a 1-row out, arbitrary chunks, resume from consumed(),
 *      equals one big push bit for bit (G7 and G1 in one); whole rows only;
 *      a tiny out takes exactly the input that completes no row
 *   7. flush on the hop grid, iff a sample is uncovered, then restart (G2)
 *   8. reset and pending
 *   9. rows are DC-centred: bin k at index nfft/2 + k, as PSD emits them
 *  10. the state blob: round trip, a mid-frame split resumed in a FRESH
 *      object bit for bit, fixed size, wrong hop refused (G4)
 *  11. the Python face's sizing: chunks shorter than a frame, each out sized
 *      by push_max_out (mostly 0), equal one big push bit for bit (G7)
 *  12. beta is ignored outside the Kaiser window
 *  13. an all-zero row reads exactly the kernel's -200 dB floor
 *  14. the blob: its size is a function of nfft alone, its envelope is
 *      'SPGM' version 1, it carries the stream position and nothing else,
 *      and set_state refuses a wrong magic, version, envelope size, nfft,
 *      hop or an impossible carry, leaving the object as it was (G4)
 * 14b. a different window or beta is NOT refused on restore: the rows that
 *      follow are the restoring object's window, not the blob's source's
 *  15. a row arrives with the push that delivers its last sample: zero
 *      latency in samples
 *
 * Fewer than nfft samples are held once a push returns (the carry bound,
 * G4's premise): rows_for(s, 0) is 0 exactly then, and it is checked after
 * every push of the chunk-invariance run (5) and the backpressure run (6).
 *
 * The rule push follows is the framer's feed contract: a sample is taken
 * unless it would complete a row out has no room for.
 */
#include "doppler/spectrogram/spectrogram_core.h"

#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NX 1000 /* stream length used throughout */

static float _Complex x[NX];

/* The PSD a row must equal: the header's own composition, stated once here
 * so a test that drifts from it fails loudly rather than agreeing. */
static dp_psd_state_t *
ref_psd (size_t nfft, int window, float beta)
{
  return dp_psd_create (nfft, 1.0, window, beta, 1, 1.0, 0, 0, 0.0);
}

/* Rows of a one-shot push of x[0..n): the reference for every partition.
 * Floats written; a failed create counts as a failure and writes 0, which
 * the caller's size check then refuses too. */
static size_t
oneshot (size_t nfft, size_t hop, int window, size_t n, float *out, size_t cap)
{
  dp_spectrogram_state_t *s
      = dp_spectrogram_create (nfft, hop, window, 7.5f, DP_SPECTROGRAM_DB);
  DP_CHECK (s != NULL);
  if (!s)
    return 0;
  size_t got = dp_spectrogram_push (s, x, n, out, cap);
  DP_CHECK (dp_spectrogram_consumed (s) == n);
  dp_spectrogram_destroy (s);
  return got;
}

/* Fill a reused output buffer with NaN first, so a short write leaves NaN
 * where a row should be rather than the last iteration's row. memcmp alone
 * would still pass two identical NaN patterns; what refuses a short write is
 * the comparison with rows_are_the_psd's freshly computed, finite rows. */
static void
fill_nan (float *p, size_t n)
{
  for (size_t i = 0; i < n; i++)
    p[i] = NAN;
}

/* The truth a stream's rows are held to WITHOUT the framer: `total` samples
 * make (total - nfft) / hop + 1 rows, and row r is PSD's dBFS of its own
 * slice x[r*hop .. r*hop + nfft). A one-shot reference shares the framer
 * with the object under test, so a framer defect common to both would pass
 * a comparison with it; it cannot pass this. 1 if `made` floats of `got`
 * are exactly those rows. */
static int
rows_are_the_psd (dp_psd_state_t *p, size_t nfft, size_t hop, size_t total,
                  const float *got, size_t made)
{
  float        ref[64];
  const size_t want = total >= nfft ? (total - nfft) / hop + 1 : 0;
  if (nfft > 64 || made != want * nfft)
    return 0;
  for (size_t r = 0; r < want; r++)
    {
      dp_psd_frame_db (p, x + r * hop, ref);
      if (memcmp (got + r * nfft, ref, nfft * sizeof *ref) != 0)
        return 0;
    }
  return 1;
}

/* ---- dp_chunk_inv.h wiring: .create / .process call the object itself ----
 */

typedef struct
{
  size_t nfft, hop;
  int    window;
} ci_cfg_t;

static void *
ci_create (void *arg)
{
  const ci_cfg_t *c = (const ci_cfg_t *)arg;
  return dp_spectrogram_create (c->nfft, c->hop, c->window, 7.5f,
                                DP_SPECTROGRAM_DB);
}

static void
ci_destroy (void *obj)
{
  dp_spectrogram_destroy ((dp_spectrogram_state_t *)obj);
}

static size_t
ci_process (void *obj, const void *in, size_t n, void *out, size_t out_cap)
{
  dp_spectrogram_state_t *s = (dp_spectrogram_state_t *)obj;
  size_t got = dp_spectrogram_push (s, (const float _Complex *)in, n,
                                    (float *)out, out_cap);
  /* the harness sizes out for the one-shot, so every chunk is taken whole */
  /* the carry bound: fewer than nfft held once a push returns, which is
     exactly when no row is due on the carry alone */
  if (dp_spectrogram_rows_for (s, 0) != 0)
    return (size_t)-1;
  return dp_spectrogram_consumed (s) == n ? got : (size_t)-1;
}

int
main (void)
{
  uint32_t rng = 0x5EC7u;
  for (size_t i = 0; i < NX; i++)
    x[i] = dp_cgauss (&rng);

  /* ---- 1. create refuses what the header says it refuses ------------- */
  {
    /* nfft 0 has no valid hop (1 <= hop <= nfft is empty), so it is refused
       whichever rule looks first; every other bad nfft is tried with a hop
       that IS valid -- 1 and nfft itself -- so the nfft rule alone refuses */
    DP_CHECK (dp_spectrogram_create (0, 1, 0, 0.0f, 0) == NULL);
    static const size_t bad_nfft[] = { 1, 3, 6, 12, 100, 1000 };
    for (size_t i = 0; i < sizeof bad_nfft / sizeof *bad_nfft; i++)
      {
        DP_CHECK (dp_spectrogram_create (bad_nfft[i], 1, 0, 0.0f, 0) == NULL);
        DP_CHECK (dp_spectrogram_create (bad_nfft[i], bad_nfft[i], 0, 0.0f, 0)
                  == NULL);
      }
    DP_CHECK (dp_spectrogram_create (8, 0, 0, 0.0f, 0) == NULL); /* hop 0 */
    DP_CHECK (dp_spectrogram_create (8, 9, 0, 0.0f, 0) == NULL); /* > nfft */
    DP_CHECK (dp_spectrogram_create (8, 4, -1, 0.0f, 0) == NULL);
    DP_CHECK (dp_spectrogram_create (8, 4, 4, 0.0f, 0) == NULL);
    DP_CHECK (dp_spectrogram_create (8, 4, 0, 0.0f, DP_SPECTROGRAM_POWER)
              == NULL);
    DP_CHECK (dp_spectrogram_create (8, 4, 0, 0.0f, 2) == NULL);
    /* The window is dp_psd_create's to refuse, and it refuses one that sums
       to zero: the symmetric Hann at nfft 2 is [0, 0], so every row would
       read NaN (#1911 (f)). */
    DP_CHECK (dp_spectrogram_create (2, 1, 0, 0.0f, 0) == NULL);
    /* ...and accepts the edges: the smallest nfft (with a window that has
       energy there), hop == 1, hop == nfft. Columns: nfft, hop, window. */
    static const size_t ok[][3]
        = { { 2, 1, 3 }, { 2, 2, 3 }, { 8, 1, 0 }, { 8, 8, 0 } };
    for (size_t i = 0; i < sizeof ok / sizeof *ok; i++)
      {
        dp_spectrogram_state_t *s = dp_spectrogram_create (
            ok[i][0], ok[i][1], (int)ok[i][2], 0.0f, 0);
        DP_CHECK (s != NULL);
        dp_spectrogram_destroy (s);
      }
    dp_spectrogram_destroy (NULL); /* a no-op, not a crash */
  }

  /* ---- 2. a row IS dp_psd_frame_db of its frame, bit for bit (G3) ---- */
  {
    for (int w = 0; w < 4; w++)
      {
        const size_t            nfft = 64, hop = 24;
        dp_spectrogram_state_t *s
            = dp_spectrogram_create (nfft, hop, w, 7.5f, 0);
        dp_psd_state_t *p = ref_psd (nfft, w, 7.5f);
        DP_REQUIRE (s != NULL && p != NULL);
        size_t rows = dp_spectrogram_rows_for (s, NX);
        DP_REQUIRE (rows == (NX - nfft) / hop + 1);
        float *got = (float *)malloc (rows * nfft * sizeof *got);
        float  want[64];
        DP_REQUIRE (got != NULL);
        DP_CHECK (dp_spectrogram_push (s, x, NX, got, rows * nfft)
                  == rows * nfft);
        int same = 1;
        for (size_t k = 0; k < rows; k++)
          {
            dp_psd_frame_db (p, x + k * hop, want);
            same &= memcmp (got + k * nfft, want, sizeof want) == 0;
          }
        DP_CHECK (same);
        free (got);
        dp_psd_destroy (p);
        dp_spectrogram_destroy (s);
      }
  }

  /* ---- 3. a full-scale tone on a bin reads 0 dBFS, every window ------- */
  {
    const size_t nfft = 256, bin = 37;
    float _Complex tone[256];
    for (size_t i = 0; i < nfft; i++)
      tone[i] = cexp (I * 2.0 * M_PI * (double)(bin * i) / (double)nfft);
    for (int w = 0; w < 4; w++)
      {
        dp_spectrogram_state_t *s
            = dp_spectrogram_create (nfft, nfft, w, 7.5f, 0);
        float row[256];
        DP_REQUIRE (s != NULL);
        DP_REQUIRE (dp_spectrogram_push (s, tone, nfft, row, nfft) == nfft);
        size_t peak = 0;
        for (size_t i = 1; i < nfft; i++)
          if (row[i] > row[peak])
            peak = i;
        DP_CHECK (peak == nfft / 2 + bin);    /* DC-centred */
        DP_CHECK_NEAR (row[peak], 0.0, 1e-3); /* its true level */
        dp_spectrogram_destroy (s);
      }
  }

  /* ---- 4. rows_for exact; push_max_out bounds one push ---------------- */
  {
    /* A sweep, not a point: every shape, from a fresh stream and from a
       carry, every n from nothing to several rows. Room push_max_out(n)
       takes all n and writes exactly that; one row less takes fewer, and
       pushing the rest after it loses no row. Both against the one-shot. */
    static float        want[128 * 16], got[128 * 16];
    static const size_t nffts[] = { 8, 16 };
    int exact = 1, bounded = 1, one_less = 1, n_one_less = 0, truth = 1;
    for (size_t f = 0; f < sizeof nffts / sizeof *nffts; f++)
      {
        const size_t    nfft    = nffts[f];
        const size_t    hops[4] = { 1, 3, nfft / 2, nfft };
        dp_psd_state_t *p       = ref_psd (nfft, 0, 7.5f);
        DP_REQUIRE (p != NULL);
        for (size_t h = 0; h < 4; h++)
          for (size_t pre = 0; pre < 2 * nfft; pre += 1 + pre / 3)
            for (size_t n = 0; n <= 4 * nfft + 3; n++)
              {
                const size_t hop = hops[h], total = pre + n;
                fill_nan (want, sizeof want / sizeof *want);
                fill_nan (got, sizeof got / sizeof *got);
                size_t want_n
                    = oneshot (nfft, hop, 0, total, want, sizeof want / 4);
                /* room = push_max_out(n) */
                dp_spectrogram_state_t *s
                    = dp_spectrogram_create (nfft, hop, 0, 7.5f, 0);
                DP_REQUIRE (s != NULL);
                size_t made
                    = dp_spectrogram_push (s, x, pre, got, sizeof got / 4);
                size_t rows = dp_spectrogram_rows_for (s, n);
                size_t cap  = dp_spectrogram_push_max_out (s, n);
                if (cap != rows * nfft)
                  exact = 0;
                size_t w
                    = dp_spectrogram_push (s, x + pre, n, got + made, cap);
                if (w != cap || dp_spectrogram_consumed (s) != n)
                  bounded = 0;
                made += w;
                if (made != want_n || memcmp (got, want, made * 4) != 0)
                  bounded = 0;
                if (!rows_are_the_psd (p, nfft, hop, total, got, made))
                  truth = 0;
                dp_spectrogram_destroy (s);

                /* room = push_max_out(n) - nfft: short by exactly one row */
                if (cap < nfft)
                  continue;
                n_one_less++;
                fill_nan (got, sizeof got / sizeof *got);
                s = dp_spectrogram_create (nfft, hop, 0, 7.5f, 0);
                DP_REQUIRE (s != NULL);
                made = dp_spectrogram_push (s, x, pre, got, sizeof got / 4);
                w    = dp_spectrogram_push (s, x + pre, n, got + made,
                                            cap - nfft);
                size_t took = dp_spectrogram_consumed (s);
                if (w != cap - nfft || took >= n)
                  one_less = 0;
                /* and took is maximal: replayed to the same carry, the samples
                   taken complete exactly the rows written, and one more would
                   complete the row there was no room for */
                {
                  static float            scratch[128 * 16];
                  dp_spectrogram_state_t *rp
                      = dp_spectrogram_create (nfft, hop, 0, 7.5f, 0);
                  DP_REQUIRE (rp != NULL);
                  dp_spectrogram_push (rp, x, pre, scratch,
                                       sizeof scratch / 4);
                  if (dp_spectrogram_rows_for (rp, took) != rows - 1
                      || dp_spectrogram_rows_for (rp, took + 1) != rows)
                    one_less = 0;
                  dp_spectrogram_destroy (rp);
                }
                made += w;
                made
                    += dp_spectrogram_push (s, x + pre + took, n - took,
                                            got + made, sizeof got / 4 - made);
                if (dp_spectrogram_consumed (s) != n - took || made != want_n
                    || memcmp (got, want, made * 4) != 0)
                  one_less = 0;
                if (!rows_are_the_psd (p, nfft, hop, total, got, made))
                  truth = 0;
                dp_spectrogram_destroy (s);
              }
        dp_psd_destroy (p);
      }
    DP_CHECK (exact);
    DP_CHECK (bounded);
    DP_CHECK (one_less);
    DP_CHECK (n_one_less > 0); /* the short-room half really ran */
    DP_CHECK (truth);

    /* an n near SIZE_MAX saturates rather than wraps */
    dp_spectrogram_state_t *s = dp_spectrogram_create (8, 2, 0, 0.0f, 0);
    DP_REQUIRE (s != NULL);
    DP_CHECK (dp_spectrogram_rows_for (s, SIZE_MAX) == (SIZE_MAX - 8) / 2 + 1);
    DP_CHECK (dp_spectrogram_push_max_out (s, SIZE_MAX) == SIZE_MAX);
    DP_CHECK (dp_spectrogram_rows_for (s, 7) == 0);
    DP_CHECK (dp_spectrogram_rows_for (s, 8) == 1);
    DP_CHECK (dp_spectrogram_push_max_out (s, 10) == 2 * 8);
    dp_spectrogram_destroy (s);
  }

  /* ---- 5. chunk invariance (G1) --------------------------------------- */
  {
    static ci_cfg_t cfg[] = { { 32, 32, 0 }, { 32, 7, 1 }, { 16, 1, 3 } };
    for (size_t k = 0; k < sizeof cfg / sizeof *cfg; k++)
      {
        dp_ci_spec_t spec = {
          .name     = "spectrogram",
          .create   = ci_create,
          .destroy  = ci_destroy,
          .process  = ci_process,
          .arg      = &cfg[k],
          .in_size  = sizeof (float _Complex),
          .out_size = sizeof (float),
          .out_cap  = ((NX - cfg[k].nfft) / cfg[k].hop + 1) * cfg[k].nfft,
          .frame_n  = cfg[k].nfft,
        };
        DP_CHECK (dp_chunk_invariance (&spec, x, NX) == 0);
      }
  }

  /* ---- 6. backpressure: a 1-row out loses nothing (G7) ---------------- */
  {
    const size_t nfft = 32, hop = 12;
    const size_t rows = (NX - nfft) / hop + 1;
    float       *want = (float *)malloc (rows * nfft * sizeof *want);
    float       *got  = (float *)malloc (rows * nfft * sizeof *got);
    DP_REQUIRE (want != NULL && got != NULL);
    DP_REQUIRE (oneshot (nfft, hop, 1, NX, want, rows * nfft) == rows * nfft);

    /* arbitrary chunks, each re-offered from consumed() until it is in */
    dp_spectrogram_state_t *s = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
    dp_spectrogram_state_t *clone
        = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
    DP_REQUIRE (s != NULL && clone != NULL);
    void *pre_blob = malloc (dp_spectrogram_state_bytes (s));
    DP_REQUIRE (pre_blob != NULL);
    uint32_t r   = 0xBAC4u;
    size_t   off = 0, made = 0, short_calls = 0;
    int      maximal = 1, progressed = 1;
    while (off < NX)
      {
        size_t chunk = 1 + dp_xs32 (&r) % (3 * nfft);
        if (chunk > NX - off)
          chunk = NX - off;
        size_t done = 0;
        while (done < chunk)
          {
            /* room for ONE row, and a few floats over: whole rows only */
            float row[32 + 5];
            /* the state before this push, kept in a clone, to judge what
               the push took against */
            dp_spectrogram_get_state (s, pre_blob);
            /* a clone that cannot be made (the object was left undrained)
               fails the verdict, not the whole run: nothing after it
               depends on the clone */
            if (dp_spectrogram_set_state (clone, pre_blob) != DP_OK)
              maximal = 0;
            size_t w = dp_spectrogram_push (s, x + off + done, chunk - done,
                                            row, nfft + 5);
            DP_CHECK (w == 0 || w == nfft);
            DP_CHECK (dp_spectrogram_pending (s) < nfft);
            DP_CHECK (dp_spectrogram_rows_for (s, 0) == 0); /* held < nfft */
            if (w && made < rows)
              memcpy (got + made * nfft, row, nfft * sizeof *row);
            made += w / nfft;
            size_t took = dp_spectrogram_consumed (s);
            /* maximal: what it took completes exactly the rows it wrote, and
               -- unless it took everything -- one more sample would complete
               a second row, which the 1-row out has no room for */
            if (dp_spectrogram_rows_for (clone, took) != w / nfft
                || (took < chunk - done
                    && dp_spectrogram_rows_for (clone, took + 1) != 2))
              maximal = 0;
            short_calls += took < chunk - done;
            done += took;
            if (!took && !w)
              {
                /* no progress: a failed check, not an abort, so the
                   sections after this one still run */
                progressed = 0;
                break;
              }
          }
        off += chunk;
      }
    DP_CHECK (progressed); /* every call made progress */
    DP_CHECK (made == rows);
    DP_CHECK (memcmp (got, want, rows * nfft * sizeof *got) == 0);
    /* the precondition: the backpressure path really ran */
    DP_CHECK (short_calls > 0);
    DP_CHECK (maximal);
    free (pre_blob);
    dp_spectrogram_destroy (clone);

    /* a max_out that is several rows and a remainder uses the whole rows,
       writes nothing past them, and takes the most it can */
    {
      dp_spectrogram_state_t *t
          = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
      DP_REQUIRE (t != NULL);
      float three[3 * 32 + 5];
      fill_nan (three, sizeof three / sizeof *three);
      DP_CHECK (dp_spectrogram_push (t, x, NX, three, 3 * nfft + 5)
                == 3 * nfft);
      int tail_untouched = 1;
      for (size_t i = 3 * nfft; i < 3 * nfft + 5; i++)
        tail_untouched &= isnan (three[i]) != 0;
      DP_CHECK (tail_untouched);
      size_t took = dp_spectrogram_consumed (t);
      dp_spectrogram_destroy (t);
      t = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
      DP_REQUIRE (t != NULL);
      DP_CHECK (dp_spectrogram_rows_for (t, took) == 3);
      DP_CHECK (dp_spectrogram_rows_for (t, took + 1) == 4);
      dp_spectrogram_destroy (t);
    }

    /* too small for one row: nothing written, and what is taken is exactly
       the input that completes no row -- all of it, or up to the sample that
       would complete the next one. From a fresh stream and from a carry. */
    for (size_t pre = 0; pre < 3 * nfft; pre += 5)
      for (size_t n = 0; n < 2 * nfft; n += 3)
        {
          dp_spectrogram_state_t *t
              = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
          float big[8 * 32], tiny[31];
          DP_REQUIRE (t != NULL);
          dp_spectrogram_push (t, x, pre, big, sizeof big / sizeof *big);
          fill_nan (tiny, sizeof tiny / sizeof *tiny);
          DP_CHECK (dp_spectrogram_push (t, x + pre, n, tiny, nfft - 1) == 0);
          int untouched = 1; /* "writes nothing" means not one float */
          for (size_t i = 0; i < sizeof tiny / sizeof *tiny; i++)
            untouched &= isnan (tiny[i]) != 0;
          DP_CHECK (untouched);
          size_t took = dp_spectrogram_consumed (t);
          DP_CHECK (took <= n);
          dp_spectrogram_destroy (t);
          /* replay to the same point: the samples it took complete no row,
             and -- unless it took them all -- one more would have */
          t = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
          DP_REQUIRE (t != NULL);
          dp_spectrogram_push (t, x, pre, big, sizeof big / sizeof *big);
          DP_CHECK (dp_spectrogram_rows_for (t, took) == 0);
          if (took < n)
            DP_CHECK (dp_spectrogram_rows_for (t, took + 1) == 1);
          dp_spectrogram_destroy (t);
        }
    /* with out NULL and max_out 0, input that completes no row is taken */
    {
      dp_spectrogram_state_t *t
          = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
      DP_REQUIRE (t != NULL);
      DP_CHECK (dp_spectrogram_push (t, x, nfft - 1, NULL, 0) == 0);
      DP_CHECK (dp_spectrogram_consumed (t) == nfft - 1);
      DP_CHECK (dp_spectrogram_push (t, x, 1, NULL, 0) == 0); /* row due */
      DP_CHECK (dp_spectrogram_consumed (t) == 0);
      dp_spectrogram_destroy (t);
    }
    free (want);
    free (got);
    dp_spectrogram_destroy (s);
  }

  /* ---- 7. flush: on the hop grid, iff a sample is uncovered (G2) ------ */
  {
    const size_t nfft = 16, hop = 6;
    int          grid = 1, iff = 1, restart = 1;
    for (size_t n = 0; n < 4 * nfft; n++)
      {
        dp_spectrogram_state_t *s
            = dp_spectrogram_create (nfft, hop, 0, 0.0f, 0);
        float buf[64 * 16], row[16], want[64 * 16];
        DP_REQUIRE (s != NULL);
        size_t rows
            = dp_spectrogram_push (s, x, n, buf, sizeof buf / 4) / nfft;
        size_t covered = rows ? (rows - 1) * hop + nfft : 0;
        size_t pend    = dp_spectrogram_pending (s);
        size_t flushed = dp_spectrogram_flush (s, row);
        if ((flushed == nfft) != (n > covered))
          iff = 0;
        /* pending is what no written row covered, counted here from the
           rows the push returned, not read back from the framer that
           flush also reads */
        if (pend != n - covered)
          iff = 0;
        if (flushed == nfft)
          {
            /* row `rows`, on the hop grid: PSD's dBFS of the input
               zero-padded past its end, sliced at rows * hop -- a truth
               that shares no framer with the object under test */
            float _Complex padded[64 + 16] = { 0 };
            memcpy (padded, x, n * sizeof *x);
            dp_psd_state_t *gp = ref_psd (nfft, 0, 0.0f);
            DP_REQUIRE (gp != NULL);
            dp_psd_frame_db (gp, padded + rows * hop, want);
            if (memcmp (row, want, sizeof row) != 0)
              grid = 0;
            dp_psd_destroy (gp);
          }
        /* restarted: a second flush owes nothing, pending is 0, and the
           next row covers samples [0, nfft) of the NEW stream */
        if (dp_spectrogram_flush (s, row) != 0 || dp_spectrogram_pending (s)
            || dp_spectrogram_consumed (s) != 0)
          restart = 0;
        float           first[16], ref[16];
        dp_psd_state_t *p = ref_psd (nfft, 0, 0.0f);
        DP_REQUIRE (p != NULL);
        dp_psd_frame_db (p, x + 3, ref);
        if (dp_spectrogram_push (s, x + 3, nfft, first, nfft) != nfft
            || memcmp (first, ref, sizeof ref) != 0)
          restart = 0;
        dp_psd_destroy (p);
        dp_spectrogram_destroy (s);
      }
    DP_CHECK (grid);
    DP_CHECK (iff);
    DP_CHECK (restart);
  }

  /* ---- 8. reset and pending ------------------------------------------- */
  {
    /* Kaiser at beta 7.5, so "reset keeps the configuration" is visible:
       the row after a reset must still be that window's */
    const size_t            nfft = 16, hop = 4;
    dp_spectrogram_state_t *s = dp_spectrogram_create (nfft, hop, 1, 7.5f, 0);
    float                   buf[16 * 16];
    DP_REQUIRE (s != NULL);
    DP_CHECK (dp_spectrogram_pending (s) == 0);
    DP_CHECK (dp_spectrogram_consumed (s) == 0);
    dp_spectrogram_push (s, x, 10, buf, sizeof buf / 4);
    DP_CHECK (dp_spectrogram_pending (s) == 10); /* no row yet */
    dp_spectrogram_push (s, x + 10, 10, buf, sizeof buf / 4);
    /* 20 samples: rows at 0 and 4 cover [0, 20); nothing is pending */
    DP_CHECK (dp_spectrogram_pending (s) == 0);
    dp_spectrogram_push (s, x + 20, 3, buf, sizeof buf / 4);
    DP_CHECK (dp_spectrogram_pending (s) == 3);
    DP_CHECK (dp_spectrogram_consumed (s) == 3);

    /* reset: the next row is [0, nfft) of the next push, whatever came
       before it */
    dp_spectrogram_reset (s);
    DP_CHECK (dp_spectrogram_pending (s) == 0);
    DP_CHECK (dp_spectrogram_consumed (s) == 0);
    float           row[16], ref[16];
    dp_psd_state_t *p = ref_psd (nfft, 1, 7.5f);
    DP_REQUIRE (p != NULL);
    DP_CHECK (dp_spectrogram_push (s, x + 100, nfft, row, nfft) == nfft);
    dp_psd_frame_db (p, x + 100, ref);
    DP_CHECK (memcmp (row, ref, sizeof row) == 0);
    dp_psd_destroy (p);
    dp_spectrogram_destroy (s);
  }

  /* ---- 9. rows are DC-centred, as PSD's kernel emits them ------------ */
  /* bin k at index nfft/2 + k: a tone above the carrier lands right of the
     centre, one below it left of it, and DC at the centre itself */
  {
    const size_t     nfft   = 64;
    static const int bins[] = { 5, -5, 0, 31, -32 };
    for (size_t b = 0; b < sizeof bins / sizeof *bins; b++)
      {
        float _Complex tone[64];
        for (size_t i = 0; i < nfft; i++)
          tone[i] = cexp (I * 2.0 * M_PI * (double)bins[b] * (double)i
                          / (double)nfft);
        float                   row[64];
        dp_spectrogram_state_t *a
            = dp_spectrogram_create (nfft, nfft, 3, 0, 0);
        DP_REQUIRE (a != NULL);
        DP_REQUIRE (dp_spectrogram_push (a, tone, nfft, row, nfft) == nfft);
        size_t want = (size_t)((long)nfft / 2 + bins[b]);
        size_t peak = 0;
        for (size_t i = 1; i < nfft; i++)
          if (row[i] > row[peak])
            peak = i;
        DP_CHECK (peak == want);
        DP_CHECK_NEAR (row[want], 0.0, 1e-4);
        dp_spectrogram_destroy (a);
      }
  }

  /* ---- 10. the state blob (G4) ---------------------------------------- */
  {
    const size_t nfft = 32, hop = 10;
    const size_t rows = (NX - nfft) / hop + 1;
    float       *want = (float *)malloc (rows * nfft * sizeof *want);
    float       *got  = (float *)malloc (rows * nfft * sizeof *got);
    DP_REQUIRE (want != NULL && got != NULL);
    DP_REQUIRE (oneshot (nfft, hop, 2, NX, want, rows * nfft) == rows * nfft);

    /* a mid-frame split at a spread of offsets across three frames (the
       stride widens, so not every one): serialize, destroy, resume in a
       FRESH object, and the rows are the one-shot's */
    int resumed = 1, fixed_size = 1;
    for (size_t cut = 0; cut < 3 * nfft; cut += 1 + cut / 4)
      {
        size_t split = 200 + cut;
        fill_nan (got, rows * nfft);
        dp_spectrogram_state_t *a
            = dp_spectrogram_create (nfft, hop, 2, 0.0f, 0);
        DP_REQUIRE (a != NULL);
        size_t made  = dp_spectrogram_push (a, x, split, got, rows * nfft);
        size_t bytes = dp_spectrogram_state_bytes (a);
        void  *blob  = malloc (bytes);
        DP_REQUIRE (blob != NULL);
        dp_spectrogram_get_state (a, blob);
        dp_spectrogram_destroy (a);

        dp_spectrogram_state_t *b
            = dp_spectrogram_create (nfft, hop, 2, 0.0f, 0);
        DP_REQUIRE (b != NULL);
        if (dp_spectrogram_state_bytes (b) != bytes)
          fixed_size = 0; /* a function of the shape, not of the fill */
        /* b is not pristine: a stray carry and a nonzero consumed, so the
           restore has to REPLACE the carry and reset the count, not merely
           find them already right */
        float scratch[32];
        dp_spectrogram_push (b, x + 700, 3, scratch, nfft);
        DP_REQUIRE (dp_spectrogram_consumed (b) == 3);
        DP_REQUIRE (dp_spectrogram_set_state (b, blob) == DP_OK);
        DP_CHECK (dp_spectrogram_consumed (b) == 0);
        made += dp_spectrogram_push (b, x + split, NX - split, got + made,
                                     rows * nfft - made);
        if (made != rows * nfft
            || memcmp (got, want, rows * nfft * sizeof *got) != 0)
          resumed = 0;
        free (blob);
        dp_spectrogram_destroy (b);
      }
    DP_CHECK (resumed);
    DP_CHECK (fixed_size);

    /* the uniform round trip: determinism, fidelity, envelope reject */
    dp_spectrogram_state_t *a = dp_spectrogram_create (nfft, hop, 2, 0.0f, 0);
    dp_spectrogram_state_t *b = dp_spectrogram_create (nfft, hop, 2, 0.0f, 0);
    DP_REQUIRE (a != NULL && b != NULL);
    /* 217 samples: 19 rows, 27 held for the next, 5 of them pending */
    dp_spectrogram_push (a, x, 217, got, rows * nfft);
    DP_CHECK (dp_spectrogram_pending (a) != 0);
    DP_STATE_ROUNDTRIP_TEST (dp_spectrogram, a, b);

    /* a blob of another hop is refused, and the refusal changes nothing */
    dp_spectrogram_state_t *c
        = dp_spectrogram_create (nfft, hop + 1, 2, 0.0f, 0);
    DP_REQUIRE (c != NULL);
    void *blob = malloc (dp_spectrogram_state_bytes (a));
    DP_REQUIRE (blob != NULL);
    DP_REQUIRE (dp_spectrogram_state_bytes (c)
                == dp_spectrogram_state_bytes (a)); /* the size cannot tell */
    dp_spectrogram_get_state (a, blob);
    dp_spectrogram_push (c, x, 5, got, rows * nfft);
    DP_CHECK (dp_spectrogram_set_state (c, blob) == DP_ERR_INVALID);
    DP_CHECK (dp_spectrogram_pending (c) == 5);
    free (blob);
    dp_spectrogram_destroy (a);
    dp_spectrogram_destroy (b);
    dp_spectrogram_destroy (c);
    free (want);
    free (got);
  }

  /* ---- 11. the Python face's sizing, in C --------------------------- */
  /* A binding with no consumed() sizes each push's out by push_max_out and
     must lose nothing: a socket handing over chunks shorter than a frame,
     most of which complete no row and so get no room at all. */
  {
    static const size_t shp[][2] = { { 64, 16 }, { 64, 64 }, { 32, 5 } };
    for (size_t k = 0; k < sizeof shp / sizeof *shp; k++)
      {
        const size_t nfft = shp[k][0], hop = shp[k][1];
        const size_t rows = (NX - nfft) / hop + 1;
        float       *want = (float *)malloc (rows * nfft * sizeof *want);
        float       *got  = (float *)malloc (rows * nfft * sizeof *got);
        DP_REQUIRE (want != NULL && got != NULL);
        DP_REQUIRE (oneshot (nfft, hop, 0, NX, want, rows * nfft)
                    == rows * nfft);
        dp_spectrogram_state_t *s
            = dp_spectrogram_create (nfft, hop, 0, 7.5f, 0);
        DP_REQUIRE (s != NULL);
        uint32_t r   = 0x50C3u + (uint32_t)k;
        size_t   off = 0, made = 0, zero_room = 0;
        int      whole = 1;
        while (off < NX)
          {
            size_t chunk = 1 + dp_xs32 (&r) % (nfft - 1);
            if (chunk > NX - off)
              chunk = NX - off;
            size_t cap = dp_spectrogram_push_max_out (s, chunk);
            zero_room += cap == 0;
            if (made + cap > rows * nfft)
              {
                whole = 0; /* the bound would overrun the one-shot */
                break;
              }
            size_t w
                = dp_spectrogram_push (s, x + off, chunk, got + made, cap);
            if (w != cap || dp_spectrogram_consumed (s) != chunk)
              whole = 0;
            made += w;
            off += chunk;
          }
        DP_CHECK (whole);
        DP_CHECK (made == rows * nfft);
        DP_CHECK (memcmp (got, want, rows * nfft * sizeof *got) == 0);
        DP_CHECK (zero_room > 0); /* the no-room path really ran */
        free (want);
        free (got);
        dp_spectrogram_destroy (s);
      }
  }

  /* ---- 12. beta is ignored outside the Kaiser window ---------------- */
  {
    const size_t nfft = 64;
    for (int w = 0; w < 4; w++)
      {
        dp_spectrogram_state_t *a
            = dp_spectrogram_create (nfft, nfft, w, 0.0f, 0);
        dp_spectrogram_state_t *b
            = dp_spectrogram_create (nfft, nfft, w, 7.5f, 0);
        float ra[64], rb[64];
        DP_REQUIRE (a != NULL && b != NULL);
        DP_REQUIRE (dp_spectrogram_push (a, x, nfft, ra, nfft) == nfft);
        DP_REQUIRE (dp_spectrogram_push (b, x, nfft, rb, nfft) == nfft);
        const int same = memcmp (ra, rb, sizeof ra) == 0;
        /* Kaiser (1) is the precondition: there beta MUST change the row,
           or "the same" for the other three would prove nothing */
        DP_CHECK (w == 1 ? !same : same);
        dp_spectrogram_destroy (a);
        dp_spectrogram_destroy (b);
      }
  }

  /* ---- 13. an all-zero row reads exactly the kernel's floor --------- */
  /* PSD clamps power at 1e-20 before the log, so a frame of zeros reads
     -200 dB in every bin, exactly, whatever the window. The clamp makes ANY
     frame below about -200 dBFS read the same, so -200 does not tell "no
     signal" from a signal under the floor; design U5 leaves that open */
  {
    const size_t nfft       = 32;
    float _Complex zero[32] = { 0 };
    float row[32];
    for (int w = 0; w < 4; w++)
      {
        dp_spectrogram_state_t *s
            = dp_spectrogram_create (nfft, nfft, w, 7.5f, 0);
        DP_REQUIRE (s != NULL);
        DP_REQUIRE (dp_spectrogram_push (s, zero, nfft, row, nfft) == nfft);
        int at_floor = 1;
        for (size_t i = 0; i < nfft; i++)
          at_floor &= row[i] == -200.0f;
        DP_CHECK (at_floor);
        dp_spectrogram_destroy (s);
      }
  }

  /* ---- 14. the blob: size from nfft alone; every refusal changes nothing */
  {
    const size_t nfft = 32, hop = 10;
    /* the size is a function of nfft alone: every window, beta and hop */
    static const size_t hops[]  = { 1, 10, 32 };
    static const float  betas[] = { 0.0f, 7.5f };
    size_t              want    = 0;
    int                 one     = 1;
    for (int w = 0; w < 4; w++)
      for (size_t h = 0; h < sizeof hops / sizeof *hops; h++)
        for (size_t bt = 0; bt < sizeof betas / sizeof *betas; bt++)
          {
            dp_spectrogram_state_t *t
                = dp_spectrogram_create (nfft, hops[h], w, betas[bt], 0);
            DP_REQUIRE (t != NULL);
            size_t bytes = dp_spectrogram_state_bytes (t);
            if (!want)
              want = bytes;
            one &= bytes == want;
            dp_spectrogram_destroy (t);
          }
    DP_CHECK (one);

    /* a real carry: 50 samples make 2 rows and hold 30 */
    float                   rows[4 * 32];
    dp_spectrogram_state_t *a = dp_spectrogram_create (nfft, hop, 0, 0.0f, 0);
    DP_REQUIRE (a != NULL);
    DP_REQUIRE (dp_spectrogram_push (a, x, 50, rows, 4 * nfft) == 2 * nfft);
    const size_t   bytes = dp_spectrogram_state_bytes (a);
    unsigned char *blob  = (unsigned char *)malloc (bytes);
    unsigned char *lie   = (unsigned char *)calloc (1, 2 * bytes);
    DP_REQUIRE (blob != NULL && lie != NULL);
    dp_spectrogram_get_state (a, blob);

    /* the envelope is the spectrogram's own: 'SPGM', version 1 */
    dp_state_hdr_t hdr;
    memcpy (&hdr, blob, sizeof hdr);
    DP_CHECK (SPECTROGRAM_STATE_MAGIC == DP_FOURCC ('S', 'P', 'G', 'M'));
    DP_CHECK (SPECTROGRAM_STATE_VERSION == 1u);
    DP_CHECK (hdr.magic == SPECTROGRAM_STATE_MAGIC);
    DP_CHECK (hdr.version == SPECTROGRAM_STATE_VERSION);
    DP_CHECK (hdr.bytes == bytes);

    /* the blob carries the stream position and nothing else: the same
       stream pushed in other pieces (so another consumed()) into a
       spectrogram of another window and beta serializes byte for byte the
       same */
    {
      dp_spectrogram_state_t *other
          = dp_spectrogram_create (nfft, hop, 2, 3.0f, 0);
      DP_REQUIRE (other != NULL);
      dp_spectrogram_push (other, x, 43, rows, 4 * nfft);
      dp_spectrogram_push (other, x + 43, 7, rows, 4 * nfft);
      DP_REQUIRE (dp_spectrogram_consumed (other)
                  != dp_spectrogram_consumed (a));
      unsigned char *ob = (unsigned char *)malloc (bytes);
      DP_REQUIRE (ob != NULL);
      dp_spectrogram_get_state (other, ob);
      DP_CHECK (memcmp (ob, blob, bytes) == 0);
      free (ob);
      dp_spectrogram_destroy (other);
    }

    /* the target holds a stream of its own, and a TWIN holds the same one
       and is never offered a bad blob: after every refusal the target must
       still be the twin -- pending, consumed, and the rows its next push
       makes */
    dp_spectrogram_state_t *b = dp_spectrogram_create (nfft, hop, 0, 0.0f, 0);
    dp_spectrogram_state_t *twin
        = dp_spectrogram_create (nfft, hop, 0, 0.0f, 0);
    DP_REQUIRE (b != NULL && twin != NULL);
    dp_spectrogram_push (b, x + 300, 5, rows, 4 * nfft);
    dp_spectrogram_push (twin, x + 300, 5, rows, 4 * nfft);
    int unchanged = 1, refused = 1;
#define REFUSE(blob_)                                                         \
  do                                                                          \
    {                                                                         \
      refused &= dp_spectrogram_set_state (b, (blob_)) == DP_ERR_INVALID;     \
      unchanged                                                               \
          &= dp_spectrogram_pending (b) == dp_spectrogram_pending (twin)      \
             && dp_spectrogram_consumed (b)                                   \
                    == dp_spectrogram_consumed (twin);                        \
    }                                                                         \
  while (0)

    /* wrong magic */
    memcpy (lie, blob, bytes);
    lie[0] ^= 0xFF;
    REFUSE (lie);

    /* another version */
    memcpy (lie, blob, bytes);
    uint16_t version;
    memcpy (&version, lie + offsetof (dp_state_hdr_t, version),
            sizeof version);
    version++;
    memcpy (lie + offsetof (dp_state_hdr_t, version), &version,
            sizeof version);
    REFUSE (lie);

    /* an envelope claiming another size, with an intact child after it */
    memcpy (lie, blob, bytes);
    uint32_t claimed = (uint32_t)bytes + 8;
    memcpy (lie + offsetof (dp_state_hdr_t, bytes), &claimed, sizeof claimed);
    REFUSE (lie);

    /* another nfft: a blob of another size altogether */
    {
      dp_spectrogram_state_t *c = dp_spectrogram_create (16, 5, 0, 0.0f, 0);
      DP_REQUIRE (c != NULL);
      DP_REQUIRE (dp_spectrogram_state_bytes (c) < bytes);
      dp_spectrogram_push (c, x, 20, rows, 4 * nfft);
      memset (lie, 0, 2 * bytes);
      dp_spectrogram_get_state (c, lie); /* zero-padded to the target's */
      REFUSE (lie);
      dp_spectrogram_destroy (c);
    }

    /* another hop: the framer stores it, and the size cannot tell */
    {
      dp_spectrogram_state_t *c
          = dp_spectrogram_create (nfft, hop + 1, 0, 0.0f, 0);
      DP_REQUIRE (c != NULL);
      DP_REQUIRE (dp_spectrogram_state_bytes (c) == bytes);
      dp_spectrogram_push (c, x, 50, rows, 4 * nfft);
      dp_spectrogram_get_state (c, lie);
      REFUSE (lie);
      dp_spectrogram_destroy (c);
    }

    /* an impossible carry in the framer's child blob: 21 held with 2 rows
       out (a drained framer holds at least nfft - hop = 22), and counters
       that agree, written = 2 * hop + 21 */
    memcpy (lie, blob, bytes);
    unsigned char *child   = lie + sizeof (dp_state_hdr_t);
    const uint64_t live    = nfft - hop - 1;
    const uint64_t written = 2 * hop + live;
    memcpy (child + sizeof (dp_state_hdr_t), &live, sizeof live);
    memcpy (child + DP_FRAMER_STATE_WRITTEN_OFFSET (float, nfft), &written,
            sizeof written);
    REFUSE (lie);
#undef REFUSE
    DP_CHECK (refused);
    DP_CHECK (unchanged);

    /* still the twin in the rows it makes next */
    {
      float  rb[4 * 32], rt[4 * 32];
      size_t wb = dp_spectrogram_push (b, x + 305, 60, rb, 4 * nfft);
      size_t wt = dp_spectrogram_push (twin, x + 305, 60, rt, 4 * nfft);
      DP_CHECK (wb == wt && wb > 0 && memcmp (rb, rt, wb * sizeof *rb) == 0);
    }

    /* ...and the real blob restores: the refusals are the checks, not a
       set_state that refuses everything */
    DP_CHECK (dp_spectrogram_set_state (b, blob) == DP_OK);
    DP_CHECK (dp_spectrogram_pending (b) == dp_spectrogram_pending (a));
    free (blob);
    free (lie);
    dp_spectrogram_destroy (a);
    dp_spectrogram_destroy (b);
    dp_spectrogram_destroy (twin);
  }

  /* ---- 14b. a different window or beta is NOT refused on restore ------ */
  /* The blob carries no window or beta, so a Hann object's blob restores
     into a Kaiser object, and what follows is Kaiser's: the continuation
     equals a Kaiser object fed the whole stream, from the row the split
     left off at. The precondition: a Hann continuation differs, so the
     comparison could tell the two apart. */
  {
    const size_t            nfft = 16, hop = 6, split = 77, total = 400;
    dp_spectrogram_state_t *a = dp_spectrogram_create (nfft, hop, 0, 0.0f, 0);
    dp_spectrogram_state_t *b = dp_spectrogram_create (nfft, hop, 1, 6.0f, 0);
    dp_spectrogram_state_t *ref
        = dp_spectrogram_create (nfft, hop, 1, 6.0f, 0);
    DP_REQUIRE (a && b && ref);
    float *out_a = malloc (total * nfft * sizeof *out_a);
    float *out_b = malloc (total * nfft * sizeof *out_b);
    float *out_r = malloc (total * nfft * sizeof *out_r);
    void  *blob  = malloc (dp_spectrogram_state_bytes (a));
    DP_REQUIRE (out_a && out_b && out_r && blob);
    size_t ka = dp_spectrogram_push (a, x, split, out_a, total * nfft) / nfft;
    DP_CHECK (dp_spectrogram_consumed (a) == split);
    dp_spectrogram_get_state (a, blob);
    DP_CHECK (dp_spectrogram_set_state (b, blob) == DP_OK); /* not refused */
    size_t kb = dp_spectrogram_push (b, x + split, total - split, out_b,
                                     total * nfft)
                / nfft;
    size_t kr
        = dp_spectrogram_push (ref, x, total, out_r, total * nfft) / nfft;
    DP_CHECK (ka + kb == kr);
    DP_CHECK (kb > 0
              && memcmp (out_b, out_r + ka * nfft, kb * nfft * sizeof *out_b)
                     == 0);
    /* the precondition: the Hann object's own continuation is different */
    size_t kh = dp_spectrogram_push (a, x + split, total - split, out_a,
                                     total * nfft)
                / nfft;
    DP_CHECK (kh == kb
              && memcmp (out_a, out_b, kb * nfft * sizeof *out_a) != 0);
    free (blob);
    free (out_r);
    free (out_b);
    free (out_a);
    dp_spectrogram_destroy (ref);
    dp_spectrogram_destroy (b);
    dp_spectrogram_destroy (a);
  }
  /* ...and "or beta": a Kaiser beta 6 blob restores into a Kaiser beta 9
     object, whose continuation is beta 9's (a beta 9 object fed the whole
     stream); the precondition is that beta 6's own continuation differs. */
  {
    const size_t            nfft = 16, hop = 6, split = 77, total = 400;
    dp_spectrogram_state_t *a = dp_spectrogram_create (nfft, hop, 1, 6.0f, 0);
    dp_spectrogram_state_t *b = dp_spectrogram_create (nfft, hop, 1, 9.0f, 0);
    dp_spectrogram_state_t *ref
        = dp_spectrogram_create (nfft, hop, 1, 9.0f, 0);
    DP_REQUIRE (a && b && ref);
    float *out_a = malloc (total * nfft * sizeof *out_a);
    float *out_b = malloc (total * nfft * sizeof *out_b);
    float *out_r = malloc (total * nfft * sizeof *out_r);
    void  *blob  = malloc (dp_spectrogram_state_bytes (a));
    DP_REQUIRE (out_a && out_b && out_r && blob);
    size_t ka = dp_spectrogram_push (a, x, split, out_a, total * nfft) / nfft;
    dp_spectrogram_get_state (a, blob);
    DP_CHECK (dp_spectrogram_set_state (b, blob) == DP_OK); /* not refused */
    size_t kb = dp_spectrogram_push (b, x + split, total - split, out_b,
                                     total * nfft)
                / nfft;
    size_t kr
        = dp_spectrogram_push (ref, x, total, out_r, total * nfft) / nfft;
    DP_CHECK (ka + kb == kr);
    DP_CHECK (kb > 0
              && memcmp (out_b, out_r + ka * nfft, kb * nfft * sizeof *out_b)
                     == 0);
    size_t k6 = dp_spectrogram_push (a, x + split, total - split, out_a,
                                     total * nfft)
                / nfft;
    DP_CHECK (k6 == kb
              && memcmp (out_a, out_b, kb * nfft * sizeof *out_a) != 0);
    free (blob);
    free (out_r);
    free (out_b);
    free (out_a);
    dp_spectrogram_destroy (ref);
    dp_spectrogram_destroy (b);
    dp_spectrogram_destroy (a);
  }

  /* ---- 15. a row arrives with the push that delivers its last sample -- */
  /* zero latency in samples: fed one sample per push, the push carrying
     sample r*hop + nfft - 1 is the one that returns row r -- never a later
     push -- and what it returns IS row r */
  {
    static const size_t shp[][2] = { { 16, 16 }, { 16, 5 }, { 8, 1 } };
    for (size_t k = 0; k < sizeof shp / sizeof *shp; k++)
      {
        const size_t            nfft = shp[k][0], hop = shp[k][1];
        dp_spectrogram_state_t *s
            = dp_spectrogram_create (nfft, hop, 0, 0.0f, 0);
        dp_psd_state_t *p = ref_psd (nfft, 0, 0.0f);
        DP_REQUIRE (s != NULL && p != NULL);
        float  row[16], ref[16];
        size_t r       = 0;
        int    on_time = 1;
        for (size_t i = 0; i < 300; i++)
          {
            size_t    w   = dp_spectrogram_push (s, x + i, 1, row, nfft);
            const int due = i + 1 >= nfft && (i + 1 - nfft) % hop == 0;
            if ((w == nfft) != due)
              on_time = 0;
            if (w == nfft)
              {
                dp_psd_frame_db (p, x + r * hop, ref);
                if (memcmp (row, ref, nfft * sizeof *row) != 0)
                  on_time = 0;
                r++;
              }
          }
        DP_CHECK (on_time);
        DP_CHECK (r == (300 - nfft) / hop + 1); /* and rows did arrive */
        dp_psd_destroy (p);
        dp_spectrogram_destroy (s);
      }
  }

  DP_TEST_END ("test_spectrogram_core");
}
