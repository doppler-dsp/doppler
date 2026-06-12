/*
 * main.c — a minimal downstream consumer of the doppler C library.
 *
 * Drives the in-process waveform generator (doppler_wfmgen, the same engine as
 * the `wfmgen` CLI) to write a small QPSK capture. It exercises the wfm path —
 * which pulls the vendored zmq sink — so it is a meaningful link test: if the
 * static archive were not self-contained, the static build would fail to link.
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
