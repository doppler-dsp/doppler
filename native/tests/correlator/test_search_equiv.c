/**
 * @file test_search_equiv.c
 * @brief The search flavors' scenes: how the stream arrives changes no
 *        byte (#2752 step 3.4b).
 *
 * These scenes were the step's oracle: in the transition commit they drove
 * acq and the correlator's search flavors side by side, compiled under
 * strict floating point, and every hit and every dwell's surface was
 * byte-identical (PR #2930's first CI run is that evidence). With acq/
 * deleted they stay as the search flavors' pins (docs/design/correlator.md
 * section 7), each against its own plain run: the same stream pushed whole,
 * on one thread, without a restore. The scenes:
 *
 *   - both doors, continuous and burst;
 *   - one peak and four, with a second emitter, and a close pair;
 *   - rate hypotheses;
 *   - the carrier coupled;
 *   - a tiled search with D > 1;
 *   - one thread and four;
 *   - odd chunks;
 *   - a mid-stream get_state / set_state into a fresh engine.
 *
 * Every hit, all 64 bytes of it, and every dwell's statistic surface (the
 * surface sink, every dwell) must match the plain run's. Built as the
 * library is, under its own flags: a delivery that changed a bit there
 * would be a bit a caller sees.
 */
#include "doppler/correlator/correlator_core.h"
#include "dp_preamble_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert (sizeof (dp_correlator_hit_t) == 64,
                "the one hit record is 64 bytes");

/* A length-31 maximal-length sequence (one period): PN(mls_poly(5),
 * seed=1), as test_correlator_search.c's CODE31. */
static const uint8_t CODE31[31]
    = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
        1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };

enum
{
  SF  = 31,
  SPC = 2,
  NX  = SF * SPC
};
static const double CHIP_RATE = 1.0e6;
static const double FS        = 2.0e6; /* CHIP_RATE * SPC */

typedef struct
{
  const char *name;
  int         burst;      /* the burst door; else continuous            */
  size_t      reps;       /* burst: repetitions                         */
  size_t      code_only;  /* continuous: code-only epochs               */
  double      cn0;        /* design C/N0, dB-Hz                         */
  double      du;         /* Doppler uncertainty, Hz                    */
  double      rate;       /* Doppler rate the depth is bounded against  */
  size_t      rate_hyps;  /* burst: rate hypotheses                     */
  double      carrier;    /* coupled carrier, Hz; 0 = uncoupled         */
  size_t      max_peaks;  /* the peak list                              */
  int         threads;    /* set_threads                                */
  size_t      pin_d;      /* configure_search_raw's depth; 0 = sized    */
  size_t      pin_nc;     /* ... and its looks                          */
  int         odd_chunks; /* push in prime-length chunks                */
  int         split;      /* get_state at the middle, into a fresh one  */
  double      f0;         /* the emitter's Doppler, Hz                  */
  double      amp;        /* its amplitude against unit noise           */
  double      amp2;       /* a second emitter at -f0; 0 = none (the peak
                             list's scenes)                            */
  size_t d2;              /* ... its lag behind the first, samples; 0 =
                             18                                        */
} scene_t;

/* Everything a run produced, as bytes. */
typedef struct
{
  unsigned char *b;
  size_t         n, cap;
} bytes_t;

static void
bytes_add (bytes_t *o, const void *p, size_t n)
{
  /* A push with no hit adds nothing, and memcpy is not defined on a NULL
     buffer even for zero bytes (UBSan, #2930's first CI run). */
  if (n == 0)
    return;
  if (o->n + n > o->cap)
    {
      o->cap           = 2 * (o->n + n) + 4096;
      unsigned char *b = realloc (o->b, o->cap);
      if (!b)
        abort ();
      o->b = b;
    }
  memcpy (o->b + o->n, p, n);
  o->n += n;
}

typedef struct
{
  bytes_t hits, surf;
  size_t  n_hits, n_dwells;
} rec_t;

static void
sink (void *ctx, const float *surface, size_t rows, size_t cols,
      uint64_t samples_consumed)
{
  rec_t   *r      = ctx;
  uint64_t hdr[3] = { rows, cols, samples_consumed };
  bytes_add (&r->surf, hdr, sizeof hdr);
  bytes_add (&r->surf, surface, rows * cols * sizeof *surface);
  r->n_dwells++;
}

/* The scene's stream: the code, held SPC samples a chip, delayed by 23
   samples and rotated by f0, plus, when amp2 > 0, a second emitter of the
   same code d2 samples behind it (18 unless set) at -f0, in unit complex
   noise.
   Deterministic. */
