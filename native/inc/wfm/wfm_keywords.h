/**
 * @file wfm_keywords.h
 * @brief BLUE extended-header keywords — the X-Midas binary tag/value codec.
 *
 * The extended header is arbitrary metadata attached to a BLUE file as a
 * packed sequence of tag/value pairs (Midas BLUE 1.1 §3.3.1, Table 26). One
 * codec serves both directions: `wfm_writer` encodes with it, `wfm_reader`
 * decodes with it, so the two can never disagree about the wire format.
 *
 * Each keyword is an 8-byte header, then the value, then the tag, then padding
 * to a multiple of eight bytes:
 *
 * ```text
 *   offset            field    bytes         note
 *   0                 lkey     4 (int_4)     TOTAL entry length, incl. padding
 *   4                 lext     2 (int_2)     NON-value length: 8 + ltag + pad
 *   6                 ltag     1 (int_1)     tag character count
 *   7                 type     1 (char)      element type (Table 6)
 *   8                 value    lkey - lext   in the HEADER's byte order
 *   8 + lkey - lext   tag      ltag          ASCII, no NUL
 *   lkey - pad        pad      pad           zero fill to an 8-byte multiple
 * ```
 *
 * Note `lext` counts the 8-byte header too, so the value length is
 * `lkey - lext` — not `lkey - 8 - ltag - pad` computed some other way. Readers
 * advance by `lkey`, which is what lets a keyword of an unrecognised type be
 * stepped over intact rather than aborting the parse (§3.3.1).
 *
 * Values are stored in the byte order the HCB declares (`head_rep`), so the
 * decoder swaps them to host order and the encoder swaps them back.
 *
 * @code
 * // Encode "F_C = 1.2345e9" into a buffer, then read it back.
 * double   fc = 1.2345e9;
 * uint8_t  buf[64];
 * size_t   n = wfm_kw_encode(buf, sizeof buf, "F_C", 'D', &fc, 1, 0);
 * wfm_keyword_t kw;
 * size_t   used;
 * wfm_kw_decode(buf, n, 0, &kw, &used);   // kw.tag = "F_C", kw.count = 1
 * double   got;
 * memcpy(&got, kw.value, sizeof got);     // 1.2345e9, host order
 * @endcode
 */
#ifndef DP_WFM_KEYWORDS_H
#define DP_WFM_KEYWORDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Longest tag the format can express: `ltag` is a single byte. */
#define WFM_KW_MAX_TAG 255

  /** One decoded keyword. `value` is `count * elem_size` bytes in HOST order;
   *  for type `'A'` it is `count` characters and is NOT NUL-terminated. */
  typedef struct
  {
    char     tag[WFM_KW_MAX_TAG + 1]; /**< NUL-terminated tag. */
    char     type;      /**< element type: B I L X F D A. */
    size_t   elem_size; /**< bytes per element (1 for 'A'). */
    size_t   count;     /**< element count (characters for 'A'). */
    uint8_t *value;     /**< value bytes, host order; owned by the holder. */
  } wfm_keyword_t;

  /**
   * @brief Bytes per element for a keyword type code, or 0 if the code cannot
   * appear in a keyword.
   *
   * Table 6's KW-legal set: `B` 1, `I` 2, `L` 4, `X` 8, `F` 4, `D` 8, `A` 1
   * (a variable-length string in keyword context — the eight-character
   * implication of `A` does not apply here). `T` is a deprecated alias for a
   * 32-bit integer and decodes as 4. `O` (offset byte), `P` (packed bits) and
   * `N` (4-bit) are explicitly not permitted in keywords; `S` is reserved.
   */
  size_t wfm_kw_elem_size (char type);

  /**
   * @brief Total encoded size (`lkey`) of a keyword, including padding.
   *
   * @param ltag   tag length in characters (1..WFM_KW_MAX_TAG).
   * @param vbytes value length in bytes.
   * @return the padded entry length, always a multiple of 8.
   */
  size_t wfm_kw_entry_size (size_t ltag, size_t vbytes);

  /**
   * @brief Encode one keyword into @p out.
   *
   * @param out   destination buffer.
   * @param cap   bytes available at @p out.
   * @param tag   NUL-terminated tag, 1..WFM_KW_MAX_TAG characters.
   * @param type  element type code (must be KW-legal, see wfm_kw_elem_size).
   * @param value @p count elements in HOST order (characters for `'A'`).
   * @param count element count; must be non-zero.
   * @param be    write the value big-endian (the HCB's `head_rep`).
   * @return bytes written, or 0 if the arguments are invalid or @p cap is too
   *         small (nothing is written in that case).
   */
  size_t wfm_kw_encode (uint8_t *out, size_t cap, const char *tag, char type,
                        const void *value, size_t count, int be);

  /**
   * @brief Decode the keyword at @p p, allocating its value.
   *
   * @param p        start of the entry.
   * @param avail    bytes remaining in the extended header from @p p.
   * @param be       the value's byte order (the HCB's `head_rep`).
   * @param out      filled in on success; `out->value` is malloc'd and must be
   *                 freed by the caller.
   * @param consumed always set to `lkey` when the entry header is intact, so
   *                 the caller can step to the next keyword even when this one
   *                 is skipped.
   * @retval 0  decoded; @p out is valid.
   * @retval 1  well-formed but unsupported type — @p out is untouched, step by
   *            @p consumed and carry on (§3.3.1's skip-don't-abort rule).
   * @retval -1 malformed: the entry does not fit in @p avail, or its internal
   *            lengths are inconsistent. @p consumed is not meaningful; stop.
   */
  int wfm_kw_decode (const uint8_t *p, size_t avail, int be,
                     wfm_keyword_t *out, size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_KEYWORDS_H */
