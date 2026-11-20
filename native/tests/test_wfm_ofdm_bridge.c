/**
 * @file test_wfm_ofdm_bridge.c
 * @brief The wfmgen `ofdm` type through the bridge: a source with a data
 * source renders the PPDUs `wlan` builds from the same octets
 * (docs/design/wlan.md, section 5).
 */
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h"
#include "doppler/wfm/wfm_frame.h"
#include "doppler/wfm_compose/wfm_compose_bridge.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "doppler/wlan/wlan_core.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef float _Complex cf;

/* 100 octets of a fixed pattern, as 800 literal bits, first bit least
   significant: the bits the bridge packs back into these octets. */
static uint8_t bits_lit[1600]; /* the 800 bits twice: two frames */
static uint8_t octets[100];

static void
make_data (void)
{
  for (size_t k = 0; k < 100; k++)
    {
      octets[k] = (uint8_t)(k * 7u + 3u);
      for (unsigned j = 0; j < 8u; j++)
        bits_lit[8u * k + j] = bits_lit[800u + 8u * k + j]
            = (octets[k] >> j) & 1u;
    }
}

static wfm_source_t
source (void)
{
  wfm_source_t s = WFM_SOURCE_DEFAULTS;
  s.type         = WFM_SYNTH_FRAMES;
  s.snr          = 100.0;
  s.data
      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 1600, .bits = bits_lit };
  s.data_len       = 800;
  s.wlan           = 36;
  s.scrambler_init = 93;
  return s;
}

static int
test_renders_the_ppdu_wlan_builds (void)
{
  wfm_source_t s   = source ();
  const char  *why = dp_wfm_source_to_synth_error (&s, 20e6);
  if (why)
    DP_TEST_ERR ("refused: %s", why);
  DP_REQUIRE (why == NULL);
  dp_wfm_synth_state_t *syn = dp_wfm_source_to_synth (&s, 20e6);
  DP_REQUIRE (syn != NULL);
  const dp_wlan_rate_t rate  = { 4u, WLAN_CODE_3_4 };
  const size_t         n_sym = dp_wlan_rate_n_sym (&rate, 100);
  const size_t         n     = dp_wlan_ppdu_len (n_sym, 0u, 1u);
  cf                  *got   = malloc (n * sizeof *got);
  cf                  *want  = malloc (n * sizeof *want);
  DP_REQUIRE (got && want);
  dp_wfm_synth_steps (syn, got, n);
  DP_REQUIRE (dp_wlan_ppdu (dp_wlan_tables_standard (), &rate, octets, 100, 93,
                            0u, 1u, want, n)
              == n);
  double num = 0, den = 0;
  for (size_t i = 0; i < n; i++)
    {
      num += cabs ((double _Complex)got[i] - want[i])
             * cabs ((double _Complex)got[i] - want[i]);
      den += cabs (want[i]) * cabs (want[i]);
    }
  DP_CHECK (den > 0.0 && sqrt (num / den) < 1e-6);
  free (got);
  free (want);
  dp_wfm_synth_destroy (syn);
  return 0;
}

/* Render `n` samples of a source at fs. */
static cf *
render (const wfm_source_t *s, double fs, size_t n)
{
  dp_wfm_synth_state_t *syn = dp_wfm_source_to_synth (s, fs);
  if (!syn)
    return NULL;
  cf *x = malloc (n * sizeof *x);
  if (x)
    dp_wfm_synth_steps (syn, x, n);
  dp_wfm_synth_destroy (syn);
  return x;
}

/* wlan N is a spelling, not a second code path: for each of the eight
   rates the source spelled with it renders the bytes of the source spelled
   with the pair Table 17-6 names for N. The expansion is held, not the
   content (that is test_wlan_core.c). */
