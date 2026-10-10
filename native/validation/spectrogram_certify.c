/**
 * @file spectrogram_certify.c
 * @brief The measurements the Spectrogram's characterization and
 * certification are built from.
 *
 * The Spectrogram has no Python binding until #1894's slice 3b, so its
 * evidence follows `docs/dev/contributing/validation.md` "Certifying a
 * component with no binding": **this file measures, and
 * `src/doppler/tests/validation/spectrogram/validate.py` renders and
 * asserts.** Nothing here decides whether a number is acceptable.
 *
 * Run with no arguments for a readable run (`make validate-c`), or with
 * `--emit` for the CSV blocks the validator parses.
 *
 * ## The certification -- the header's claims at scale
 *
 * Every row is held to an oracle built without the object: row k of a
 * stream is dp_psd_frame_db() of samples [k*hop, k*hop + nfft), and the
 * flushed row is the same of that slice zero-padded. PSD's kernel is the
 * one part the two share, and it is certified on its own.
 *
 * - `rows`: 71 partitions per shape (fixed chunks and seeded random
 *   splits), every row against the oracle, every push sized by
 *   push_max_out against the stream's arithmetic, the carry bound, pending
 *   and zero latency after every push.
 * - `backpressure`: an output of a few rows plus 13 floats, re-offered
 *   until taken: nothing lost, whole rows only, the tail untouched, and a
 *   push that stops short only when its room is full.
 * - `sizing`: rows_for, push_max_out and a push from every carry state.
 * - `flush`: every length 0..4*nfft+3, the grid row iff owed, once, then a
 *   restart.
 * - `state`: every cut point, two fills, a restore into a new object that
 *   holds a stray carry, the refusals, and another window's continuation.
 * - `level`: a full-scale tone on every bin, its peak index and level.
 *
 * ## U5 -- the dB floor (docs/design/spectrogram-measurements.md §5.4)
 *
 * PSD's kernel clamps power at 1e-20 before the log, so a row cannot read
 * below -200 dB. Three measurements, every window, nfft 1024:
 *
 * - `floor_tone`: a single on-bin tone of amplitude 10^(L/20), so of power
 *   L dBFS, across L from -120 to -250. Its bin reads L while L is above the
 *   floor, and the clamp below it.
 * - `floor_zero`: an all-zero frame, the digital silence a caller might want
 *   to tell apart.
 * - `floor_noise`: complex Gaussian noise of total power L dBFS (E|z|^2 = 1
 *   from dp_rng_test.h, scaled), NOISE_FRAMES frames per window and level.
 *   A bin's power is exponential with mean mu = P * ENBW / n against PSD's
 *   tone reference, so its median sits at L + 10 log10(ENBW / n)
 *   + 10 log10(ln 2), and a bin reads the floor with probability
 *   1 - exp(-1e-20 / mu). Both expectations are computed from the window's
 *   own ENBW and printed beside the frames' mean median and mean count.
 *
 * Samples this small are ordinary floats (float32's smallest normal is about
 * 1.2e-38, an amplitude of -759 dBFS), and in-tree sources make them: a wfm
 * source's `level` is in dBFS with no lower bound (wfm_compose.h, `level`;
 * applied as a gain of 10^(level/20) in wfm_compose.c).
 */
#include "doppler/spectrogram/spectrogram_core.h"

#include "dp_rng_test.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFFT 1024
#define TONE 37 /* the tone's bin: on the grid, away from DC */
/* noise frames averaged per window and level: one frame's median scatters
   by about 0.2 dB, more than the gap between two windows' ENBW */
#define NOISE_FRAMES 256

static const char *const WINDOW[4]
    = { "hann", "kaiser", "blackman-harris", "rect" };

/** @brief One row of the first NFFT samples of @p x, under @p window. */
static int
row_of (int window, const float _Complex *x, float *row, double *enbw)
{
  /* Kaiser at beta 8, a display's usual choice; beta is ignored otherwise */
  dp_spectrogram_state_t *s = dp_spectrogram_create (
      NFFT, NFFT, window, window == 1 ? 8.0f : 0.0f, DP_SPECTROGRAM_DB);
  if (!s)
    return -1;
  size_t w = dp_spectrogram_push (s, x, NFFT, row, NFFT);
  if (enbw)
    *enbw = s->psd->enbw;
  dp_spectrogram_destroy (s);
  return w == NFFT ? 0 : -1;
}

static size_t
at_floor (const float *row)
{
  size_t k = 0;
  for (size_t i = 0; i < NFFT; i++)
    k += row[i] == -200.0f;
  return k;
}

static int
cmp_float (const void *a, const void *b)
{
  const float x = *(const float *)a, y = *(const float *)b;
  return (x > y) - (x < y);
}

