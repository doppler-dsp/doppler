/**
 * @file acq_peak_list.c
 * @brief The peak list on the searcher's surface: two emitters resolved,
 *        the power spread at which the weaker disappears under the
 *        stronger's floor, and the false-alarm rate under the list.
 *
 * The continuous async-DSSS design puts every emitter on one (tile x code
 * phase) surface and asks the detector to report every peak above the gate
 * (docs/design/async-dsss-receiver.md §6, §7.1): the iterated maximum with
 * an exclusion zone of one tile by one chip around each pick, and the
 * two-epoch rule for a pick at an already-listed code phase (one emitter's
 * data transition splits it into twins at its own code phase on other
 * tiles; a twin moves with the transition, a real second emitter at that
 * code phase stays at its tile). §12 steps 2-4 ask what that buys and
 * costs, and this harness measures them on the engine as built:
 *
 *   separability   two equal emitters `df` tiles and `dtau` chips apart:
 *                  how often BOTH are listed, and where they are reported.
 *                  Inside one tile AND one chip they are one peak by
 *                  construction -- the zone is the resolution;
 *   the knee       a strong emitter fixed, a weak one stepped down: the
 *                  spread at which the weak one stops being a peak. Beside
 *                  it the same weak emitter ALONE, so a noise-limited miss
 *                  is told from a floor-limited one -- the knee is where
 *                  the two curves part, and it is the number §6.3 forks on;
 *   pfa            pure noise under `max_peaks` of 1, 4 and 8: the fraction
 *                  of dwells reporting anything, against the configured
 *                  pfa -- the list must not change it, since a second peak
 *                  is another draw from the same cells against the same
 *                  gate -- and, with one strong emitter present, the rate
 *                  of false peaks in its sidelobes.
 *
 * Method. The operating point of §12: a 1023-chip Gold code (CCSDS #365)
 * at 5 Mcps, two samples per chip, +/-50 kHz (21 tiles), asynchronous BPSK
 * data at 2700 sym/s from the shipped continuous-DSSS synth with its own
 * PRBS data, one synth per emitter, each at its own tile, code phase and
 * level; noise from the shipped awgn generator sized by
 * awgn_amplitude_for_snr() from the strong emitter's C/N0. The engine is
 * the continuous front door sized by its own physics at the design C/N0
 * (n_noncoh looks per dwell, whatever it chooses), fed one epoch at a time,
 * with the list at `max_peaks`. A hit matches an emitter when it is on the
 * emitter's tile (either neighbour when the emitter sits half a tile off)
 * and within one chip of its code phase, circularly; a hit at an emitter's
 * code phase on another tile is a twin. Nothing here builds a chip, a bit
 * or a sigma by hand.
 *
 * Usage:
 *   validate_acq_peak_list            the full tables
 *   validate_acq_peak_list --check    the spot checks CTest runs: two clean
 *                                     emitters are one hit at max_peaks 1
 *                                     and both at 4, at their coordinates;
 *                                     a data-split twin is held on the
 *                                     first dwell and listed on the second
 *                                     when it recurs at the same tile, and
 *                                     rarely under PRBS data; the per-dwell
 *                                     false-alarm rate under the list stays
 *                                     at the configured pfa
 */
#include "acq/acq_core.h"
#include "awgn/awgn_core.h"
#include "dp_test.h"
#include "gold/gold_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define NX (SF * SPC) /* code_bins = 2046 = one epoch                 */
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define DU 50000.0 /* +/-50 kHz: 21 tiles                            */
#define PFA 1e-3
#define PD 0.9
#define MAX_EMIT 2
#define MAX_HITS 64

typedef struct
{
  size_t tile;       /* window hypothesis                           */
  double frac;       /* fraction of a tile off its centre           */
  size_t tau;        /* code phase, samples: the synth's chip 0 sits
                        this many samples before the epoch start    */
  double   level_db; /* relative to unit power (0 = the strong one) */
  int      data;     /* WFM_DSSS_DATA_{PRBS,NONE,BITS}              */
  uint32_t seed;
} emitter_t;

typedef struct
{
  wfm_synth_state_t *syn;
  float              amp;
  size_t             row;  /* the tile the engine reports it on   */
  size_t             col;  /* the code phase it reports           */
  int                half; /* sits half a tile off: either neighbour */
} source_t;

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

static const uint8_t two_bits[2] = { 0, 1 };