static float _Complex *
stream (const scene_t *sc, size_t n)
{
  float _Complex *pre = dp_code_preamble (CODE31, SF, SPC);
  float _Complex *x   = malloc (n * sizeof *x);
  if (!pre || !x)
    abort ();
  uint32_t     rng = 0x2752u;
  const double tau = 2.0 * acos (-1.0) * sc->f0 / FS;
  for (size_t k = 0; k < n; k++)
    {
      const double ph = tau * (double)k;
      x[k] = sc->amp * pre[(k + NX - 23) % NX]
                 * (float _Complex) (cos (ph) + I * sin (ph))
             + sc->amp2 * pre[(k + 2 * NX - 23 - (sc->d2 ? sc->d2 : 18)) % NX]
                   * (float _Complex) (cos (ph) - I * sin (ph))
             + dp_cgauss (&rng);
    }
  free (pre);
  return x;
}

/* One driver, two APIs. P is the function prefix, T the state type and H
   the hit type; everything else is the same text. */
#define DEFINE_RUN(NAME, P, T, H)                                             \
  static T *NAME##_make (const scene_t *sc)                                   \
  {                                                                           \
    T *e;                                                                     \
    if (sc->burst)                                                            \
      {                                                                       \
        float _Complex *pre = dp_code_preamble (CODE31, SF, SPC);             \
        e = P##create_burst (pre, NX, sc->reps, FS, sc->cn0, sc->du, 1e-3,    \
                             0.9, 0, sc->rate,                                \
                             sc->rate_hyps ? sc->rate_hyps : 1);              \
        free (pre);                                                           \
      }                                                                       \
    else                                                                      \
      e = P##create_continuous (CODE31, SF, FS, CHIP_RATE, 0.0, sc->cn0,      \
                                sc->du, 1e-3, 0.9, 0, sc->code_only,          \
                                sc->rate);                                    \
    if (!e)                                                                   \
      return NULL;                                                            \
    /* A setter that refuses leaves the scene unbuilt: NULL, which the        \
       run's DP_REQUIRE reports. */                                           \
    if ((sc->pin_d                                                            \
         && P##configure_search_raw (e, sc->pin_d, sc->pin_nc, NULL))         \
        || (sc->carrier > 0.0                                                 \
            && P##set_carrier_freq_hz (e, sc->carrier) != DP_OK)              \
        || P##set_max_peaks (e, sc->max_peaks, NULL) != DP_OK                 \
        || P##set_threads (e, sc->threads) != DP_OK)                          \
      {                                                                       \
        P##destroy (e);                                                       \
        return NULL;                                                          \
      }                                                                       \
    return e;                                                                 \
  }                                                                           \
                                                                              \
  static int NAME##_run (const scene_t *sc, const float _Complex *x,          \
                         size_t n, rec_t *r)                                  \
  {                                                                           \
    static const size_t PRIMES[] = { 97, 251, 13, 509, 61, 1031, 7 };         \
    T                  *e        = NAME##_make (sc);                          \
    DP_REQUIRE (e != NULL);                                                   \
    DP_REQUIRE (P##set_surface_sink (e, sink, r, 1) == DP_OK);                \
    H hits[DP_CORRELATOR_MAX_PEAKS];                                          \
    /* Off every period: 3 frames past a multiple of the NC looks (4) and     \
       of the block (D = 4), and 17 samples into a frame, so the blob         \
       carries a carry, a block in progress and a look count. */              \
    const size_t at  = n / 2 + 3 * NX + 17;                                   \
    size_t       off = 0, k = 0;                                              \
    int          split_done = !sc->split;                                     \
    while (off < n)                                                           \
      {                                                                       \
        if (!split_done && off >= at)                                         \
          {                                                                   \
            /* A fresh engine resumes from the blob; the sink is the          \
               caller's, so it is set again. */                               \
            void *blob = malloc (P##state_bytes (e));                         \
            DP_REQUIRE (blob != NULL);                                        \
            P##get_state (e, blob);                                           \
            T *f = NAME##_make (sc);                                          \
            DP_REQUIRE (f != NULL);                                           \
            DP_REQUIRE (P##set_state (f, blob) == DP_OK);                     \
            free (blob);                                                      \
            P##destroy (e);                                                   \
            e = f;                                                            \
            DP_REQUIRE (P##set_surface_sink (e, sink, r, 1) == DP_OK);        \
            split_done = 1;                                                   \
          }                                                                   \
        size_t len = sc->odd_chunks ? PRIMES[k++ % 7] : n - off;              \
        if (len > n - off)                                                    \
          len = n - off;                                                      \
        if (!split_done && off < at && off + len > at)                        \
          len = at - off;                                                     \
        /* A full list stops a push; the rest is offered again. */            \
        const size_t nh                                                       \
            = P##push (e, x + off, len, hits, DP_CORRELATOR_MAX_PEAKS);       \
        bytes_add (&r->hits, hits, nh * sizeof *hits);                        \
        r->n_hits += nh;                                                      \
        off += P##consumed (e);                                               \
      }                                                                       \
    P##destroy (e);                                                           \
    return 0;                                                                 \
  }

DEFINE_RUN (corr, dp_correlator_, dp_correlator_state_t, dp_correlator_hit_t)

static int
equiv (const scene_t *sc)
{
  const size_t    n = (size_t)400 * NX;
  float _Complex *x = stream (sc, n);
  /* The plain run: the stream whole, one thread, no restore. */
  scene_t plain    = *sc;
  plain.odd_chunks = 0;
  plain.split      = 0;
  plain.threads    = 1;
  rec_t a = { 0 }, c = { 0 };
  corr_run (&plain, x, n, &a);
  corr_run (sc, x, n, &c);
  free (x);
  printf ("  %-28s hits %4zu/%-4zu dwells %4zu/%-4zu\n", sc->name, a.n_hits,
          c.n_hits, a.n_dwells, c.n_dwells);
  /* The scene must say something: a run with no hit or no dwell would
     match its plain run vacuously. */
  DP_CHECK_MSG (a.n_hits > 0 && a.n_dwells > 0, sc->name);
  /* Where two runs part, so a red names the byte. */
  for (int w = 0; w < 2; w++)
    {
      const bytes_t *x = w ? &a.surf : &a.hits, *y = w ? &c.surf : &c.hits;
      for (size_t i = 0; i < x->n && i < y->n; i++)
        if (x->b[i] != y->b[i])
          {
            printf ("    first difference: %s byte %zu\n",
                    w ? "surface" : "hits", i);
            if (!w)
              {
                const dp_correlator_hit_t *ha
                    = (const void *)(x->b + i / 64 * 64);
                const dp_correlator_hit_t *hc
                    = (const void *)(y->b + i / 64 * 64);
                printf ("    plain stat %a peak %a row %zu col %zu\n"
                        "    scene stat %a peak %a row %zu col %zu\n",
                        ha->test_stat, ha->test_stat_peak, ha->row, ha->col,
                        hc->test_stat, hc->test_stat_peak, hc->row, hc->col);
              }
            break;
          }
    }
  DP_CHECK_MSG (a.n_hits == c.n_hits && a.hits.n == c.hits.n
                    && memcmp (a.hits.b, c.hits.b, a.hits.n) == 0,
                sc->name);
  DP_CHECK_MSG (a.n_dwells == c.n_dwells && a.surf.n == c.surf.n
                    && memcmp (a.surf.b, c.surf.b, a.surf.n) == 0,
                sc->name);
  free (a.hits.b);
  free (a.surf.b);
  free (c.hits.b);
  free (c.surf.b);
  return 0;
}

int
main (void)
{
  /* Continuous scenes are sized at a design C/N0 that the emitter (amp
     0.7 against unit noise, about 69 dB-Hz at 2 MHz) clears; the burst
     ones for the whole preamble. A Doppler of 3.1 kHz sits off the native
     grid. */
  const scene_t S[] = {
    { .name      = "continuous",
      .code_only = 1,
      .cn0       = 66.0,
      .du        = 10.0e3,
      .max_peaks = 1,
      .threads   = 1,
      .f0        = 3.1e3,
      .amp       = 0.7 },
    { .name      = "continuous, four peaks",
      .code_only = 1,
      .cn0       = 66.0,
      .du        = 10.0e3,
      .max_peaks = 4,
      .threads   = 1,
      .f0        = 3.1e3,
      .amp       = 0.7,
      .amp2      = 0.5 },
    { .name      = "continuous, carrier",
      .code_only = 1,
      .cn0       = 66.0,
      .du        = 10.0e3,
      .carrier   = 2.5e9,
      .max_peaks = 1,
      .threads   = 1,
      .f0        = 3.1e3,
      .amp       = 0.7 },
    { .name       = "continuous, odd chunks",
      .code_only  = 1,
      .cn0        = 66.0,
      .du         = 10.0e3,
      .max_peaks  = 4,
      .threads    = 1,
      .odd_chunks = 1,
      .f0         = 3.1e3,
      .amp        = 0.7,
      .amp2       = 0.5 },
    /* Two near-equal emitters three samples apart: the second pick's
       neighbour lies in the first's exclusion zone and exceeds it, so the
       peak-on-lag estimate meets its upper clamp (19 times here). That is
       the only way it can: the bound binds iff the neighbour beats the
       centre. */
    { .name      = "continuous, close pair",
      .code_only = 1,
      .cn0       = 66.0,
      .du        = 10.0e3,
      .max_peaks = 4,
      .threads   = 1,
      .f0        = 3.1e3,
      .amp       = 0.7,
      .amp2      = 0.69,
      .d2        = 3 },
    { .name       = "continuous, split",
      .code_only  = 1,
      .cn0        = 66.0,
      .du         = 10.0e3,
      .max_peaks  = 4,
      .threads    = 1,
      .odd_chunks = 1,
      .split      = 1,
      .f0         = 3.1e3,
      .amp        = 0.7,
      .amp2       = 0.5 },
    /* Tiled: an uncertainty past the native span (chip_rate / 2 sf, 16.1
       kHz) tiles the window (5 tiles), and eight code-only epochs size the
       depth to D = 4. Sized, never pinned: configure_search_raw exits
       wideband. */
    { .name      = "tiled, D=4",
      .code_only = 8,
      .cn0       = 66.0,
      .du        = 60.0e3,
      .rate      = 500.0,
      .max_peaks = 4,
      .threads   = 1,
      .f0        = 23.0e3,
      .amp       = 0.7,
      .amp2      = 0.5 },
    { .name      = "tiled, D=4, four threads",
      .code_only = 8,
      .cn0       = 66.0,
      .rate      = 500.0,
      .du        = 60.0e3,
      .max_peaks = 4,
      .threads   = 4,
      .f0        = 23.0e3,
      .amp       = 0.7,
      .amp2      = 0.5 },
    { .name       = "tiled, D=4, carrier, split",
      .code_only  = 8,
      .cn0        = 66.0,
      .rate       = 500.0,
      .du         = 60.0e3,
      .carrier    = 2.5e9,
      .max_peaks  = 4,
      .threads    = 4,
      .odd_chunks = 1,
      .split      = 1,
      .f0         = 23.0e3,
      .amp        = 0.7,
      .amp2       = 0.5 },
    { .name      = "non-coherent",
      .code_only = 1,
      .cn0       = 66.0,
      .du        = 10.0e3,
      .max_peaks = 1,
      .threads   = 1,
      .pin_d     = 1,
      .pin_nc    = 4,
      .f0        = 3.1e3,
      .amp       = 0.5 },
    { .name       = "non-coherent, split",
      .code_only  = 1,
      .cn0        = 66.0,
      .du         = 10.0e3,
      .max_peaks  = 1,
      .threads    = 1,
      .pin_d      = 1,
      .pin_nc     = 4,
      .odd_chunks = 1,
      .split      = 1,
      .f0         = 3.1e3,
      .amp        = 0.5 },
    { .name      = "burst",
      .burst     = 1,
      .reps      = 8,
      .cn0       = DP_CORRELATOR_CN0_NONE,
      .du        = 10.0e3,
      .max_peaks = 1,
      .threads   = 1,
      .f0        = 3.1e3,
      .amp       = 0.7 },
    { .name       = "burst, four peaks, threads",
      .burst      = 1,
      .reps       = 8,
      .cn0        = DP_CORRELATOR_CN0_NONE,
      .du         = 10.0e3,
      .max_peaks  = 4,
      .threads    = 4,
      .odd_chunks = 1,
      .f0         = 3.1e3,
      .amp        = 0.7,
      .amp2       = 0.5 },
    { .name      = "burst, rate hypotheses",
      .burst     = 1,
      .reps      = 8,
      .cn0       = DP_CORRELATOR_CN0_NONE,
      .du        = 10.0e3,
      .rate      = 2.0e3,
      .rate_hyps = 5,
      .max_peaks = 1,
      .threads   = 1,
      .f0        = 3.1e3,
      .amp       = 0.7 },
    { .name       = "burst, rates, split",
      .burst      = 1,
      .reps       = 8,
      .cn0        = DP_CORRELATOR_CN0_NONE,
      .du         = 10.0e3,
      .rate       = 2.0e3,
      .rate_hyps  = 5,
      .max_peaks  = 4,
      .threads    = 4,
      .odd_chunks = 1,
      .split      = 1,
      .f0         = 3.1e3,
      .amp        = 0.7,
      .amp2       = 0.5 },
  };
  for (size_t i = 0; i < sizeof S / sizeof *S; i++)
    equiv (&S[i]);
  DP_TEST_END ("test_search_equiv");
}