static int
floor_tone (int emit)
{
  static const double level[]
      = { -120, -150, -180, -190, -195, -199, -200, -201, -205, -210, -250 };
  const size_t nl = sizeof level / sizeof level[0];
  float _Complex x[NFFT];
  float row[NFFT];
  printf (emit ? "# floor_tone\nwindow,level_dbfs,tone_bin_db,"
                 "tone_bin_minus_level_db,bins_at_floor\n"
               : "\nfloor_tone: an on-bin tone of power L dBFS, its bin\n");
  for (int w = 0; w < 4; w++)
    for (size_t j = 0; j < nl; j++)
      {
        const double a = pow (10.0, level[j] / 20.0);
        for (size_t i = 0; i < NFFT; i++)
          x[i] = (float)(a * cos (2.0 * M_PI * TONE * (double)i / NFFT))
                 + (float)(a * sin (2.0 * M_PI * TONE * (double)i / NFFT)) * I;
        if (row_of (w, x, row, NULL))
          return 1;
        const float b = row[NFFT / 2 + TONE];
        printf (emit ? "%s,%.0f,%.4f,%.4f,%zu\n"
                     : "  %-16s L %6.0f  bin %10.4f  (%+.4f)  at floor %zu\n",
                WINDOW[w], level[j], b, b - level[j], at_floor (row));
      }
  return 0;
}

static int
floor_zero (int emit)
{
  float _Complex x[NFFT] = { 0 };
  float row[NFFT];
  printf (emit ? "# floor_zero\nwindow,bins_at_floor,bins,min_db,max_db\n"
               : "\nfloor_zero: an all-zero frame\n");
  for (int w = 0; w < 4; w++)
    {
      if (row_of (w, x, row, NULL))
        return 1;
      float lo = row[0], hi = row[0];
      for (size_t i = 1; i < NFFT; i++)
        {
          lo = row[i] < lo ? row[i] : lo;
          hi = row[i] > hi ? row[i] : hi;
        }
      printf (emit ? "%s,%zu,%d,%.4f,%.4f\n"
                   : "  %-16s at floor %zu of %d  min %.4f  max %.4f\n",
              WINDOW[w], at_floor (row), NFFT, lo, hi);
    }
  return 0;
}

static int
floor_noise (int emit)
{
  static const double level[] = { -150, -160, -170, -175, -180, -190, -210 };
  const size_t        nl      = sizeof level / sizeof level[0];
  float _Complex x[NFFT];
  float row[NFFT], sorted[NFFT];
  printf (emit ? "# floor_noise\nwindow,level_dbfs,frames,expected_median_db,"
                 "mean_median_db,expected_at_floor,mean_at_floor,"
                 "max_at_floor\n"
               : "\nfloor_noise: complex noise of total power L dBFS, the "
                 "mean over frames\n");
  for (int w = 0; w < 4; w++)
    for (size_t j = 0; j < nl; j++)
      {
        uint32_t     seed = 1894u + (uint32_t)(100 * w + j);
        const double g    = pow (10.0, level[j] / 20.0);
        double       enbw = 0.0, med_sum = 0.0, floor_sum = 0.0;
        size_t       floor_max = 0;
        for (int f = 0; f < NOISE_FRAMES; f++)
          {
            for (size_t i = 0; i < NFFT; i++)
              x[i] = (float _Complex) (g * dp_cgauss (&seed));
            if (row_of (w, x, row, &enbw))
              return 1;
            memcpy (sorted, row, sizeof row);
            qsort (sorted, NFFT, sizeof *sorted, cmp_float);
            med_sum += 0.5 * (sorted[NFFT / 2 - 1] + sorted[NFFT / 2]);
            const size_t k = at_floor (row);
            floor_sum += (double)k;
            floor_max = k > floor_max ? k : floor_max;
          }
        /* each bin's power is exponential with mean mu against the tone
           reference, so P(bin <= floor) = 1 - exp(-floor / mu) */
        const double mu         = pow (10.0, level[j] / 10.0) * enbw / NFFT;
        const double want_med   = 10.0 * log10 (mu) + 10.0 * log10 (log (2.0));
        const double want_floor = NFFT * -expm1 (-1e-20 / mu);
        printf (emit ? "%s,%.0f,%d,%.4f,%.4f,%.4f,%.4f,%zu\n"
                     : "  %-16s L %6.0f  frames %d  median: expected %9.4f "
                       "mean %9.4f  at floor: expected %8.3f mean %8.3f "
                       "max %zu\n",
                WINDOW[w], level[j], NOISE_FRAMES, want_med,
                med_sum / NOISE_FRAMES, want_floor, floor_sum / NOISE_FRAMES,
                floor_max);
      }
  return 0;
}

/* ══ The certification: the header's claims at scale ═══════════════════════
 *
 * Every row below is held to an oracle that shares no code with the object
 * beyond PSD's kernel, which is certified on its own: row k of a stream is
 * dp_psd_frame_db() of X[k*hop .. k*hop + nfft), and the flushed row is the
 * same of that slice zero-padded. A pass is a count of zero. */

typedef float _Complex cf;

#define MAXLEN 131101 /* the longest stream any block feeds */
#define NRAND 64      /* seeded random partitions per shape */

