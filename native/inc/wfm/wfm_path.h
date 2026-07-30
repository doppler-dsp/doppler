/**
 * @file wfm_path.h
 * @brief Sibling-path construction shared by the wfm reader and writer.
 *
 * Several file types come in pairs — a BLUE detached capture is
 * `<base>.hdr` + `<base>.det`, a SigMF one is `<base>.sigmf-data` +
 * `<base>.sigmf-meta` — so both halves of the library have to derive one
 * member's name from the other's. Reader and writer MUST agree on that
 * derivation, or the writer emits a sidecar at a name the reader will not
 * look for; one definition here is what makes that impossible.
 */
#ifndef WFM_PATH_H
#define WFM_PATH_H

#include <stdio.h>
#include <string.h>

/**
 * @brief Swap @p path's final extension for @p ext.
 *
 * `"foo/cap.prm"` + `".det"` → `"foo/cap.det"`. A path whose basename carries
 * no dot simply gains the extension, so `"cap"` → `"cap.det"`.
 *
 * The dot must be in the BASENAME: a dotted directory (`"../cap"`,
 * `"v1.2/cap"`) is not an extension, and treating it as one would write the
 * sibling into the wrong place entirely.
 *
 * @param path source path.
 * @param ext  replacement extension, leading dot included.
 * @param out  destination buffer; always NUL-terminated, truncated if @p cap
 *             is too small.
 * @param cap  bytes available at @p out.
 */
static inline void
wfm_swap_ext (const char *path, const char *ext, char *out, size_t cap)
{
  const char *dot   = strrchr (path, '.');
  const char *slash = strrchr (path, '/');
  size_t      base  = (dot && (!slash || dot > slash)) ? (size_t)(dot - path)
                                                       : strlen (path);
  snprintf (out, cap, "%.*s%s", (int)base, path, ext);
}

#endif /* WFM_PATH_H */