static int
test_wlan_n_is_its_pair_for_all_eight (void)
{
  static const unsigned   rates[8] = { 6, 9, 12, 18, 24, 36, 48, 54 };
  const dp_wlan_tables_t *t        = dp_wlan_tables_standard ();
  for (unsigned i = 0; i < 8; i++)
    {
      wfm_source_t a = source ();
      a.wlan         = (int)rates[i];
      dp_wlan_rate_t pair;
      DP_REQUIRE (dp_wlan_rate_from_mbps (t, rates[i], &pair));
      wfm_source_t b  = source ();
      b.wlan          = 0;
      b.constellation = pair.n_bpsc == 1u   ? 0
                        : pair.n_bpsc == 2u ? 1
                        : pair.n_bpsc == 4u ? 2
                                            : 3;
      b.code_rate     = (int)pair.code;
      const size_t n  = 600; /* the SIGNAL and Data symbols are in it */
      cf          *x  = render (&a, 20e6, n);
      cf          *y  = render (&b, 20e6, n);
      DP_REQUIRE (x && y);
      DP_CHECK (memcmp (x, y, n * sizeof *x) == 0);
      /* control: a different rate is not the same bytes */
      wfm_source_t c = a;
      c.wlan         = (int)rates[(i + 1) % 8];
      cf *z          = render (&c, 20e6, n);
      DP_REQUIRE (z);
      DP_CHECK (memcmp (x, z, n * sizeof *x) != 0);
      free (x);
      free (y);
      free (z);
    }
  return 0;
}

typedef struct
{
  const char *name;
  void (*edit) (wfm_source_t *);
  const char *names; /* the sentence says this */
} refusal_t;

static void
r_wlan7 (wfm_source_t *s)
{
  s->wlan = 7;
}
static void
r_both (wfm_source_t *s)
{
  s->constellation = 2;
}
static void
r_bw (wfm_source_t *s)
{
  s->bandwidth = 10e6;
}
static void
r_type (wfm_source_t *s)
{
  s->type = WFM_SYNTH_BPSK;
}
static void
r_esno (wfm_source_t *s)
{
  s->snr_mode = 3;
}
static void
r_ebno (wfm_source_t *s)
{
  s->snr_mode = 2;
}
static void
r_macdata (wfm_source_t *s)
{
  s->mac = 2;
}
static void
r_len (wfm_source_t *s)
{
  s->data_len = 12;
}
static void
r_scr (wfm_source_t *s)
{
  s->scrambler_init = 128;
}
static void
r_win (wfm_source_t *s)
{
  s->window = 81;
}
static void
r_sps (wfm_source_t *s)
{
  s->sps = 4;
}
static void
r_nodata (wfm_source_t *s)
{
  s->data = (wfm_seq_t){ 0 };
}

static int
test_each_refusal_names_its_field (void)
{
  static const refusal_t t[] = {
    { "wlan not one of the eight", r_wlan7, "wlan" },
    { "wlan with constellation", r_both, "wlan" },
    { "wlan with another bandwidth", r_bw, "bandwidth" },
    { "wlan on another type", r_type, "wlan" },
    { "esno", r_esno, "snr_mode" },
    { "ebno", r_ebno, "snr_mode" },
    { "mac data without a header", r_macdata, "mac" },
    { "data_len not whole octets", r_len, "octets" },
    { "scrambler_init above 127", r_scr, "scrambler_init" },
    { "window beyond 80 M", r_win, "window" },
    { "sps on a finished waveform", r_sps, "sps" },
    { "no data source", r_nodata, "data" },
  };
  for (size_t k = 0; k < sizeof t / sizeof *t; k++)
    {
      wfm_source_t s = source ();
      t[k].edit (&s);
      if (s.type != WFM_SYNTH_FRAMES)
        s.wlan = 36; /* the field a type that is not ofdm must refuse */
      const char *why = dp_wfm_source_to_synth_error (&s, 20e6);
      if (!why || !strstr (why, t[k].names))
        DP_TEST_ERR ("%s: got \"%s\"", t[k].name, why ? why : "(accepted)");
      DP_CHECK (why && strstr (why, t[k].names));
      /* the bridge refuses what the validator refuses */
      DP_CHECK (dp_wfm_source_to_synth (&s, 20e6) == NULL);
    }
  /* a bandwidth that fs is not an integer multiple of */
  wfm_source_t s  = source ();
  s.wlan          = 0;
  s.constellation = 1;
  const char *why = dp_wfm_source_to_synth_error (&s, 30e6);
  DP_CHECK (why && strstr (why, "multiple"));
  /* controls: the source the table edits is itself accepted, at M = 1, 2 */
  s = source ();
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 20e6) == NULL);
  DP_CHECK (dp_wfm_source_to_synth_error (&s, 40e6) == NULL);
  return 0;
}

/* The gap is noise at the source's noise variance, like every sample of the
   source: with snr 10 dB against a unit-power PPDU the gap's variance is 0.1.
   A clean source's gap is exact zeros. */
