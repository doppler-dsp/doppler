/*
 * main.c — a minimal downstream consumer of the doppler C library.
 *
 * Drives the in-process waveform generator (doppler_wfmgen, the same engine as
 * the `wfmgen` CLI) to write a small QPSK capture to a file. wfmgen lives in
 * the pure-C core, and its `zmq://` path is a weak seam (resolved only by the
 * optional libdoppler_stream component) — so this file-output consumer links
 * against the core alone with just `-lm`, proving the core is C++/zmq-free.
 *
 * Builds against either link mode; see CMakeLists.txt (find_package) and the
 * pkg-config commands in docs/install/c.md.
 */
#include <stddef.h>
#include <stdio.h>

#include "wfm/wfmgen.h"

int
main (void)
{
  char *argv[] = { "wfmgen", "--type",   "qpsk",          "--count",
                   "1024",   "--output", "consumer.cf32", NULL };
  int   rc     = doppler_wfmgen (7, argv);
  printf ("doppler_wfmgen rc=%d -> consumer.cf32\n", rc);
  return rc;
}