static cf X[MAXLEN];

/** @brief The test stream: seeded complex Gaussian, distinct everywhere. */
static void
stream_init (void)
{
  uint32_t seed = 0x5C3A7u;
  for (size_t i = 0; i < MAXLEN; i++)
    X[i] = dp_cgauss (&seed);
}

static float
beta_of (int window)
{
  return window == 1 ? 8.0f : 0.0f;
}

static dp_spectrogram_state_t *
make (size_t nfft, size_t hop, int window)
{
  return dp_spectrogram_create (nfft, hop, window, beta_of (window),
                                DP_SPECTROGRAM_DB);
}

/** @brief The header's composition, built separately: the oracle's PSD. */
static dp_psd_state_t *
ref_psd (size_t nfft, int window)
{
  return dp_psd_create (nfft, 1.0, window, beta_of (window), 1, 1.0, 0, 0,
                        0.0);
}

/** @brief Rows `len` samples make: (len - nfft) / hop + 1, or 0. */
static size_t
nrows (size_t len, size_t nfft, size_t hop)
{
  return len >= nfft ? (len - nfft) / hop + 1 : 0;
}

/** @brief Rows 0..R-1 of X[0..len), each PSD's dBFS of its own slice. */
static float *
oracle (size_t nfft, size_t hop, int window, size_t len, size_t *rows)
{
  const size_t    r_n  = nrows (len, nfft, hop);
  float          *want = malloc ((r_n + 1) * nfft * sizeof *want);
  dp_psd_state_t *p    = ref_psd (nfft, window);
  if (!want || !p)
    {
      free (want);
      if (p)
        dp_psd_destroy (p);
      return NULL;
    }
  for (size_t r = 0; r < r_n; r++)
    dp_psd_frame_db (p, X + r * hop, want + r * nfft);
  dp_psd_destroy (p);
  *rows = r_n;
  return want;
}

/** @brief Rows of `got` that differ from `want`, over the first `n`. */
static size_t
rows_differing (const float *got, const float *want, size_t n, size_t nfft)
{
  size_t bad = 0;
  for (size_t r = 0; r < n; r++)
    bad += memcmp (got + r * nfft, want + r * nfft, nfft * sizeof *got) != 0;
  return bad;
}

/** @brief Block heading: `# name` and its CSV header, or a readable title. */
static void
block (int emit, const char *name, const char *title, const char *header)
{
  if (emit)
    printf ("# %s\n%s\n", name, header);
  else
    printf ("\n%s: %s\n%s\n", name, title, header);
}

/* ── rows: the oracle, every partition, the carry, pending, latency ─────── */

typedef struct
{
  size_t pushes, rows, bad_rows, count_wrong, room_wrong, consumed_short;
  size_t carry_over, pending_wrong, late;
} part_t;

/**
 * One partition of X[0..len): `chunk` samples per push, or for chunk 0 a
 * seeded random split of 1..2*nfft+1. Every push is sized by push_max_out,
 * which must be exactly the rows the stream's arithmetic says the push
 * completes; the push must take it all; after it, fewer than nfft samples
 * are held (rows_for(s, 0) == 0) and pending is what no written row covers.
 * For chunk 1, a row must come back with the push that delivers its last
 * sample.
 */
static int
partition (size_t nfft, size_t hop, int window, size_t len, size_t chunk,
           uint32_t seed, const float *want, size_t r_n, float *got, part_t *t)
{
  dp_spectrogram_state_t *s = make (nfft, hop, window);
  if (!s)
    return -1;
  memset (t, 0, sizeof *t);
  size_t made = 0, off = 0;
  while (off < len)
    {
      size_t n = chunk ? chunk : 1 + dp_xs32 (&seed) % (2 * nfft + 1);
      if (n > len - off)
        n = len - off;
      size_t       room = dp_spectrogram_push_max_out (s, n);
      const size_t truth
          = (nrows (off + n, nfft, hop) - nrows (off, nfft, hop)) * nfft;
      t->room_wrong += room != truth;
      if (room > (r_n + 1) * nfft - made) /* never write past `got` */
        room = (r_n + 1) * nfft - made;
      const size_t w = dp_spectrogram_push (s, X + off, n, got + made, room);
      t->pushes++;
      t->consumed_short += dp_spectrogram_consumed (s) != n;
      if (chunk == 1)
        {
          const size_t due
              = off + 1 >= nfft && (off + 1 - nfft) % hop == 0 ? nfft : 0;
          t->late += w != due;
        }
      made += w;
      off += n;
      t->carry_over += dp_spectrogram_rows_for (s, 0) != 0;
      const size_t r = made / nfft, covered = r ? (r - 1) * hop + nfft : 0;
      t->pending_wrong
          += covered > off || dp_spectrogram_pending (s) != off - covered;
    }
  t->rows        = made / nfft;
  t->count_wrong = made != r_n * nfft;
  t->bad_rows
      = rows_differing (got, want, t->rows < r_n ? t->rows : r_n, nfft);
  dp_spectrogram_destroy (s);
  return 0;
}

