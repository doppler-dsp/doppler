/**
 * @file iq_info.c
 * @brief The C half of the example — links libdoppler.a, no Python involved.
 *
 * `capture_core.c` is compiled into this project's own library, so the same
 * façade the Python extension binds is also callable straight from C. This
 * program is the proof: it opens a capture both ways and prints what each
 * constructor believes about it.
 *
 * Build (from the project root):
 * @code
 * cmake -B build . -Ddoppler_DIR=/path/to/doppler/build
 * cmake --build build --target iq_info
 * ./build/tools/iq_info capture.blue
 * ./build/tools/iq_info capture.raw ci16 2.4e6
 * @endcode
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capture/capture_core.h"

/** Sample-type names, in the order doppler encodes them (0..4). */
static const char *const SAMPLE_TYPES[]
    = { "cf32", "cf64", "ci32", "ci16", "ci8" };
#define N_SAMPLE_TYPES ((int)(sizeof SAMPLE_TYPES / sizeof SAMPLE_TYPES[0]))

/** Provenance names, matching the `metadata_source` enum. */
static const char *const META[] = { "none", "file", "supplied" };

/** Map a sample-type name to doppler's integer encoding, or -1. */
static int
sample_type_index (const char *name)
{
  int i;

  for (i = 0; i < N_SAMPLE_TYPES; i++)
    if (strcmp (name, SAMPLE_TYPES[i]) == 0)
      return i;
  return -1;
}

/** Print everything the façade knows, then drain the capture. */
static int
report (const char *label, capture_state_t *cap)
{
  float complex buf[4096];
  size_t        total = 0;
  size_t        n;

  if (cap == NULL)
    {
      fprintf (stderr, "%s: could not open the capture\n", label);
      return 1;
    }

  printf ("  %-12s fs=%12.1f Hz  fc=%14.1f Hz  n=%-8zu source=%s\n", label,
          capture_get_fs (cap), capture_get_fc (cap),
          capture_get_num_samples (cap),
          META[capture_get_metadata_source (cap)]);

  /* Drain it, to show the read path works the same from C as from Python. */
  while ((n = capture_read (cap, 4096, buf, 4096)) > 0)
    total += n;
  printf ("  %-12s streamed %zu samples\n", "", total);

  capture_destroy (cap);
  return 0;
}

int
main (int argc, char **argv)
{
  const char *path;
  char       *end;
  double      fs;
  int         st;

  if (argc < 2)
    {
      fprintf (stderr,
               "usage: %s <capture> [sample_type] [fs]\n"
               "  sample_type: cf32 cf64 ci32 ci16 ci8 (enables RawCapture)\n",
               argv[0]);
      return 2;
    }

  path = argv[1];
  printf ("capture: %s\n", path);
  fs = 1.0;

  /* 1. The auto-detecting constructor: right for a self-describing file. */
  if (report ("Capture", capture_create (path)) != 0)
    return 1;

  /* 2. The view's constructor, when the caller supplied what the file cannot
        carry. Same core, same read path — only the way in differs. */
  if (argc >= 3)
    {
      st = sample_type_index (argv[2]);
      if (st < 0)
        {
          fprintf (stderr, "unknown sample type '%s'\n", argv[2]);
          return 2;
        }
      /* strtod, not atof: atof cannot report a bad number, and silently
         reading a typo'd sample rate as 0.0 is exactly the kind of quiet
         wrong answer this whole example is about. */
      if (argc >= 4)
        {
          fs = strtod (argv[3], &end);
          if (end == argv[3] || *end != '\0')
            {
              fprintf (stderr, "not a number: '%s'\n", argv[3]);
              return 2;
            }
        }

      if (report ("RawCapture", capture_open_raw (path, st, 0, fs, 0.0)) != 0)
        return 1;
    }

  return 0;
}
