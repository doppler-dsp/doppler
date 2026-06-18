/*
 * wfm_sample.h — shared interleaved-I/Q wire-format converter.
 *
 * One bit-faithful decoder for the five wfm sample types (cf32, cf64, ci32,
 * ci16, ci8), used by both the container-aware reader (wfm_reader.c) and the
 * headerless IqFile reader (iq_file_core.c). The integer rescale divides by the
 * writer's full-scale (2^31-1 / 32767 / 127), so a wfmgen capture decodes back
 * to the unit-scale samples it was quantised from.
 */
#ifndef WFM_SAMPLE_H
#define WFM_SAMPLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Decode one interleaved I/Q pair at *p from the wire type into unit-scale
     floats. sample_type is 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8; endian is 0
     little / 1 big (the on-disk byte order — the host is assumed little). */
  void wfm_convert_pair (const uint8_t *p, int sample_type, int endian,
                         float *re, float *im);

  /* Bytes per complex sample for the wire type: cf32 8, cf64 16, ci32 8,
     ci16 4, ci8 2. Returns 0 for an out-of-range sample_type. */
  size_t wfm_bytes_per_sample (int sample_type);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SAMPLE_H */