static int
sweep_rows (int emit)
{
  /* nfft, hop, window, stream length */
  static const size_t shape[][4] = {
    { 2, 1, 3, 4099 },         { 8, 8, 0, 10007 },
    { 8, 3, 1, 10007 },        { 8, 1, 2, 6007 },
    { 64, 16, 3, 30011 },      { 256, 64, 0, 65537 },
    { 1024, 256, 1, MAXLEN },  { 1024, 1024, 2, MAXLEN },
    { 4096, 1365, 3, MAXLEN },
  };
  block (emit, "rows",
         "every row against PSD's dBFS of its slice, under fixed and "
         "random partitions",
         "nfft,hop,window,length,partitions,rows,pushes,bad_partitions,"
         "bad_rows,count_wrong,room_wrong,consumed_short,carry_over,"
         "pending_wrong,late_rows");
  for (size_t i = 0; i < sizeof shape / sizeof *shape; i++)
    {
      const size_t nfft = shape[i][0], hop = shape[i][1], len = shape[i][3];
      const int    w = (int)shape[i][2];
      size_t       r_n;
      float       *want = oracle (nfft, hop, w, len, &r_n);
      float       *got  = malloc ((r_n + 1) * nfft * sizeof *got);
      if (!want || !got)
        return 1;
      const size_t fixed[]
          = { 1, 7, nfft - 1, nfft, nfft + 1, 3 * nfft + 5, len };
      const size_t nfixed    = sizeof fixed / sizeof *fixed;
      part_t       sum       = { 0 }, t;
      size_t       bad_parts = 0, late = 0;
      for (size_t k = 0; k < nfixed + NRAND; k++)
        {
          const size_t chunk = k < nfixed ? fixed[k] : 0;
          if (partition (nfft, hop, w, len, chunk, 1894u + (uint32_t)k, want,
                         r_n, got, &t))
            return 1;
          bad_parts += t.bad_rows || t.count_wrong || t.room_wrong
                       || t.consumed_short || t.carry_over || t.pending_wrong
                       || t.late;
          sum.pushes += t.pushes;
          sum.bad_rows += t.bad_rows;
          sum.count_wrong += t.count_wrong;
          sum.room_wrong += t.room_wrong;
          sum.consumed_short += t.consumed_short;
          sum.carry_over += t.carry_over;
          sum.pending_wrong += t.pending_wrong;
          late += t.late;
        }
      printf ("%zu,%zu,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n",
              nfft, hop, WINDOW[w], len, nfixed + NRAND, r_n, sum.pushes,
              bad_parts, sum.bad_rows, sum.count_wrong, sum.room_wrong,
              sum.consumed_short, sum.carry_over, sum.pending_wrong, late);
      free (want);
      free (got);
    }
  return 0;
}

/* ── backpressure: a short out slows the stream and loses nothing ───────── */

static int
sweep_backpressure (int emit)
{
  const size_t        nfft = 256, hop = 64, len = 100003;
  static const size_t chunk[] = { 1, 37, 700, 5000, 100003 };
  static const size_t rooms[] = { 1, 2, 7 };
  size_t              r_n;
  float              *want = oracle (nfft, hop, 0, len, &r_n);
  float              *got  = malloc ((r_n + 8) * nfft * sizeof *got);
  float              *buf  = malloc ((7 * nfft + 13) * sizeof *buf);
  if (!want || !got || !buf)
    return 1;
  block (emit, "backpressure",
         "nfft 256, hop 64, Hann: an out of `room_rows` rows plus 13 floats, "
         "re-offered until taken",
         "chunk,room_rows,room_floats,offered,taken,stalls,rows,bad_rows,"
         "count_wrong,over_room,partial_writes,tail_touched,not_maximal,"
         "carry_over,no_progress");
  for (size_t c = 0; c < sizeof chunk / sizeof *chunk; c++)
    for (size_t q = 0; q < sizeof rooms / sizeof *rooms; q++)
      {
        dp_spectrogram_state_t *s = make (nfft, hop, 0);
        if (!s)
          return 1;
        const size_t full = rooms[q] * nfft, cap = full + 13;
        size_t       offered = 0, taken = 0, stalls = 0, made = 0, off = 0;
        size_t       over = 0, partial = 0, touched = 0, not_max = 0;
        size_t       carry = 0, stuck = 0;
        while (off < len && !stuck)
          {
            const size_t n    = chunk[c] < len - off ? chunk[c] : len - off;
            size_t       done = 0;
            while (done < n)
              {
                for (size_t i = 0; i < cap; i++)
                  buf[i] = NAN;
                const size_t w    = dp_spectrogram_push (s, X + off + done,
                                                         n - done, buf, cap);
                const size_t took = dp_spectrogram_consumed (s);
                offered += n - done;
                taken += took;
                over += w > full;
                partial += w % nfft != 0;
                for (size_t i = w < cap ? w : cap; i < cap; i++)
                  if (!isnan (buf[i]))
                    {
                      touched++;
                      break;
                    }
                /* stopping short is right only when the room is full AND
                   the next sample would complete a row */
                if (took < n - done)
                  {
                    stalls++;
                    not_max
                        += w != full || dp_spectrogram_rows_for (s, 1) == 0;
                  }
                carry += dp_spectrogram_rows_for (s, 0) != 0;
                const size_t keep = w < full ? w : full;
                if (made + keep <= (r_n + 8) * nfft)
                  memcpy (got + made, buf, keep * sizeof *buf);
                made += keep;
                if (!took && !w)
                  {
                    stuck = 1; /* no progress: a defect, not a stall */
                    break;
                  }
                done += took;
              }
            off += n;
          }
        const size_t rows = made / nfft;
        printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d,%zu,%zu,%zu,%zu,%zu,%zu\n",
                chunk[c], rooms[q], cap, offered, taken, stalls, rows,
                rows_differing (got, want, rows < r_n ? rows : r_n, nfft),
                made != r_n * nfft, over, partial, touched, not_max, carry,
                stuck);
        dp_spectrogram_destroy (s);
      }
  free (want);
  free (got);
  free (buf);
  return 0;
}

