/**
 * @file wfm_seq_parse.h
 * @brief One text spelling of a frame field, shared by every face.
 *
 * A frame field is either a **selection** — a generated sequence named by a
 * handful of numbers — or a **user-defined pattern**. `wfm_seq_t` already says
 * exactly that (`wfm/wfm_frame.h`), and every face then threw the distinction
 * away: the CLI, the JSON scene and the composer each took literal bits only,
 * so three of the four kinds the descriptor supports could not be spelled by
 * any caller.
 *
 * This is the grammar that gives them one spelling, and it is deliberately
 * ONE function rather than a per-face parser, because there were already two
 * that disagreed: `wfmgen`'s `parse_bit_string()` rejected a bad character
 * while `wfm_json`'s `string_to_bits()` silently skipped it, so a scene file
 * accepted input the command line refused. Both are replaced by this.
 *
 * ## The grammar
 *
 * ```text
 * pn:len=N,reg=B[,poly=P][,seed=S][,lfsr=galois|fibonacci]
 * gold:len=N,reg=B[,taps_a=A][,seed_a=S][,taps_b=B2][,seed_b=S2]
 * dotted:len=N
 * hex:AA55        (or 0xAA55)   — a literal, MSB-first per digit
 * file:PATH                     — a literal, MSB-first per byte
 * 1011010…                      — a literal, the bare 0/1 form
 * ```
 *
 * Numbers accept `0x` prefixes. The bare form is last and unprefixed, which is
 * what makes this a strict superset of what every face already accepted: every
 * `--sync 1111100110101` and every `"sync": "1011…"` in a committed scene
 * parses unchanged, so nothing migrates.
 *
 * ## Growing it
 *
 * A new **kind** is one row in the table here and reaches every field at once.
 * A new **field** is one flag, and reaches every kind at once. That is the
 * property the per-field-per-kind flags did not have: `--sync-pn`,
 * `--sync-gold`, `--payload-pn`… is fields x kinds, and a header field would
 * have made it seven flags before anyone asked for a Zadoff-Chu.
 *
 * @code
 * wfm_seq_t   s = { 0 };
 * uint8_t    *owned = NULL;
 * const char *err = NULL;
 * if (wfm_seq_parse ("pn:len=127,reg=7", &s, &owned, &err) != 0)
 *   fprintf (stderr, "bad field: %s\n", err);
 * // ... use s ...
 * free (owned);
 * @endcode
 *
 * @see docs/design/rx-test.md section 7
 */
#ifndef WFM_SEQ_PARSE_H
#define WFM_SEQ_PARSE_H

#include "wfm/wfm_frame.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Parse one field spec into a @ref wfm_seq_t.
   *
   * @param spec   The spec text; NULL or empty yields an ABSENT field
   *               (`out->len == 0`), which is not an error — that is how a
   *               frame says it carries no preamble.
   * @param out    Filled on success; zeroed first, so a partial parse never
   *               leaves a half-populated descriptor.
   * @param owned  Receives the literal array for a literal kind, NULL for a
   *               generated one. The caller frees it; `out->bits` points into
   *               it, so it must outlive @p out.
   * @param err    On failure, receives a static message naming what was
   *               wrong. May be NULL.
   * @return 0 on success, -1 on a malformed spec (nothing is allocated).
   */
  int wfm_seq_parse (const char *spec, wfm_seq_t *out, uint8_t **owned,
                     const char **err);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SEQ_PARSE_H */