static int
test_the_gap_carries_noise_at_the_noise_variance (void)
{
  wfm_source_t s    = source ();
  s.gap             = 20000;
  const size_t ppdu = 160 + 160 + 80 + 80 * 8; /* 36 Mb/s, 104 octets */
  (void)ppdu;
  const dp_wlan_rate_t rate = { 4u, WLAN_CODE_3_4 };
  const size_t         len
      = dp_wlan_ppdu_len (dp_wlan_rate_n_sym (&rate, 100), 0u, 1u);
  cf *clean = render (&s, 20e6, len + s.gap);
  DP_REQUIRE (clean);
  for (size_t i = len; i < len + s.gap; i++)
    DP_CHECK (clean[i] == 0.0f);
  free (clean);
  s.snr = 10.0;
  cf *x = render (&s, 20e6, len + s.gap);
  DP_REQUIRE (x);
  double p = 0;
  for (size_t i = len; i < len + s.gap; i++)
    p += cabs (x[i]) * cabs (x[i]);
  p /= (double)s.gap;
  /* 20000 complex samples: the sample variance is within 3 % (sigma of the
     estimate is 1/sqrt(20000) = 0.7 %), so 5 sigma is the bound. */
  DP_CHECK (fabs (p / 0.1 - 1.0) < 5.0 / sqrt ((double)s.gap));
  free (x);
  return 0;
}

/* A resume mid-PPDU, in the second frame, continues byte for byte; a blob of
   another configuration is refused. */
static int
test_a_resume_mid_ppdu_continues_and_a_key_is_refused (void)
{
  wfm_source_t s   = source ();
  s.scrambler_init = 0; /* drawn per PPDU, so the draw's frame index travels */
  s.seed           = 5;
  const size_t          cut = 880 + 333, n = 880 * 2 + 200;
  dp_wfm_synth_state_t *a = dp_wfm_source_to_synth (&s, 20e6);
  DP_REQUIRE (a);
  cf *want = malloc (n * sizeof *want);
  DP_REQUIRE (want);
  dp_wfm_synth_steps (a, want, cut);
  const size_t nb   = dp_wfm_synth_state_bytes (a);
  uint8_t     *blob = malloc (nb);
  DP_REQUIRE (blob);
  dp_wfm_synth_get_state (a, blob);
  dp_wfm_synth_steps (a, want + cut, n - cut);
  dp_wfm_synth_state_t *b = dp_wfm_source_to_synth (&s, 20e6);
  DP_REQUIRE (b && dp_wfm_synth_set_state (b, blob) == DP_OK);
  cf *got = malloc ((n - cut) * sizeof *got);
  DP_REQUIRE (got);
  dp_wfm_synth_steps (b, got, n - cut);
  DP_CHECK (memcmp (got, want + cut, (n - cut) * sizeof *got) == 0);
  /* control: the second PPDU is not the first (the scrambler was drawn anew)
   */
  DP_CHECK (memcmp (want, want + 880, 880 * sizeof *want) != 0);
  /* each configuration key: a different rate, window, seed, gap and mac */
  wfm_source_t o[5];
  for (int k = 0; k < 5; k++)
    o[k] = s;
  o[0].wlan   = 24;
  o[1].window = 4;
  o[2].seed   = 6;
  o[3].gap    = 8;
  o[4].mac    = 1;
  for (int k = 0; k < 5; k++)
    {
      dp_wfm_synth_state_t *c = dp_wfm_source_to_synth (&o[k], 20e6);
      DP_REQUIRE (c);
      DP_CHECK (dp_wfm_synth_state_bytes (c) != 0);
      DP_CHECK (dp_wfm_synth_set_state (c, blob) == DP_ERR_INVALID);
      dp_wfm_synth_destroy (c);
    }
  free (want);
  free (got);
  free (blob);
  dp_wfm_synth_destroy (a);
  dp_wfm_synth_destroy (b);
  return 0;
}

int
main (void)
{
  make_data ();
  int rc = 0;
  rc |= test_renders_the_ppdu_wlan_builds ();
  rc |= test_wlan_n_is_its_pair_for_all_eight ();
  rc |= test_each_refusal_names_its_field ();
  rc |= test_the_gap_carries_noise_at_the_noise_variance ();
  rc |= test_a_resume_mid_ppdu_continues_and_a_key_is_refused ();
  if (rc)
    return 1;
  DP_TEST_END ("wfm_ofdm_bridge");
  return 0;
}
