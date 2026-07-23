/*
 * write_blue_header.c — wfm_writer module-level function.
 *
 * Path-taking public alias for the BLUE type-1000 HCB header writer. The
 * kernel lives once in wfm_writer_core.c (wfm_blue_write_hcb, FILE-based,
 * shared with the wfmgen CLI and the streaming Writer — C-first, not
 * duplicated here); this thin shim opens the path so the whole surface — path
 * coercion, string-enum sample_type / endian, and raise-on-failure — is a
 * generated jm function (check_return + enum:stype/endian, gh-353/gh-363)
 * instead of a hand-written binding with its own copy of the enum tables.
 * It lives on the wfm_writer module because that is where wfm_writer_core is
 * already linked (with its keyword/cJSON/draw deps).
 */
#include "wfm_writer/wfm_writer_core.h" /* wfm_blue_write_hcb */

#include <stddef.h>
#include <stdio.h>

int
write_blue_header (const char *path, int sample_type, int endian, double fs,
                   double fc, double data_start, size_t total_samples,
                   int detached)
{
  FILE *fp = fopen (path, "wb");
  if (!fp)
    return -1;
  int rc = wfm_blue_write_hcb (fp, sample_type, endian, fs, fc, data_start,
                               total_samples, detached);
  /* Success needs both a full write and a clean close (fclose is where a
     full-disk error finally surfaces); either failing is the nonzero status
     the binding's check_return raises on. */
  return (fclose (fp) == 0 && rc == 0) ? 0 : -1;
}
