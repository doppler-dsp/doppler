/*
 * wfm_seq_parse.c — the one text spelling of a frame field. The grammar, and
 * why it is one function rather than one per face, live on the declaration in
 * wfm/wfm_seq_parse.h.
 */
#include "wfm/wfm_seq_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A key=value run, comma separated. Returns the value text for `key` and its
   length, or NULL. Values do not contain ',' or '=' by construction, so this
   needs no quoting rules and deliberately has none: a field spec is numbers
   and identifiers, and inventing an escape here would be a wire format. */
static const char *
kv_find (const char *args, const char *key, size_t *vlen)
{
  size_t klen = strlen (key);
  for (const char *p = args; p && *p;)
    {
      const char *eq    = strchr (p, '=');
      const char *comma = strchr (p, ',');
      if (!eq || (comma && eq > comma))
        return NULL; /* a bare token where key=value was required */
      if ((size_t)(eq - p) == klen && strncmp (p, key, klen) == 0)
        {
          const char *v = eq + 1;
          const char *e = strchr (v, ',');
          *vlen         = e ? (size_t)(e - v) : strlen (v);
          return v;
        }
      p = comma ? comma + 1 : NULL;
    }
  return NULL;
}

/* An unsigned integer, decimal or 0x-prefixed. Returns 0 and sets *ok on a
   value that is not entirely consumed — a trailing character is a typo, and
   accepting `reg=7x` as 7 is how a spec silently means something else. */
static uint64_t
kv_u64 (const char *args, const char *key, uint64_t dflt, int *ok)
{
  size_t      vlen = 0;
  const char *v    = kv_find (args, key, &vlen);
  if (!v || vlen == 0)
    return dflt;
  char buf[32];
  if (vlen >= sizeof buf)
    {
      *ok = 0;
      return 0;
    }
  memcpy (buf, v, vlen);
  buf[vlen]    = '\0';
  char    *end = NULL;
  uint64_t n   = strtoull (buf, &end, 0);
  if (end == buf || *end != '\0')
    *ok = 0;
  return n;
}

/* Reject any key the kind does not define. A misspelled `reg_bits=7` on a PN
   field would otherwise leave reg at 0 and produce an unbuildable descriptor
   whose error names the register, not the typo. */
static int
kv_only (const char *args, const char *const *allowed, size_t n_allowed)
{
  for (const char *p = args; p && *p;)
    {
      const char *eq = strchr (p, '=');
      if (!eq)
        return 0;
      size_t klen  = (size_t)(eq - p);
      int    found = 0;
      for (size_t i = 0; i < n_allowed && !found; i++)
        found = strlen (allowed[i]) == klen
                && strncmp (p, allowed[i], klen) == 0;
      if (!found)
        return 0;
      const char *comma = strchr (eq, ',');
      p                 = comma ? comma + 1 : NULL;
    }
  return 1;
}

/* A bare 0/1 string. Unlike the two parsers this replaces, a stray character
   is an ERROR rather than something to skip: wfm_json's string_to_bits()
   skipped it and wfmgen's parse_bit_string() refused it, so the same text
   meant different things on two faces. Refusing is the one that cannot
   silently shorten a sync word. */
static uint8_t *
literal_bits (const char *s, size_t len, size_t *n)
{
  uint8_t *b = (uint8_t *)malloc (len ? len : 1);
  if (!b)
    return NULL;
  for (size_t i = 0; i < len; i++)
    {
      if (s[i] != '0' && s[i] != '1')
        {
          free (b);
          return NULL;
        }
      b[i] = (uint8_t)(s[i] - '0');
    }
  *n = len;
  return b;
}

/* Hex digits, MSB-first, four bits each. */
static uint8_t *
literal_hex (const char *s, size_t len, size_t *n)
{
  uint8_t *b = (uint8_t *)malloc (len ? len * 4 : 1);
  if (!b)
    return NULL;
  for (size_t i = 0; i < len; i++)
    {
      char c = s[i];
      int  v = (c >= '0' && c <= '9')   ? c - '0'
               : (c >= 'a' && c <= 'f') ? c - 'a' + 10
               : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                        : -1;
      if (v < 0)
        {
          free (b);
          return NULL;
        }
      for (int k = 0; k < 4; k++)
        b[i * 4 + k] = (uint8_t)((v >> (3 - k)) & 1);
    }
  *n = len * 4;
  return b;
}

/* Every bit of a binary file, MSB-first per byte. */
static uint8_t *
literal_file (const char *path, size_t *n)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  if (fseek (f, 0, SEEK_END) != 0)
    {
      (void)fclose (f);
      return NULL;
    }
  long sz = ftell (f);
  rewind (f);
  if (sz <= 0)
    {
      (void)fclose (f);
      return NULL;
    }
  unsigned char *raw = (unsigned char *)malloc ((size_t)sz);
  if (!raw || fread (raw, 1, (size_t)sz, f) != (size_t)sz)
    {
      free (raw);
      (void)fclose (f);
      return NULL;
    }
  (void)fclose (f);

  uint8_t *b = (uint8_t *)malloc ((size_t)sz * 8u);
  if (!b)
    {
      free (raw);
      return NULL;
    }
  for (long i = 0; i < sz; i++)
    for (int k = 0; k < 8; k++)
      b[i * 8 + k] = (uint8_t)((raw[i] >> (7 - k)) & 1u);
  free (raw);
  *n = (size_t)sz * 8u;
  return b;
}