/* An emitter as a source: the shipped synth at the tile's frequency plus
   the fraction, clean (noise is added once, for the scene), its code phase
   set by rendering `tau` samples ahead of the first epoch. */
static int
source_open (source_t *s, const uint8_t *code, const emitter_t *e, size_t W)
{
  const long   r      = dp_fftfreq_index (e->tile, W);
  const double f_norm = ((double)r + e->frac) / (double)NX;
  s->syn
      = wfm_synth_create (WFM_SYNTH_DSSS, FS, f_norm * FS, WFM_SYNTH_SNR_CLEAN,
                          1, e->seed, (int)SPC, 15, 0, 0, 0.0);
  if (!s->syn)
    return 1;
  int rc;
  if (e->data == WFM_DSSS_DATA_BITS) /* a transition mid-epoch, every epoch */
    rc = wfm_synth_set_dsss_cont (s->syn, code, SF, (double)SF / 2.0,
                                  WFM_DSSS_DATA_BITS, two_bits, 2);
  else
    rc = wfm_synth_set_dsss_cont (s->syn, code, SF, CHIP_RATE / SYM_RATE,
                                  e->data, NULL, 0);
  if (rc != 0)
    return 1;
  if (e->tau)
    {
      float complex *skip = malloc (e->tau * sizeof *skip);
      wfm_synth_steps (s->syn, skip, e->tau);
      free (skip);
    }
  s->amp  = (float)pow (10.0, e->level_db / 20.0);
  s->row  = e->tile;
  s->col  = (NX - e->tau) % NX;
  s->half = fabs (e->frac - 0.5) < 1e-9;
  return 0;
}

/* Does the hit report this source: its tile (a neighbour too when it sits
   half a tile off) and within one chip of its code phase, circularly. */
static int
hit_is (const acq_result_t *h, const source_t *s, size_t W)
{
  size_t dc = h->code_phase > s->col ? h->code_phase - s->col
                                     : s->col - h->code_phase;
  if (dc > NX / 2)
    dc = NX - dc;
  if (dc > SPC)
    return 0;
  if (h->doppler_bin == s->row)
    return 1;
  return s->half && h->doppler_bin == (s->row + 1) % W;
}

/* A hit at this source's code phase but not on its tile: a twin. */
static int
hit_is_twin (const acq_result_t *h, const source_t *s, size_t W)
{
  size_t dc = h->code_phase > s->col ? h->code_phase - s->col
                                     : s->col - h->code_phase;
  if (dc > NX / 2)
    dc = NX - dc;
  return dc <= SPC && !hit_is (h, s, W);
}

typedef struct
{
  acq_state_t   *a;
  awgn_state_t  *g;
  source_t       src[MAX_EMIT];
  size_t         n_src;
  float complex *epoch, *blk;
  size_t         W, n_noncoh;
} scene_t;

/* The scene: the noise at `cn0_dbhz` (a unit-power emitter's C/N0), the
   engine sized at `size_cn0` (the C/N0 of the emitter it must find), the
   sources. `max_peaks` configures the list. Returns 0 on success. */