/* ── sizing: rows_for and push_max_out from every carry the shape has ───── */

static int
sweep_sizing (int emit)
{
  static const size_t shape[][2]
      = { { 8, 3 }, { 64, 16 }, { 256, 256 }, { 1024, 256 } };
  block (emit, "sizing",
         "from every carry state (p samples pushed, 0 <= p < nfft + hop), "
         "rows_for / push_max_out / a push against the stream's arithmetic",
         "nfft,hop,positions,trials,restore_refused,rows_for_wrong,"
         "max_out_wrong,push_wrong,one_less_wrong,saturates");
  for (size_t i = 0; i < sizeof shape / sizeof *shape; i++)
    {
      const size_t nfft = shape[i][0], hop = shape[i][1];
      const size_t ns[] = { 0,        1,    hop - 1,  hop,         hop + 1,
                            nfft - 1, nfft, nfft + 1, 3 * nfft + 5 };
      const size_t cap  = ((4 * nfft + 5) / hop + 2) * nfft;
      dp_spectrogram_state_t *base = make (nfft, hop, 3);
      dp_spectrogram_state_t *t    = make (nfft, hop, 3);
      float                  *out  = malloc (cap * sizeof *out);
      unsigned char          *blob
          = base ? malloc (dp_spectrogram_state_bytes (base)) : NULL;
      if (!base || !t || !out || !blob)
        return 1;
      size_t trials = 0, refused = 0, rf = 0, mo = 0, pw = 0, ol = 0;
      for (size_t p = 0; p < nfft + hop; p++)
        {
          dp_spectrogram_get_state (base, blob);
          for (size_t j = 0; j < sizeof ns / sizeof *ns; j++)
            {
              const size_t n = ns[j];
              const size_t r = nrows (p + n, nfft, hop) - nrows (p, nfft, hop);
              trials++;
              if (dp_spectrogram_set_state (t, blob) != DP_OK)
                {
                  refused++;
                  continue;
                }
              rf += dp_spectrogram_rows_for (t, n) != r;
              mo += dp_spectrogram_push_max_out (t, n) != r * nfft;
              pw += dp_spectrogram_push (t, X + p, n, out, r * nfft)
                        != r * nfft
                    || dp_spectrogram_consumed (t) != n;
              if (r > 0) /* one row less: fewer taken, one row fewer */
                {
                  refused += dp_spectrogram_set_state (t, blob) != DP_OK;
                  ol += dp_spectrogram_push (t, X + p, n, out, (r - 1) * nfft)
                            != (r - 1) * nfft
                        || dp_spectrogram_consumed (t) >= n;
                }
            }
          dp_spectrogram_push (base, X + p, 1, out,
                               dp_spectrogram_push_max_out (base, 1));
        }
      /* saturation: from a fresh state, rows for SIZE_MAX samples times
         nfft overflows, and must read SIZE_MAX, never a wrapped count */
      dp_spectrogram_reset (t);
      const size_t big      = dp_spectrogram_rows_for (t, SIZE_MAX);
      const size_t sat_want = big > SIZE_MAX / nfft ? SIZE_MAX : big * nfft;
      const int sat = big == nrows (SIZE_MAX, nfft, hop)
                      && dp_spectrogram_push_max_out (t, SIZE_MAX) == sat_want;
      printf ("%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d\n", nfft, hop,
              nfft + hop, trials, refused, rf, mo, pw, ol, sat);
      free (blob);
      free (out);
      dp_spectrogram_destroy (t);
      dp_spectrogram_destroy (base);
    }
  return 0;
}

/* ── flush: on the hop grid, iff owed, once, then a restart ─────────────── */

