/*
 * write_blue_header.c — wfm module-level function.
 */
#include "wfm/wfm_core.h"
#include "wfm/wfm_writer.h" /* wfm_blue_write_hcb */

int
write_blue_header (const char *path, size_t total, int sample_type, int endian,
                   double fs, double fc, double data_start, int detached)
{
  FILE *fp = fopen (path, "wb");
  if (!fp)
    return -1;
  int rc = wfm_blue_write_hcb (fp, sample_type, endian, fs, fc, data_start,
                                total, detached);
  fclose (fp);
  return rc;
}