static int
scene_open (scene_t *sc, const uint8_t *code, const emitter_t *em, size_t n_em,
            double cn0_dbhz, double size_cn0, size_t max_peaks, uint32_t seed)
{
  memset (sc, 0, sizeof *sc);
  sc->a = acq_create_continuous (code, SF, SPC, CHIP_RATE, SYM_RATE, size_cn0,
                                 DU, PFA, PD, 0);
  if (!sc->a || acq_set_max_peaks (sc->a, max_peaks) != 0)
    return 1;
  sc->W        = sc->a->window_bins;
  sc->n_noncoh = sc->a->n_noncoh;
  sc->g        = awgn_create (
      seed * 7919u + 1u,
      awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
  sc->epoch = malloc (NX * sizeof *sc->epoch);
  sc->blk   = malloc (NX * sizeof *sc->blk);
  if (!sc->g || !sc->epoch || !sc->blk)
    return 1;
  sc->n_src = n_em;
  for (size_t i = 0; i < n_em; i++)
    if (source_open (&sc->src[i], code, &em[i], sc->W))
      return 1;
  return 0;
}

static void
scene_close (scene_t *sc)
{
  for (size_t i = 0; i < sc->n_src; i++)
    wfm_synth_destroy (sc->src[i].syn);
  awgn_destroy (sc->g);
  acq_destroy (sc->a);
  free (sc->epoch);
  free (sc->blk);
}

/* One dwell: n_noncoh epochs of every source summed at its level, plus the
   noise, pushed one epoch at a time; the dwell's hits are returned. */
static size_t
scene_dwell (scene_t *sc, acq_result_t *hits, size_t max_hits)
{
  size_t nh = 0;
  for (size_t look = 0; look < sc->n_noncoh; look++)
    {
      awgn_generate (sc->g, NX, sc->blk, NX);
      for (size_t i = 0; i < sc->n_src; i++)
        {
          wfm_synth_steps (sc->src[i].syn, sc->epoch, NX);
          const float amp = sc->src[i].amp;
          for (size_t k = 0; k < NX; k++)
            sc->blk[k] += amp * sc->epoch[k];
        }
      nh += acq_push (sc->a, sc->blk, NX, hits + nh, max_hits - nh);
    }
  return nh;
}

/* Count, over the dwell's hits, each source's detections and twins, and
   the hits that are neither (false peaks). A hit is one source's at most:
   two emitters inside one zone are ONE peak, and that peak must not count
   as both found. */
static void
tally (const scene_t *sc, const acq_result_t *hits, size_t nh, int found[],
       int twins[], int *other)
{
  for (size_t i = 0; i < sc->n_src; i++)
    found[i] = twins[i] = 0;
  *other = 0;
  for (size_t h = 0; h < nh; h++)
    {
      int mine = 0;
      for (size_t i = 0; i < sc->n_src && !mine; i++)
        if (!found[i] && hit_is (&hits[h], &sc->src[i], sc->W))
          {
            found[i] = 1;
            mine     = 1;
          }
      for (size_t i = 0; i < sc->n_src && !mine; i++)
        if (hit_is_twin (&hits[h], &sc->src[i], sc->W))
          {
            twins[i]++;
            mine = 1;
          }
      if (!mine)
        (*other)++;
    }
}

/* Two emitters, `trials` independent scenes of one dwell each: the fraction
   of dwells listing both, and each one, and the worst reported offset. */
typedef struct
{
  double p_both, p_a, p_b;
  double twins_per_dwell, other_per_dwell;
} pair_t;

static int
pair_measure (const uint8_t *code, const emitter_t em[2], double cn0,
              double size_cn0, size_t max_peaks, int trials, uint32_t seed0,
              pair_t *out, size_t *n_noncoh)
{
  int both = 0, na = 0, nb = 0, tw = 0, ot = 0;
  for (int t = 0; t < trials; t++)
    {
      scene_t   sc;
      emitter_t e[2] = { em[0], em[1] };
      e[0].seed      = seed0 + 2u * (uint32_t)t;
      e[1].seed      = seed0 + 2u * (uint32_t)t + 1u;
      if (scene_open (&sc, code, e, 2, cn0, size_cn0, max_peaks,
                      seed0 + 1000u + (uint32_t)t))
        return 1;
      *n_noncoh = sc.n_noncoh;
      /* Two dwells, the second scored: the two-epoch rule holds a peak at
         an already-listed code phase for one dwell, so a real second
         emitter at the first's code phase is listed from the second dwell
         on -- one dwell alone would read the rule as a miss. */
      acq_result_t hits[MAX_HITS];
      (void)scene_dwell (&sc, hits, MAX_HITS);
      size_t nh = scene_dwell (&sc, hits, MAX_HITS);
      int    found[2], twins[2], other;
      tally (&sc, hits, nh, found, twins, &other);
      both += found[0] && found[1];
      na += found[0];
      nb += found[1];
      tw += twins[0] + twins[1];
      ot += other;
      scene_close (&sc);
    }
  out->p_both          = (double)both / trials;
  out->p_a             = (double)na / trials;
  out->p_b             = (double)nb / trials;
  out->twins_per_dwell = (double)tw / trials;
  out->other_per_dwell = (double)ot / trials;
  return 0;
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);

  /* The scene's two emitters: A at tile 5, code phase 777 samples; B four
     tiles and 100 chips away unless a cell says otherwise. */
  const emitter_t A = { 5, 0.0, 777, 0.0, WFM_DSSS_DATA_PRBS, 1u };
  const emitter_t B = { 9, 0.0, 777 + 200, 0.0, WFM_DSSS_DATA_PRBS, 2u };

  {
    scene_t   sc;
    emitter_t e[2] = { A, B };
    DP_REQUIRE (scene_open (&sc, code, e, 2, 45.0, 45.0, 1, 7u) == 0);
    printf ("engine at 45 dB-Hz: %zu tiles, %zu looks per dwell, eta_nc "
            "%.2f (threshold %.2f)\n\n",
            sc.W, sc.n_noncoh, sc.a->eta_nc, sc.a->threshold);
    scene_close (&sc);
  }

  if (check)
    {
      /* Two clean emitters: one hit at max_peaks 1 (the maximum), both at
         4, each on its own tile and code phase, and nothing else. */
      emitter_t e[2] = { A, B };
      e[0].data = e[1].data = WFM_DSSS_DATA_NONE;
      for (size_t mp = 1; mp <= 4; mp += 3)
        {
          scene_t sc;
          DP_REQUIRE (scene_open (&sc, code, e, 2, WFM_SYNTH_SNR_CLEAN,
                                  WFM_SYNTH_SNR_CLEAN, mp, 3u)
                      == 0);
          acq_result_t hits[MAX_HITS];
          size_t       nh = scene_dwell (&sc, hits, MAX_HITS);
          int          found[2], twins[2], other;
          tally (&sc, hits, nh, found, twins, &other);
          printf ("  clean pair, max_peaks %zu: %zu hit(s); A %d B %d twins "
                  "%d other %d\n",
                  mp, nh, found[0], found[1], twins[0] + twins[1], other);
          DP_CHECK (nh == (mp == 1 ? 1u : 2u));
          DP_CHECK (found[0] + found[1] == (int)(mp == 1 ? 1 : 2));
          DP_CHECK (other == 0 && twins[0] + twins[1] == 0);
          scene_close (&sc);
        }
      /* The two-epoch rule, deterministically: one emitter with a data
         transition exactly mid-epoch, every epoch, has NO peak on its own
         tile (the two halves cancel at its frequency) and equal peaks at
         its code phase two tiles either side -- §12.2's twins -- at the
         SAME tiles every epoch. Dwell 1 lists the strongest of them alone
         and holds the rest as same-phase candidates; dwell 2 lists those
         too, since they are still there at the same tiles. Every hit is at
         the emitter's code phase; nothing else is listed. */
      {
        emitter_t t = A;
        t.data      = WFM_DSSS_DATA_BITS;
        scene_t sc;
        DP_REQUIRE (scene_open (&sc, code, &t, 1, WFM_SYNTH_SNR_CLEAN,
                                WFM_SYNTH_SNR_CLEAN, 8, 5u)
                    == 0);
        DP_REQUIRE (sc.n_noncoh == 1); /* clean: one look per dwell */
        acq_result_t hits[MAX_HITS];
        int          found[1], twins[1], other;
        size_t       nh = scene_dwell (&sc, hits, MAX_HITS);
        tally (&sc, hits, nh, found, twins, &other);
        printf ("  split emitter, dwell 1: %zu hit(s) at its code phase %d, "
                "held %zu\n",
                nh, found[0] + twins[0], sc.a->n_twins);
        DP_CHECK (nh == 1 && found[0] + twins[0] == 1 && other == 0);
        DP_CHECK (sc.a->n_twins >= 1);
        nh = scene_dwell (&sc, hits, MAX_HITS);
        tally (&sc, hits, nh, found, twins, &other);
        printf ("  split emitter, dwell 2: %zu hit(s) at its code phase %d\n",
                nh, found[0] + twins[0]);
        DP_CHECK (nh >= 2 && found[0] + twins[0] == (int)nh && other == 0);
        scene_close (&sc);
      }
      /* Two emitters at the SAME code phase on different tiles, 45 dB-Hz:
         the rule holds the second on dwell 1 and lists it on dwell 2. */
      {
        emitter_t e[2] = { A, B };
        e[1].tau       = A.tau; /* B at A's code phase, four tiles away */
        pair_t r;
        size_t nc;
        DP_REQUIRE (pair_measure (code, e, 45.0, 45.0, 4, 10, 500u, &r, &nc)
                    == 0);
        printf ("  same code phase, four tiles apart, dwell 2 of 10 scenes: "
                "both %.2f\n",
                r.p_both);
        DP_CHECK (r.p_both >= 0.9);
      }
      /* Under PRBS data at 45 dB-Hz the transitions move, so the twins do
         not recur at one tile: the emitter is listed every dwell and its
         twins almost never are. */
      {
        scene_t sc;
        DP_REQUIRE (scene_open (&sc, code, &A, 1, 45.0, 45.0, 8, 9u) == 0);
        int listed = 0, tw = 0;
        for (int d = 0; d < 40; d++)
          {
            acq_result_t hits[MAX_HITS];
            int          found[1], twins[1], other;
            size_t       nh = scene_dwell (&sc, hits, MAX_HITS);
            tally (&sc, hits, nh, found, twins, &other);
            listed += found[0];
            tw += twins[0];
          }
        printf ("  PRBS emitter at 45 dB-Hz, 40 dwells: listed %d, twins "
                "listed %d\n",
                listed, tw);
        DP_CHECK (listed >= 38);
        DP_CHECK (tw <= 4);
        scene_close (&sc);
      }
      /* Pure noise under the list: the per-dwell false-alarm rate is the
         configured pfa whatever max_peaks is -- the first pick is the
         maximum the classic detector gated, so on the SAME noise the same
         dwells report at max_peaks 1 and 4, and 2000 dwells at 1e-2 land
         near 20 (held under 3x and above 0.2x). */
      {
        int reported[2] = { 0, 0 };
        for (size_t mp = 1; mp <= 4; mp += 3)
          {
            acq_state_t *a = acq_create_continuous (
                code, SF, SPC, CHIP_RATE, SYM_RATE, 45.0, DU, 1e-2, PD, 0);
            DP_REQUIRE (a != NULL && acq_set_max_peaks (a, mp) == 0);
            awgn_state_t *g = awgn_create (
                31u, awgn_amplitude_for_snr ((float)(45.0 - 10.0 * log10 (FS)),
                                             1.0f));
            float complex *blk          = malloc (NX * sizeof *blk);
            int            false_dwells = 0;
            const int      dwells       = 2000;
            for (int d = 0; d < dwells; d++)
              {
                acq_result_t hits[MAX_HITS];
                size_t       nh = 0;
                for (size_t look = 0; look < a->n_noncoh; look++)
                  {
                    awgn_generate (g, NX, blk, NX);
                    nh += acq_push (a, blk, NX, hits + nh, MAX_HITS - nh);
                  }
                false_dwells += nh > 0;
              }
            printf ("  noise, pfa 1e-2, max_peaks %zu: %d of %d dwells "
                    "reported (%.4f)\n",
                    mp, false_dwells, dwells, (double)false_dwells / dwells);
            DP_CHECK (false_dwells >= 4 && false_dwells <= 60);
            reported[mp == 1 ? 0 : 1] = false_dwells;
            free (blk);
            awgn_destroy (g);
            acq_destroy (a);
          }
        DP_CHECK (reported[0] == reported[1]);
      }
      DP_TEST_END ("validate_acq_peak_list");
    }

  const int trials = 200;

  /* ── separability ── */
  printf ("separability: two equal emitters at 45 dB-Hz, max_peaks 4, %d "
          "dwells per cell\n",
          trials);
  printf ("  df tiles  dtau chips   P(both)  P(A)   P(B)   twins/dwell  "
          "other/dwell\n");
  const double dfs[]   = { 0.5, 1.0, 2.0, 4.0 };
  const double dtaus[] = { 0.5, 1.0, 2.0, 4.0 };
  for (int fi = 0; fi < 4; fi++)
    for (int ti = 0; ti < 4; ti++)
      {
        emitter_t e[2] = { A, A };
        e[1].seed      = 2u;
        e[1].tile      = A.tile + (size_t)floor (dfs[fi]);
        e[1].frac      = dfs[fi] - floor (dfs[fi]);
        e[1].tau       = A.tau + (size_t)(dtaus[ti] * SPC);
        pair_t r;
        size_t nc;
        if (pair_measure (code, e, 45.0, 45.0, 4, trials, 100u, &r, &nc))
          return 1;
        printf ("  %8.1f  %10.1f   %6.2f  %5.2f  %5.2f   %10.2f   %10.2f\n",
                dfs[fi], dtaus[ti], r.p_both, r.p_a, r.p_b, r.twins_per_dwell,
                r.other_per_dwell);
      }

  /* ── the knee ── */
  const int ktrials = 100;
  printf ("\nthe knee: strong emitter fixed, weak one stepped down, 4 tiles "
          "and 100 chips away, max_peaks 4, the engine sized at the WEAK "
          "emitter's C/N0 (so it is the floor that decides, not the sizing), "
          "%d scenes per point; and the weak one ALONE at the same level\n",
          ktrials);
  const double strongs[]    = { 55.0, 45.0 };
  const double max_spread[] = { 27.0, 15.0 }; /* the weak one >= 30 dB-Hz */
  for (int si = 0; si < 2; si++)
    {
      printf ("  strong at %.0f dB-Hz\n", strongs[si]);
      printf ("  spread dB   weak dB-Hz   looks   P(weak with strong)   "
              "P(weak alone)   P(strong)   twins/dwell\n");
      for (double spread = 0.0; spread <= max_spread[si]; spread += 3.0)
        {
          const double weak_cn0 = strongs[si] - spread;
          emitter_t    e[2]     = { A, B };
          e[1].level_db         = -spread;
          pair_t with, alone;
          size_t nc;
          if (pair_measure (code, e, strongs[si], weak_cn0, 4, ktrials, 300u,
                            &with, &nc))
            return 1;
          emitter_t only[2] = { B, B };
          only[0].level_db  = -spread;
          only[1].level_db  = -200.0; /* the second source silent */
          only[1].seed      = 77u;
          if (pair_measure (code, only, strongs[si], weak_cn0, 4, ktrials,
                            300u, &alone, &nc))
            return 1;
          printf ("  %9.0f   %10.0f   %5zu   %19.2f   %13.2f   %9.2f   "
                  "%11.2f\n",
                  spread, weak_cn0, nc, with.p_b, alone.p_a, with.p_a,
                  with.twins_per_dwell);
        }
    }

  /* ── pfa under the list ── */
  printf ("\npfa under the list: pure noise, engine sized at 45 dB-Hz, "
          "configured pfa 1e-2\n");
  printf ("  max_peaks   dwells   reported   rate      peaks per reported "
          "dwell\n");
  const size_t mps[] = { 1, 4, 8 };
  for (int mi = 0; mi < 3; mi++)
    {
      acq_state_t *a = acq_create_continuous (code, SF, SPC, CHIP_RATE,
                                              SYM_RATE, 45.0, DU, 1e-2, PD, 0);
      if (!a || acq_set_max_peaks (a, mps[mi]) != 0)
        return 1;
      awgn_state_t *g = awgn_create (
          41u + (uint32_t)mi,
          awgn_amplitude_for_snr ((float)(45.0 - 10.0 * log10 (FS)), 1.0f));
      float complex *blk    = malloc (NX * sizeof *blk);
      const int      dwells = 20000;
      int            rep = 0, peaks = 0;
      for (int d = 0; d < dwells; d++)
        {
          acq_result_t hits[MAX_HITS];
          size_t       nh = 0;
          for (size_t look = 0; look < a->n_noncoh; look++)
            {
              awgn_generate (g, NX, blk, NX);
              nh += acq_push (a, blk, NX, hits + nh, MAX_HITS - nh);
            }
          rep += nh > 0;
          peaks += (int)nh;
        }
      printf ("  %9zu   %6d   %8d   %.4f    %.2f\n", mps[mi], dwells, rep,
              (double)rep / dwells, rep ? (double)peaks / rep : 0.0);
      free (blk);
      awgn_destroy (g);
      acq_destroy (a);
    }
  /* ...and with one strong emitter present: false peaks in its sidelobes,
     outside its zone and not at its code phase. */
  printf ("\n  with one emitter present (PRBS data), max_peaks 8, 2000 "
          "dwells: peaks that are neither it nor its twin\n");
  printf ("  C/N0    other peaks per dwell   twins per dwell   listed\n");
  const double cn0s[] = { 55.0, 45.0 };
  for (int ci = 0; ci < 2; ci++)
    {
      scene_t sc;
      if (scene_open (&sc, code, &A, 1, cn0s[ci], cn0s[ci], 8, 51u))
        return 1;
      int       listed = 0, tw = 0, ot = 0;
      const int dwells = 2000;
      for (int d = 0; d < dwells; d++)
        {
          acq_result_t hits[MAX_HITS];
          int          found[1], twins[1], other;
          size_t       nh = scene_dwell (&sc, hits, MAX_HITS);
          tally (&sc, hits, nh, found, twins, &other);
          listed += found[0];
          tw += twins[0];
          ot += other;
        }
      printf ("  %4.0f    %21.4f   %15.4f   %d/%d\n", cn0s[ci],
              (double)ot / dwells, (double)tw / dwells, listed, dwells);
      scene_close (&sc);
    }
  return 0;
}
