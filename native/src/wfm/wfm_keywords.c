/*
 * wfm_keywords.c — BLUE extended-header keyword codec (Midas BLUE 1.1 §3.3.1).
 *
 * One implementation shared by wfm_writer (encode) and wfm_reader (decode);
 * the two directions are each other's exact inverse by construction.
 */
#include "wfm/wfm_keywords.h"

#include <stdlib.h>
#include <string.h>

/* Copy sz bytes, reversing when the file is big-endian. Same convention as the
   reader's swab_copy / the writer's put: host order on one side, file order on
   the other, and the direction is symmetric so one helper serves both. */
static void
swab (void *dst, const void *src, size_t sz, int be)
{
  uint8_t       *d = dst;
  const uint8_t *s = src;
  for (size_t k = 0; k < sz; k++)
    d[k] = be ? s[sz - 1 - k] : s[k];
}

size_t
wfm_kw_elem_size (char type)
{
  switch (type)
    {
    case 'B':
      return 1; /* 8-bit integer */
    case 'I':
      return 2; /* 16-bit integer */
    case 'L':
      return 4; /* 32-bit integer */
    case 'X':
      return 8; /* 64-bit integer */
    case 'F':
      return 4; /* 32-bit float */
    case 'D':
      return 8; /* 64-bit float */
    case 'A':
      return 1; /* variable-length ASCII (NOT the 8-char DATA meaning) */
    case 'T':
      return 4; /* deprecated pre-4.9.0 alias for a 32-bit integer */
    default:
      return 0; /* O/P/N are illegal in keywords; S is reserved */
    }
}

size_t
wfm_kw_entry_size (size_t ltag, size_t vbytes)
{
  size_t n = 8 + vbytes + ltag; /* header + value + tag, before padding */
  return n + ((8 - (n % 8)) % 8);
}

size_t
wfm_kw_encode (uint8_t *out, size_t cap, const char *tag, char type,
               const void *value, size_t count, int be)
{
  size_t esz = wfm_kw_elem_size (type);
  if (!out || !tag || !value || esz == 0 || count == 0)
    return 0;
  size_t ltag = strlen (tag);
  if (ltag == 0 || ltag > WFM_KW_MAX_TAG)
    return 0;
  size_t vbytes = count * esz;
  size_t lkey   = wfm_kw_entry_size (ltag, vbytes);
  if (lkey > cap || lkey > INT32_MAX)
    return 0;
  /* lext is the NON-value length: the 8-byte header, the tag, and the pad. */
  size_t lext = lkey - vbytes;

  memset (out, 0, lkey); /* zero-fills the padding for free */
  int32_t k32 = (int32_t)lkey;
  int16_t x16 = (int16_t)lext;
  swab (out + 0, &k32, 4, be);
  swab (out + 4, &x16, 2, be);
  out[6] = (uint8_t)ltag;
  out[7] = (uint8_t)type;

  /* Each ELEMENT is byte-swapped individually — the value is an array, not one
     wide integer, so reversing the whole region would corrupt every element
     past the first. 'A' is a byte string and needs no swap either way. */
  const uint8_t *src = (const uint8_t *)value;
  for (size_t i = 0; i < count; i++)
    swab (out + 8 + i * esz, src + i * esz, esz, be);
  memcpy (out + 8 + vbytes, tag, ltag);
  return lkey;
}

int
wfm_kw_decode (const uint8_t *p, size_t avail, int be, wfm_keyword_t *out,
               size_t *consumed)
{
  if (avail < 8) /* not even a keyword header left */
    return -1;
  int32_t k32;
  int16_t x16;
  swab (&k32, p + 0, 4, be);
  swab (&x16, p + 4, 2, be);
  size_t ltag = p[6];
  char   type = (char)p[7];

  /* Everything below is arithmetic on numbers a foreign file chose, so each
     one is bounded before it is used as a length or an offset. A zero/negative
     lkey would also spin the caller's walk forever. */
  if (k32 < 8 || x16 < 8 || (size_t)k32 > avail || (size_t)x16 > (size_t)k32)
    return -1;
  size_t lkey   = (size_t)k32;
  size_t vbytes = lkey - (size_t)x16;
  if (8 + vbytes + ltag > lkey) /* value + tag must fit inside the entry */
    return -1;
  *consumed = lkey;

  size_t esz = wfm_kw_elem_size (type);
  if (esz == 0 || vbytes == 0 || (vbytes % esz) != 0)
    return 1; /* intact but undecodable — skip it, don't fail the file */

  size_t count = vbytes / esz;
  out->value   = (uint8_t *)malloc (vbytes);
  if (!out->value)
    return -1;
  const uint8_t *src = p + 8;
  for (size_t i = 0; i < count; i++)
    swab (out->value + i * esz, src + i * esz, esz, be);
  memcpy (out->tag, p + 8 + vbytes, ltag);
  out->tag[ltag] = '\0';
  out->type      = type;
  out->elem_size = esz;
  out->count     = count;
  return 0;
}