static int
sweep_flush (int emit)
{
  static const size_t shape[][3]
      = { { 2, 1, 3 }, { 8, 8, 0 },   { 8, 3, 1 },
          { 8, 1, 2 }, { 64, 16, 3 }, { 256, 64, 0 } };
  block (emit, "flush",
         "every stream length 0..4*nfft+3: push, flush, flush again, restart",
         "nfft,hop,window,lengths,emitted,wrong_pending,wrong_decision,"
         "off_grid,second_nonzero,after_flush_wrong,bad_restart");
  for (size_t i = 0; i < sizeof shape / sizeof *shape; i++)
    {
      const size_t nfft = shape[i][0], hop = shape[i][1];
      const int    w    = (int)shape[i][2];
      const size_t lens = 4 * nfft + 4;
      float *out = malloc ((nrows (lens, nfft, hop) + 1) * nfft * sizeof *out);
      float *row = malloc (nfft * sizeof *row);
      float *want       = malloc (nfft * sizeof *want);
      cf    *frame      = malloc (nfft * sizeof *frame);
      dp_psd_state_t *p = ref_psd (nfft, w);
      if (!out || !row || !want || !frame || !p)
        return 1;
      size_t emitted = 0, wp = 0, wd = 0, og = 0, sn = 0, af = 0, br = 0;
      for (size_t len = 0; len < lens; len++)
        {
          dp_spectrogram_state_t *s = make (nfft, hop, w);
          if (!s)
            return 1;
          const size_t made = dp_spectrogram_push (
              s, X, len, out, dp_spectrogram_push_max_out (s, len));
          const size_t r = made / nfft, covered = r ? (r - 1) * hop + nfft : 0;
          const size_t owed = len > covered ? len - covered : 0;
          wp += dp_spectrogram_pending (s) != owed;
          for (size_t j = 0; j < nfft; j++)
            row[j] = NAN;
          const size_t wf = dp_spectrogram_flush (s, row);
          wd += (wf != 0 && wf != nfft) || (wf == nfft) != (owed > 0);
          if (wf == nfft)
            {
              emitted++;
              /* the next row start, k*hop, zero-padded past the end */
              const size_t start = r * hop;
              for (size_t j = 0; j < nfft; j++)
                frame[j] = start + j < len ? X[start + j] : (cf)0.0f;
              dp_psd_frame_db (p, frame, want);
              og += memcmp (row, want, nfft * sizeof *row) != 0;
            }
          sn += dp_spectrogram_flush (s, row) != 0;
          af += dp_spectrogram_consumed (s) != 0
                || dp_spectrogram_pending (s) != 0;
          /* restarted at sample 0: the next nfft samples are row 0 */
          dp_psd_frame_db (p, X, want);
          br += dp_spectrogram_push (s, X, nfft, row, nfft) != nfft
                || memcmp (row, want, nfft * sizeof *row) != 0;
          dp_spectrogram_destroy (s);
        }
      printf ("%zu,%zu,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", nfft, hop,
              WINDOW[w], lens, emitted, wp, wd, og, sn, af, br);
      dp_psd_destroy (p);
      free (frame);
      free (want);
      free (row);
      free (out);
    }
  return 0;
}

/* ── state: every cut point, a fresh object, and what is refused ────────── */

/** @brief Push X[from..to) in pieces of 5, rows to `out`; floats written. */
static size_t
push_in_fives (dp_spectrogram_state_t *s, size_t from, size_t to, float *out)
{
  size_t made = 0;
  for (size_t off = from; off < to; off += 5)
    {
      const size_t n = to - off < 5 ? to - off : 5;
      made += dp_spectrogram_push (s, X + off, n, out + made,
                                   dp_spectrogram_push_max_out (s, n));
    }
  return made;
}

