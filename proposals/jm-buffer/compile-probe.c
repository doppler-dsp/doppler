/* Builds the generated fragment against this repo's real buffer.h.
 *
 * Run it through ./regenerate.sh, which compiles AND LINKS this file --
 * see the note there about why linking is the step that matters.
 *
 * The include names are the files as they exist beside this one. An earlier
 * version named `shim.h` and `frag.c`, which are not in the tree, so the
 * documented command could not run at all and the "it compiles" claim was
 * not reproducible. Hence regenerate.sh now runs it.
 */
#define _GNU_SOURCE
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "proposed-siblings.h"
#include <numpy/arrayobject.h>

/* `dp_interrupted()` is the one symbol buffer.h's wait spin needs from
   doppler's interrupt subsystem, whose real implementation pulls in the
   whole guard chain. Never-interrupted is right for a probe with no signal
   handler; doppler compiles the real one into every module. */
int
dp_interrupted (void)
{
  return 0;
}

#include "generated/buffer_ext_f32_buffer.c"

/* The module aggregator is what references the type object; this probe
   includes only the fragment, so name it once. Without this the build is
   "clean except for one warning", and an expected warning is how an
   unexpected one goes unread. */
const void *dp_probe_keep = &F32BufferObjType;
