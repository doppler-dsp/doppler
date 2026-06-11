/*
 * wfmgen_main.c — the `wfmgen` executable's process entry point.
 *
 * Deliberately the *only* translation unit that defines `main`: the CLI body
 * lives in wfmgen.c as the callable `doppler_wfmgen()`, which is archived into
 * libdoppler. Keeping `main` in this separate, binary-only TU is what lets the
 * library carry the generator without a `main` symbol colliding in a
 * downstream that links libdoppler.a alongside its own `main`.
 */
#include "wfm/wfmgen.h"

int
main (int argc, char *argv[])
{
  return doppler_wfmgen (argc, argv);
}