static int
sweep_state (int emit)
{
  /* nfft, hop, window, cut stride */
  static const size_t shape[][4] = {
    { 8, 3, 0, 1 }, { 64, 16, 1, 1 }, { 256, 64, 2, 1 }, { 1024, 256, 3, 13 }
  };
  block (emit, "state",
         "a cut at every point 0..3*nfft+3 (every 13th at nfft 1024): "
         "serialize, restore into a new object holding a stray carry, "
         "finish in pieces of 5",
         "nfft,hop,window,cuts,bytes,distinct_sizes,size_varies,unwritten,"
         "restore_refused,after_restore_wrong,resume_bad,corrupt_tried,"
         "corrupt_refused,target_changed,hop_tried,hop_refused,nfft_tried,"
         "nfft_refused,window_refused,window_wrong");
  for (size_t i = 0; i < sizeof shape / sizeof *shape; i++)
    {
      const size_t            nfft = shape[i][0], hop = shape[i][1];
      const size_t            stride = shape[i][3], len = 4 * nfft + hop + 37;
      const int               w = (int)shape[i][2], w2 = (w + 1) % 4;
      const size_t            hop2 = hop < nfft ? hop + 1 : hop - 1;
      size_t                  r_n, r_d;
      float                  *want   = oracle (nfft, hop, w, len, &r_n);
      float                  *wd     = oracle (nfft, hop, w2, len, &r_d);
      float                  *got    = malloc ((r_n + 2) * nfft * sizeof *got);
      dp_spectrogram_state_t *target = make (nfft, hop, w);
      dp_spectrogram_state_t *twin   = make (nfft, hop, w);
      if (!want || !wd || !got || !target || !twin)
        return 1;
      const size_t   bytes = dp_spectrogram_state_bytes (target);
      unsigned char *b1    = malloc (bytes);
      unsigned char *b2    = malloc (bytes);
      unsigned char *lie   = malloc (bytes);
      if (!b1 || !b2 || !lie)
        return 1;

      /* the size is a function of nfft alone */
      size_t                  varies = 0;
      dp_spectrogram_state_t *o[3]
          = { make (nfft, 1, 0), make (nfft, nfft, 3), make (nfft, hop2, 1) };
      for (int k = 0; k < 3; k++)
        {
          if (!o[k])
            return 1;
          varies += dp_spectrogram_state_bytes (o[k]) != bytes;
          dp_spectrogram_destroy (o[k]);
        }

      /* the refusal target holds a stream of its own; its twin is never
         offered a bad blob */
      float scratch[8];
      dp_spectrogram_push (target, X + 7, 5, scratch, 0);
      dp_spectrogram_push (twin, X + 7, 5, scratch, 0);

      size_t cuts = 0, sizes = 0, unwritten = 0, refused = 0, after = 0;
      size_t resume = 0, ctried = 0, crefused = 0, changed = 0;
      size_t htried = 0, hrefused = 0, ntried = 0, nrefused = 0;
      size_t wrefused = 0, wwrong = 0;
      for (size_t cut = 0; cut <= 3 * nfft + 3; cut += stride)
        {
          cuts++;
          dp_spectrogram_state_t *b = make (nfft, hop, w);
          dp_spectrogram_state_t *c = make (nfft, hop, w);
          dp_spectrogram_state_t *v = make (nfft, hop, w2);
          if (!b || !c || !v)
            return 1;
          const size_t rb
              = dp_spectrogram_push (b, X, cut, got,
                                     dp_spectrogram_push_max_out (b, cut))
                / nfft;
          sizes += dp_spectrogram_state_bytes (b) != bytes;
          memset (b1, 0xAA, bytes);
          memset (b2, 0x55, bytes);
          dp_spectrogram_get_state (b, b1);
          dp_spectrogram_get_state (b, b2);
          unwritten += memcmp (b1, b2, bytes) != 0;

          /* resume in a new object, bit for bit. It is not pristine: a
             stray 3-sample carry and a consumed of 3, so the restore has
             to REPLACE the carry and reset the count, not find them
             already right */
          dp_spectrogram_push (c, X + 700, 3, scratch, 0);
          dp_spectrogram_push (v, X + 700, 3, scratch, 0);
          if (dp_spectrogram_set_state (c, b1) != DP_OK)
            refused++;
          after += dp_spectrogram_consumed (c) != 0
                   || dp_spectrogram_pending (c) != dp_spectrogram_pending (b);
          const size_t rc
              = push_in_fives (c, cut, len, got + rb * nfft) / nfft;
          resume
              += rb + rc != r_n || rows_differing (got, want, r_n, nfft) != 0;

          /* another window is NOT refused; the rows that follow are its */
          if (dp_spectrogram_set_state (v, b1) != DP_OK)
            wrefused++;
          else
            {
              const size_t rv
                  = push_in_fives (v, cut, len, got + rb * nfft) / nfft;
              wwrong += rb + rv != r_d
                        || rows_differing (got + rb * nfft, wd + rb * nfft, rv,
                                           nfft)
                               != 0;
            }

          /* corrupt envelopes: magic, version, size */
          const size_t pend = dp_spectrogram_pending (target);
          const size_t cons = dp_spectrogram_consumed (target);
          for (int k = 0; k < 3; k++)
            {
              memcpy (lie, b1, bytes);
              dp_state_hdr_t h;
              memcpy (&h, lie, sizeof h);
              if (k == 0)
                h.magic ^= 0xFFu;
              else if (k == 1)
                h.version++;
              else
                h.bytes += 8;
              memcpy (lie, &h, sizeof h);
              ctried++;
              crefused
                  += dp_spectrogram_set_state (target, lie) == DP_ERR_INVALID;
              changed += dp_spectrogram_pending (target) != pend
                         || dp_spectrogram_consumed (target) != cons;
            }

          /* the same nfft at another hop: the size cannot tell */
          {
            dp_spectrogram_state_t *h = make (nfft, hop2, w);
            if (!h || dp_spectrogram_state_bytes (h) != bytes)
              return 1;
            dp_spectrogram_push (h, X, cut, got,
                                 dp_spectrogram_push_max_out (h, cut));
            dp_spectrogram_get_state (h, lie);
            htried++;
            hrefused
                += dp_spectrogram_set_state (target, lie) == DP_ERR_INVALID;
            changed += dp_spectrogram_pending (target) != pend
                       || dp_spectrogram_consumed (target) != cons;
            dp_spectrogram_destroy (h);
          }

          /* half the nfft: a shorter blob, zero-padded to this size */
          {
            const size_t            hn = nfft / 2;
            dp_spectrogram_state_t *h  = make (hn, hop < hn ? hop : hn, w);
            if (!h || dp_spectrogram_state_bytes (h) >= bytes)
              return 1;
            dp_spectrogram_push (h, X, cut, got,
                                 dp_spectrogram_push_max_out (h, cut));
            memset (lie, 0, bytes);
            dp_spectrogram_get_state (h, lie);
            ntried++;
            nrefused
                += dp_spectrogram_set_state (target, lie) == DP_ERR_INVALID;
            changed += dp_spectrogram_pending (target) != pend
                       || dp_spectrogram_consumed (target) != cons;
            dp_spectrogram_destroy (h);
          }
          dp_spectrogram_destroy (v);
          dp_spectrogram_destroy (c);
          dp_spectrogram_destroy (b);
        }

      /* after every refusal the target is still its twin, in the rows its
         next push makes too */
      {
        const size_t n  = 3 * nfft;
        float       *rt = malloc ((n / hop + 2) * nfft * sizeof *rt);
        float       *rw = malloc ((n / hop + 2) * nfft * sizeof *rw);
        if (!rt || !rw)
          return 1;
        const size_t wt = dp_spectrogram_push (
            target, X + 12, n, rt, dp_spectrogram_push_max_out (target, n));
        const size_t ww = dp_spectrogram_push (
            twin, X + 12, n, rw, dp_spectrogram_push_max_out (twin, n));
        changed
            += wt != ww || wt == 0 || memcmp (rt, rw, wt * sizeof *rt) != 0;
        free (rw);
        free (rt);
      }
      printf ("%zu,%zu,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,"
              "%zu,%zu,%zu,%zu\n",
              nfft, hop, WINDOW[w], cuts, bytes, sizes + 1, varies, unwritten,
              refused, after, resume, ctried, crefused, changed, htried,
              hrefused, ntried, nrefused, wrefused, wwrong);
      free (lie);
      free (b2);
      free (b1);
      dp_spectrogram_destroy (twin);
      dp_spectrogram_destroy (target);
      free (got);
      free (wd);
      free (want);
    }
  return 0;
}

