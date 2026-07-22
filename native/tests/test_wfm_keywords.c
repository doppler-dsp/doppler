/*
 * test_wfm_keywords.c — the BLUE extended-header keyword codec against the
 * spec's own byte layout (Midas BLUE 1.1 §3.3.1, Table 26).
 *
 * Round-tripping our own encoder proves self-consistency, not correctness: an
 * encoder and decoder that share a wrong idea of `lext` agree perfectly and
 * still cannot read a real X-Midas file. So the first test asserts the exact
 * bytes the spec requires, field by field, and only then do the round-trips
 * ride on top.
 */
#include "wfm/wfm_keywords.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, m)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", m);                                  \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

static int32_t
rd32 (const uint8_t *p)
{
  int32_t v;
  memcpy (&v, p, 4);
  return v;
}

static int16_t
rd16 (const uint8_t *p)
{
  int16_t v;
  memcpy (&v, p, 2);
  return v;
}

/* Table 26, byte for byte, little-endian:
     0  lkey  int_4  total entry length INCLUDING padding
     4  lext  int_2  NON-value length: 8 (header) + ltag + padding
     6  ltag  int_1  tag characters
     7  type  char   element type
     8  value lkey - lext bytes, header byte order
        tag   ltag characters, immediately after the value, no padding between
        pad   zero fill to a multiple of 8
   The `lext` semantic is the trap: it counts the 8-byte header too, so the
   value length is lkey - lext and NOT lkey - 8 - ltag. */
static int
test_exact_layout (void)
{
  uint8_t buf[64];
  double  fc = 1.2345e9;

  /* 8 header + 8 value + 3 tag = 19 -> padded to 24 */
  size_t n = wfm_kw_encode (buf, sizeof buf, "F_C", 'D', &fc, 1, 0);
  CHECK (n == 24, "lkey is padded to a multiple of 8");
  CHECK (rd32 (buf + 0) == 24, "lkey field");
  CHECK (rd16 (buf + 4) == 16, "lext = 8 header + 3 tag + 5 pad");
  CHECK (buf[6] == 3, "ltag");
  CHECK (buf[7] == 'D', "type");
  double v;
  memcpy (&v, buf + 8, 8);
  CHECK (v == fc, "value sits at offset 8, host order on a LE file");
  CHECK (memcmp (buf + 16, "F_C", 3) == 0, "tag immediately follows value");
  CHECK (buf[19] == 0 && buf[23] == 0, "padding is zero-filled");
  /* the derived length the spec states: value bytes == lkey - lext */
  CHECK ((size_t)(rd32 (buf) - rd16 (buf + 4)) == sizeof (double),
         "value length is lkey - lext");

  /* An entry that needs no padding: 8 + 8 value + 8 tag = 24 exactly. */
  n = wfm_kw_encode (buf, sizeof buf, "TAGTAGTA", 'D', &fc, 1, 0);
  CHECK (n == 24, "already a multiple of 8 -> no padding added");
  CHECK (rd16 (buf + 4) == 16, "lext = 8 + 8 tag + 0 pad");

  /* 'A' in a keyword is a variable-length string, NOT the 8-char DATA form. */
  const char *s = "10 dB pad";
  n = wfm_kw_encode (buf, sizeof buf, "COMMENT", 'A', s, strlen (s), 0);
  CHECK (n == 24, "8 + 9 string + 7 tag = 24");
  CHECK (buf[7] == 'A' && rd16 (buf + 4) == 15, "A: lext = 8 + 7 tag + 0 pad");
  CHECK (memcmp (buf + 8, s, 9) == 0, "string value is not NUL-padded");
  return 0;
}

/* Every KW-legal type survives a round-trip, in both byte orders, as arrays
   as well as scalars. Values are per-ELEMENT byte swapped: reversing the whole
   value region instead would round-trip a scalar perfectly and silently
   reverse the element order of every array. */
static int
test_roundtrip_types (void)
{
  static const struct
  {
    char   type;
    size_t esz;
  } T[] = { { 'B', 1 }, { 'I', 2 }, { 'L', 4 }, { 'X', 8 },
            { 'F', 4 }, { 'D', 8 }, { 'A', 1 } };
  uint8_t src[64], buf[256];
  for (size_t i = 0; i < sizeof src; i++)
    src[i] = (uint8_t)(i * 7 + 1); /* distinguishable per byte */

  for (size_t t = 0; t < sizeof T / sizeof *T; t++)
    for (int be = 0; be <= 1; be++)
      for (size_t count = 1; count <= 5; count++)
        {
          size_t vb = count * T[t].esz;
          size_t n  = wfm_kw_encode (buf, sizeof buf, "TAG", T[t].type, src,
                                     count, be);
          CHECK (n > 0 && n % 8 == 0, "encoded, 8-aligned");
          wfm_keyword_t kw;
          size_t        used = 0;
          CHECK (wfm_kw_decode (buf, n, be, &kw, &used) == 0, "decodes");
          CHECK (used == n, "consumed the whole entry");
          CHECK (kw.type == T[t].type, "type survives");
          CHECK (kw.elem_size == T[t].esz, "element size");
          CHECK (kw.count == count, "element count");
          CHECK (strcmp (kw.tag, "TAG") == 0, "tag survives");
          CHECK (memcmp (kw.value, src, vb) == 0, "value bytes survive");
          free (kw.value);
        }
  return 0;
}

/* A big-endian entry must differ on the wire from its little-endian twin --
   otherwise the swap is a no-op and only same-endian files would ever work. */
