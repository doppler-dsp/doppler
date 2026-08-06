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

/**
 * @brief Name the `.sigmf-meta` sidecar belonging to the capture at @p path.
 *
 * Two derivations, because two different things are being named:
 *
 * - `"cap.sigmf-data"` → `"cap.sigmf-meta"` (SWAP). Not a choice: SigMF
 *   defines a capture as the `<base>.sigmf-data` + `<base>.sigmf-meta` pair,
 *   and conformant tools find the second half by exactly that name.
 * - `"cap.raw"` → `"cap.raw.sigmf-meta"` (APPEND). Anything else is not a
 *   SigMF capture, so no external tool looks for its metadata under any name,
 *   which leaves the derivation free — and a free choice should be the one
 *   that cannot collide. Swapping would give `cap.raw` and a genuine
 *   `cap.sigmf-data` in one directory the SAME sidecar name, so writing one
 *   capture would silently overwrite the other's metadata. Appending keeps
 *   the sidecar 1:1 with the file it describes, which is also what makes it
 *   safe to read back by exact name: wfm_reader_create deliberately does NOT
 *   sniff for `<base>.sigmf-meta` beside an arbitrary file, because a shared
 *   base name hijacked two unrelated files the first time that was tried.
 *
 * @param path capture (data) path.
 * @param out  destination buffer; always NUL-terminated, truncated if @p cap
 *             is too small.
 * @param cap  bytes available at @p out.
 */
static inline void
wfm_meta_path (const char *path, char *out, size_t cap)
{
  size_t n = strlen (path);
  if (n >= 11 && strcmp (path + n - 11, ".sigmf-data") == 0)
    wfm_swap_ext (path, ".sigmf-meta", out, cap);
  else
    snprintf (out, cap, "%s.sigmf-meta", path);
}

#endif /* WFM_PATH_H */
