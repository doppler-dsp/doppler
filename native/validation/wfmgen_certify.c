/*
 * wfmgen_certify.c — the C leg of wfmgen's four-API byte-identity claim.
 *
 * `docs/design/wfmgen.md` goal 2 promises that "the same scene expressed
 * through any of the four renders byte-identically", and names the C API as
 * the PRIMARY one. Two of the four legs were pinned before this existed:
 * Python against the CLI (`test_compose.py::test_byte_parity_vs_wfmgen`) and
 * JSON against the CLI (`test_cli_record_replays.py`). The C leg was not.
 *
 * What stood in for it was `native/examples/wfmgen_demo.c`, whose §5 says
 * "the same declaration composes byte-identically" and then composes one
 * scene TWICE IN ONE PROCESS and memcmp's the two buffers. That is
 * determinism — a real property, and not this one. It would pass unchanged
 * if the C API and the CLI had diverged completely.
 *
 * So this harness renders through the STRUCT API — `dp_wfm_compose_create`
 * over a hand-built `wfm_source_t`, not `dp_wfm_compose_from_json` — and
 * writes the raw samples out for something else to compare. Going through JSON
 * here would have made all three legs share one parser, and a consistency test
 * is structurally blind to any defect its paths share (validation.md, step 2).
 *
 * IT DECIDES NOTHING. It renders and writes; whether the three renders agree
 * is `validate.py`'s limit to hold, because two places deciding that would be
 * two envelopes and the one nobody runs would be the one that is wrong.
 * It does not hash, either: the comparison wants ONE hash implementation, and
 * Python already has `hashlib`.
 *
 * Usage:
 *   wfmgen_certify --list                 scene names, one per line
 *   wfmgen_certify --render <name> <path> raw interleaved cf32, native LE
 *   wfmgen_certify --check                render every scene, exit 0 if sane
 */
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h"

#include "doppler/dp_complex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The defaults a C caller is given: WFM_SOURCE_DEFAULTS, generated from the
 * manifest (#1142). This harness once replicated three of them by hand, and
 * said so as a finding -- the C API told a caller none of the values goal 2
 * depends on. It now starts from the one declaration every face reads, so
 * the C leg measures what a C caller actually gets. */
typedef struct
{
  const char *name;
  int         type; /* a wfm_type index; see TYPE_NAMES in wfm_names.h */
} scene_t;

/* Deliberately the SAME five waveform types the Python<->CLI parity test
   already sweeps, so the C leg slots into an existing comparison rather than
   inventing a second matrix that could drift from it. */
static const scene_t SCENES[] = {
  { "tone", 0 }, { "noise", 1 }, { "pn", 2 }, { "bpsk", 3 }, { "qpsk", 4 },
};
#define N_SCENES (sizeof SCENES / sizeof *SCENES)

/* The one scene geometry, matching test_byte_parity_vs_wfmgen's flags:
   --fs 1e6 --freq 1e5 --count 1024 --snr 100. */
#define SCENE_FS 1.0e6
#define SCENE_FREQ 1.0e5
#define SCENE_N 1024u

/* Render one scene into `out` (SCENE_N samples). Returns samples produced. */
static size_t
render (const scene_t *sc, float complex *out)
{
  wfm_source_t src = WFM_SOURCE_DEFAULTS;
  src.type         = sc->type;
  src.freq         = SCENE_FREQ;

  wfm_segment_t seg = { 0 };
  seg.sources       = &src;
  seg.n_sources     = 1;
  seg.fs            = SCENE_FS;
  seg.num_samples   = SCENE_N;

  dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
  if (!c)
    return 0;
  size_t n = dp_wfm_compose_execute (c, out, SCENE_N);
  dp_wfm_compose_destroy (c);
  return n;
}

static const scene_t *
find_scene (const char *name)
{
  for (size_t i = 0; i < N_SCENES; i++)
    if (strcmp (SCENES[i].name, name) == 0)
      return &SCENES[i];
  return NULL;
}

int
main (int argc, char **argv)
{
  const char *mode = (argc > 1) ? argv[1] : "--check";

  if (strcmp (mode, "--list") == 0)
    {
      for (size_t i = 0; i < N_SCENES; i++)
        printf ("%s\n", SCENES[i].name);
      return 0;
    }

  if (strcmp (mode, "--render") == 0)
    {
      if (argc < 4)
        {
          fprintf (stderr, "usage: wfmgen_certify --render <name> <path>\n");
          return 2;
        }
      const scene_t *sc = find_scene (argv[2]);
      if (!sc)
        {
          fprintf (stderr, "wfmgen_certify: no scene named '%s'\n", argv[2]);
          return 2;
        }
      float complex *buf = calloc (SCENE_N, sizeof *buf);
      if (!buf)
        return 1;
      const size_t n = render (sc, buf);
      if (n != SCENE_N)
        {
          fprintf (stderr, "wfmgen_certify: %s produced %zu of %u samples\n",
                   sc->name, n, SCENE_N);
          free (buf);
          return 1;
        }
      FILE *f = fopen (argv[3], "wb");
      if (!f)
        {
          fprintf (stderr, "wfmgen_certify: cannot open %s\n", argv[3]);
          free (buf);
          return 1;
        }
      const size_t wrote = fwrite (buf, sizeof *buf, n, f);
      const int    ferr  = ferror (f) || fclose (f) != 0;
      free (buf);
      if (wrote != n || ferr)
        {
          fprintf (stderr, "wfmgen_certify: short write to %s\n", argv[3]);
          return 1;
        }
      return 0;
    }

  /* --check: every scene renders its full length. Sanity only -- the
     cross-API comparison is validate.py's, and this must not duplicate it. */
  float complex *buf = calloc (SCENE_N, sizeof *buf);
  if (!buf)
    return 1;
  for (size_t i = 0; i < N_SCENES; i++)
    {
      const size_t n = render (&SCENES[i], buf);
      if (n != SCENE_N)
        {
          fprintf (stderr, "wfmgen_certify: %s produced %zu of %u samples\n",
                   SCENES[i].name, n, SCENE_N);
          free (buf);
          return 1;
        }
    }
  free (buf);
  printf ("wfmgen_certify: %zu scene(s) render at full length\n", N_SCENES);
  return 0;
}
