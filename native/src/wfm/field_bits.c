/*
 * field_bits.c — wfm module-level function.
 *
 * The Field text form's door into Python, and nothing else: the grammar is
 * read once, by dp_wfm_field_parse, and rendered once, by
 * dp_wfm_field_render, both inside dp_wfm_field_bits. The binding sizes `out`
 * with the same call and NULL, so the capacity here is that size.
 */
#include "doppler/wfm/wfm_core.h"

size_t
dp_field_bits (const char *spec, uint8_t *out, const char **why)
{
  /* 0 on refusal from either call; the binding raises on 0 (check_return),
   * with *why's sentence when the parser gave one (just-makeit#1706). */
  return dp_wfm_field_bits (spec, out, dp_wfm_field_bits (spec, NULL, 0, NULL),
                            why);
}