/* ── level: a full-scale tone on every bin reads 0 dBFS, at nfft/2 + k ──── */

static int
sweep_level (int emit)
{
  static const size_t sizes[] = { 8, 64, 1024 };
  block (emit, "level",
         "a full-scale complex tone on each bin k: the row's peak index and "
         "its level",
         "window,nfft,bins,peak_wrong,max_abs_err_db");
  for (int w = 0; w < 4; w++)
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++)
      {
        const size_t            nfft = sizes[i];
        dp_spectrogram_state_t *s    = make (nfft, nfft, w);
        cf                     *x    = malloc (nfft * sizeof *x);
        float                  *row  = malloc (nfft * sizeof *row);
        if (!s || !x || !row)
          return 1;
        size_t wrong = 0;
        double err   = 0.0;
        for (long k = -(long)nfft / 2; k < (long)nfft / 2; k++)
          {
            for (size_t j = 0; j < nfft; j++)
              {
                const double ph
                    = 2.0 * M_PI * (double)k * (double)j / (double)nfft;
                x[j] = (float)cos (ph) + (float)sin (ph) * I;
              }
            if (dp_spectrogram_push (s, x, nfft, row, nfft) != nfft)
              return 1;
            size_t pk = 0;
            for (size_t j = 1; j < nfft; j++)
              pk = row[j] > row[pk] ? j : pk;
            wrong += pk != (size_t)((long)nfft / 2 + k);
            const double e = fabs ((double)row[pk]);
            err            = e > err ? e : err;
          }
        printf ("%s,%zu,%zu,%zu,%.3e\n", WINDOW[w], nfft, nfft, wrong, err);
        free (row);
        free (x);
        dp_spectrogram_destroy (s);
      }
  return 0;
}

int
main (int argc, char **argv)
{
  const int emit = argc > 1 && strcmp (argv[1], "--emit") == 0;
  if (!emit)
    printf ("spectrogram_certify: the certification, then U5's floor\n");
  stream_init ();
  /* the blocks in the order the validator renders them */
  static int (*const run[]) (int)
      = { sweep_rows,  sweep_backpressure, sweep_sizing,
          sweep_flush, sweep_state,        sweep_level,
          floor_tone,  floor_zero,         floor_noise };
  for (size_t i = 0; i < sizeof run / sizeof *run; i++)
    if (run[i](emit))
      {
        (void)fprintf (stderr,
                       "spectrogram_certify: a run could not be made\n");
        return 1;
      }
  return 0;
}