int
wfm_seq_parse (const char *spec, wfm_seq_t *out, uint8_t **owned,
               const char **err)
{
  const char *ignored = NULL;
  if (!err)
    err = &ignored;
  *err = NULL;
  if (!out || !owned)
    {
      *err = "no destination";
      return -1;
    }
  memset (out, 0, sizeof *out);
  *owned = NULL;
  if (!spec || !*spec)
    return 0; /* absent, which is a frame saying it carries no such field */

  /* A prefix is `kind:`, and only for the kinds named here — a bare 0/1
     string never contains ':', so the split is unambiguous without lookahead
     and a literal needs no prefix at all. */
  const char *colon = strchr (spec, ':');
  const char *args  = colon ? colon + 1 : NULL;
  size_t      klen  = colon ? (size_t)(colon - spec) : 0;
  int         ok    = 1;

#define KIND_IS(name)                                                         \
  (colon && klen == strlen (name) && strncmp (spec, name, klen) == 0)

  if (KIND_IS ("pn"))
    {
      static const char *const keys[]
          = { "len", "reg", "poly", "seed", "lfsr" };
      if (!kv_only (args, keys, sizeof keys / sizeof keys[0]))
        {
          *err = "pn: unknown key (want len, reg, poly, seed, lfsr)";
          return -1;
        }
      out->kind      = WFM_SEQ_PN;
      out->len       = (size_t)kv_u64 (args, "len", 0, &ok);
      out->reg_bits  = (uint32_t)kv_u64 (args, "reg", 0, &ok);
      out->poly      = kv_u64 (args, "poly", 0, &ok);
      out->seed      = kv_u64 (args, "seed", 0, &ok);
      size_t      lv = 0;
      const char *l  = kv_find (args, "lfsr", &lv);
      if (l)
        {
          if (lv == 6 && strncmp (l, "galois", 6) == 0)
            out->lfsr = 0;
          else if (lv == 9 && strncmp (l, "fibonacci", 9) == 0)
            out->lfsr = 1;
          else
            {
              *err = "pn: lfsr must be galois or fibonacci";
              return -1;
            }
        }
      if (!ok)
        {
          *err = "pn: a value is not a number";
          return -1;
        }
      if (out->len == 0 || out->reg_bits == 0 || out->reg_bits > 64)
        {
          *err = "pn: needs len > 0 and reg in 1..64";
          return -1;
        }
      return 0;
    }

  if (KIND_IS ("gold"))
    {
      static const char *const keys[]
          = { "len", "reg", "taps_a", "seed_a", "taps_b", "seed_b" };
      if (!kv_only (args, keys, sizeof keys / sizeof keys[0]))
        {
          *err = "gold: unknown key (want len, reg, taps_a, seed_a, taps_b, "
                 "seed_b)";
          return -1;
        }
      out->kind     = WFM_SEQ_GOLD;
      out->len      = (size_t)kv_u64 (args, "len", 0, &ok);
      out->reg_bits = (uint32_t)kv_u64 (args, "reg", 0, &ok);
      out->taps_a   = kv_u64 (args, "taps_a", 0, &ok);
      out->seed_a   = kv_u64 (args, "seed_a", 0, &ok);
      out->taps_b   = kv_u64 (args, "taps_b", 0, &ok);
      out->seed_b   = kv_u64 (args, "seed_b", 0, &ok);
      if (!ok)
        {
          *err = "gold: a value is not a number";
          return -1;
        }
      if (out->len == 0 || out->reg_bits == 0 || out->reg_bits > 64)
        {
          *err = "gold: needs len > 0 and reg in 1..64";
          return -1;
        }
      return 0;
    }

  if (KIND_IS ("dotted"))
    {
      static const char *const keys[] = { "len" };
      if (!kv_only (args, keys, 1))
        {
          *err = "dotted: unknown key (want len)";
          return -1;
        }
      out->kind = WFM_SEQ_DOTTED;
      out->len  = (size_t)kv_u64 (args, "len", 0, &ok);
      if (!ok || out->len == 0)
        {
          *err = "dotted: needs len > 0";
          return -1;
        }
      return 0;
    }

  /* ── the literal kinds ── */
  size_t   n = 0;
  uint8_t *b = NULL;
  if (KIND_IS ("hex"))
    {
      b = literal_hex (args, strlen (args), &n);
      if (!b)
        {
          *err = "hex: not a hex string";
          return -1;
        }
    }
  else if (KIND_IS ("file"))
    {
      b = literal_file (args, &n);
      if (!b)
        {
          *err = "file: cannot read, or is empty";
          return -1;
        }
    }
  else if (spec[0] == '0' && (spec[1] == 'x' || spec[1] == 'X'))
    {
      b = literal_hex (spec + 2, strlen (spec) - 2, &n);
      if (!b)
        {
          *err = "0x: not a hex string";
          return -1;
        }
    }
  else if (colon)
    {
      *err = "unknown kind (want pn, gold, dotted, hex, file, or 0/1 bits)";
      return -1;
    }
  else
    {
      b = literal_bits (spec, strlen (spec), &n);
      if (!b)
        {
          *err = "not a 0/1 string";
          return -1;
        }
    }
#undef KIND_IS

  out->kind = WFM_SEQ_LITERAL;
  out->bits = b;
  out->len  = n;
  *owned    = b;
  return 0;
}