static int
test_endianness_is_real (void)
{
  uint8_t le[32], be[32];
  int32_t v = 0x01020304;
  size_t  a = wfm_kw_encode (le, sizeof le, "N", 'L', &v, 1, 0);
  size_t  b = wfm_kw_encode (be, sizeof be, "N", 'L', &v, 1, 1);
  CHECK (a == b && a > 0, "same length either way");
  CHECK (memcmp (le, be, a) != 0, "the encodings actually differ");
  CHECK (le[8] == 0x04 && be[8] == 0x01, "value bytes are reversed");
  CHECK (le[0] == 16 && be[3] == 16, "the LENGTH fields are swapped too");

  /* an array in BE: each element reversed independently, order preserved */
  int16_t arr[3] = { 0x0102, 0x0304, 0x0506 };
  size_t  n      = wfm_kw_encode (be, sizeof be, "A", 'I', arr, 3, 1);
  CHECK (be[8] == 0x01 && be[9] == 0x02, "elem 0 reversed in place");
  CHECK (be[10] == 0x03 && be[12] == 0x05, "element ORDER is preserved");
  wfm_keyword_t kw;
  size_t        used;
  CHECK (wfm_kw_decode (be, n, 1, &kw, &used) == 0, "BE array decodes");
  CHECK (memcmp (kw.value, arr, sizeof arr) == 0, "array survives BE");
  free (kw.value);
  return 0;
}

/* §3.3.1: "any keyword using a type not recognized by a BLUE file processor
   can at least be skipped over or passed through uninterpreted." An unknown
   type must therefore consume lkey and report skip -- never abort the walk,
   which would drop every keyword after it. */
static int
test_unknown_type_is_skipped (void)
{
  uint8_t       buf[64];
  wfm_keyword_t kw;
  size_t        used;

  int32_t v = 42;
  size_t  n = wfm_kw_encode (buf, sizeof buf, "N", 'L', &v, 1, 0);
  /* forge the type into codes that are illegal or reserved in keywords */
  static const char BAD[] = { 'O', 'P', 'N', 'S', 'Z', '?' };
  for (size_t i = 0; i < sizeof BAD; i++)
    {
      buf[7] = (uint8_t)BAD[i];
      used   = 0;
      CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == 1, "reports skip");
      CHECK (used == n, "still consumes the full entry so the walk continues");
    }

  /* 'T' is the deprecated pre-4.9.0 spelling of a 32-bit integer: decodable */
  buf[7] = 'T';
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == 0, "T decodes");
  CHECK (kw.elem_size == 4 && kw.count == 1, "T is a 32-bit integer");
  free (kw.value);
  return 0;
}

/* A foreign file's length fields are attacker-grade input: every one is used
   as an offset or a size. Malformed entries must be rejected, and above all
   must not loop forever (lkey == 0) or read past the region. */
static int
test_malformed_is_rejected (void)
{
  uint8_t       buf[64];
  wfm_keyword_t kw;
  size_t        used;
  double        d = 1.0;
  size_t        n = wfm_kw_encode (buf, sizeof buf, "F_C", 'D', &d, 1, 0);

  CHECK (wfm_kw_decode (buf, 7, 0, &kw, &used) == -1, "short of a header");
  CHECK (wfm_kw_decode (buf, n - 1, 0, &kw, &used) == -1,
         "entry overruns the region");

  int32_t z = 0;
  memcpy (buf, &z, 4); /* lkey = 0 would spin a walk forever */
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == -1, "lkey = 0 rejected");
  z = -8;
  memcpy (buf, &z, 4);
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == -1, "negative lkey");

  n           = wfm_kw_encode (buf, sizeof buf, "F_C", 'D', &d, 1, 0);
  int16_t bad = 4; /* lext < 8: smaller than the header itself */
  memcpy (buf + 4, &bad, 2);
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == -1, "lext < 8 rejected");

  n   = wfm_kw_encode (buf, sizeof buf, "F_C", 'D', &d, 1, 0);
  bad = 32; /* lext > lkey: value length would go negative */
  memcpy (buf + 4, &bad, 2);
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == -1, "lext > lkey rejected");

  n      = wfm_kw_encode (buf, sizeof buf, "F_C", 'D', &d, 1, 0);
  buf[6] = 200; /* ltag that cannot fit alongside the value */
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == -1, "oversized ltag");

  /* a value length that is not a whole number of elements is undecodable but
     structurally sound -> skip, not a hard error */
  n      = wfm_kw_encode (buf, sizeof buf, "F_C", 'B', "abc", 3, 0);
  buf[7] = 'D'; /* claim 8-byte elements over a 3-byte value */
  CHECK (wfm_kw_decode (buf, n, 0, &kw, &used) == 1,
         "ragged value is skipped");
  return 0;
}

static int
test_encode_guards (void)
{
  uint8_t buf[64];
  double  d = 1.0;
  CHECK (wfm_kw_encode (buf, sizeof buf, "", 'D', &d, 1, 0) == 0,
         "empty tag refused");
  CHECK (wfm_kw_encode (buf, sizeof buf, "T", 'O', &d, 1, 0) == 0,
         "offset-byte type refused (illegal in keywords)");
  CHECK (wfm_kw_encode (buf, sizeof buf, "T", 'D', &d, 0, 0) == 0,
         "zero count refused");
  CHECK (wfm_kw_encode (buf, 16, "T", 'D', &d, 4, 0) == 0,
         "too small a buffer refused, nothing written");
  return 0;
}

int
main (void)
{
  if (test_exact_layout ())
    return 1;
  if (test_roundtrip_types ())
    return 1;
  if (test_endianness_is_real ())
    return 1;
  if (test_unknown_type_is_skipped ())
    return 1;
  if (test_malformed_is_rejected ())
    return 1;
  if (test_encode_guards ())
    return 1;
  printf ("test_wfm_keywords: all passed\n");
  return 0;
}
