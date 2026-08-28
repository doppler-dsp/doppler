/*
 * wfm_reader.c — input file types for generated IQ (the dual of wfm_writer).
 *
 * Auto-detects raw / CSV / BLUE type-1000 (attached or detached) / SigMF and
 * yields unit-scale float complex samples. File-type parsing and the wire→unit
 * conversion live here, in C; the Python `Reader` is a thin binding.
 */
#include "wfm_reader/wfm_reader_core.h"

#include "dp_interrupt.h"
#include "wfm/wfm_time.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "wfm/wfm_keywords.h"
#include "wfm/wfm_path.h"

/* per sample_type (0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8) — mirror wfm_writer
 */
static const size_t ELEM[5]  = { 4, 8, 4, 2, 1 }; /* bytes per component */
static const double SCALE[5] = { 0, 0, 2147483647.0, 32767.0, 127.0 };
static const char   FMTCH[5]
    = { 'F', 'D', 'L', 'I', 'B' }; /* BLUE format char */

struct wfm_reader_state
{
  FILE          *fp;
  int            file_type;   /* wfm_filetype_t */
  int            sample_type; /* 0..4 */
  int            mode;        /* wfm_mode_t: 0 complex, 1 scalar */
  int            endian;      /* 0 le, 1 be */
  double         fs, fc;      /* Hz; 0 if unknown */
  size_t         num_samples; /* total complex samples; 0 if unknown */
  uint8_t       *scratch;     /* read buffer for binary file types */
  size_t         scratch_cap;
  wfm_keyword_t *kw; /* decoded extended-header keywords (BLUE only) */
  size_t         nkw;
  /* Every field of the 512-byte HCB, decoded and carried as keyword entries
     so the existing tag/value codec serves the header dict too -- no second
     marshaller, and `header` and `keywords` can never disagree about how a
     double or an ASCII field is turned into a Python object. */
  wfm_keyword_t *hdr;
  size_t         nhdr;
  /* BLUE declares its payload length, and anything after it (an extended
     header, X-Midas slack) is NOT samples. `bounded` says the limit is known;
     `remaining` counts down the samples still owed. Raw/CSV/SigMF run to EOF,
     which for them is the same thing. */
  int    bounded;
  size_t remaining;
  long   data_off;    /* byte offset of the first sample, for reset() */
  size_t data_bytes;  /* declared payload length in BYTES; 0 = run to EOF */
  int    fc_source;   /* wfm_fc_source_t: which tag `fc` came from */
  size_t trailing;    /* payload bytes past the last whole sample */
  int    csv_counted; /* CSV num_samples has been scanned for (lazy) */
  int    fs_source;   /* wfm_fs_source_t: which metadata `fs` came from */
  /* Following a capture that is still being written. `ending` is why the
     last read_follow() came back empty -- a wfm_follow_end_t index, which
     is what the `ending` property decodes to a string, corresponding 1:1 to
     DP_OK / DP_ERR_EOF / _TIMEOUT / _INTERRUPTED. `follow_bounded` latches
     once the writer patches data_size -- the 0 -> N transition is the writer
     having finished, OBSERVED, rather than the promise the header opened
     with. See docs/design/end-of-capture.md. */
  uint32_t follow_timeout_ms; /* 0 = wait forever */
  uint32_t follow_grace_ms;   /* 0 = wait forever, after a stop is asked */
  int      ending;
  int      follow_bounded;
  /* How the follow loop learns a stop was requested. A capture reader has no
     business depending on the process interrupt primitive -- that is the
     caller's policy, and hard-wiring it would put dp_interrupt.c on the link
     line of every consumer of wfm_reader_core. So the predicate is injected;
     doppler passes dp_interrupted, a test passes its own. NULL never stops. */
  int (*stop_fn) (void);
  double t0_unix_sec; /* capture start, UNIX seconds; 0 if unknown */
  int    t0_source;   /* wfm_t0_source_t: where `t0` came from */
};

/* Copy sz bytes of *src into *dst, reversing on big-endian so the host (LE on
   both wheel targets) sees a native value. Inverse of wfm_writer's put(). */
static void
swab_copy (void *dst, const uint8_t *src, size_t sz, int be)
{
  uint8_t *d = dst;
  for (size_t k = 0; k < sz; k++)
    d[k] = be ? src[sz - 1 - k] : src[k];
}

static int
ends_with (const char *s, const char *suffix)
{
  size_t ls = strlen (s), lx = strlen (suffix);
  return ls >= lx && strcmp (s + ls - lx, suffix) == 0;
}

/* Decode ONE wire component (an I, or a Q, or a scalar-mode real sample) into
   a unit-scale float. Per-component rather than per-pair because BLUE 'S'
   files carry one component per sample; complex is just two calls. */
static float
convert_elem (const uint8_t *p, int stype, int be)
{
  switch (stype)
    {
    case 0:
      {
        float a;
        swab_copy (&a, p, 4, be);
        return a;
      }
    case 1:
      {
        double a;
        swab_copy (&a, p, 8, be);
        return (float)a;
      }
    case 2:
      {
        int32_t a;
        swab_copy (&a, p, 4, be);
        return (float)(a / SCALE[2]);
      }
    case 3:
      {
        int16_t a;
        swab_copy (&a, p, 2, be);
        return (float)(a / SCALE[3]);
      }
    default:
      return (float)((int8_t)p[0] / SCALE[4]);
    }
}

/* Wire components per emitted sample: 2 for interleaved I/Q, 1 for scalar. */
static size_t
comps (int mode)
{
  return (mode == WFM_MODE_SCALAR) ? 1u : 2u;
}

/* Everything wfm_reader needs out of a 512-byte BLUE type-1000 HCB. */
typedef struct
{
  int    stype, mode, endian, detached;
  double fs;
  double timecode;   /* BLUE header byte 56: J1950 seconds, 0 = unset */
  double data_start; /* bytes from the start of the DATA file */
  size_t nsamples;
  size_t data_bytes; /* the HCB's data_size, unrounded */
  long   ext_off;    /* bytes from the start of the HEADER file; 0 = none */
  size_t ext_size;   /* extended-header length in bytes; 0 = none */
} blue_hcb_t;

/* Parse a 512-byte BLUE type-1000 HCB. Returns 0 on success, -1 if this is not
   a BLUE header — the "BLUE" magic at byte 0 is the gate, so a file that is
   not BLUE (a stray .hdr, a raw file that happens to be .det) is rejected,
   never mis-read. Every BLUE path (attached + detached) goes through here.

   The two-character `format` field (bytes 52..53) is [mode][type]: the mode
   says how many components make a sample, the type says how each component is
   stored. Both halves are validated — an unsupported mode (V/Q/M/T/…, three or
   more components per sample) is REJECTED rather than assumed to be
   interleaved I/Q, which would silently return garbage at the wrong stride. */
/* What the reader REPORTS as its sample type: one of the ten wavegen-order
   names, complex five then scalar five.

   `r->sample_type` is the ELEMENT index alone -- 0..4 -- because ELEM[],
   SCALE[] and convert_elem() are all indexed by it, and `r->mode` carries the
   component count separately. That split is right internally and wrong at the
   surface: a real float capture reporting `sample_type == "cf32"` alongside
   `mode == "scalar"` says two contradictory things, and the combined name is
   also exactly what wfm_reader_create() accepts as a hint, so what you pass
   for a headerless file is what you get back. */
static int
reported_stype (const wfm_reader_state_t *r)
{
  return r->sample_type + (r->mode == WFM_MODE_SCALAR ? 5 : 0);
}

static int
parse_blue_hcb (const uint8_t h[512], blue_hcb_t *o)
{
  if (memcmp (h, "BLUE", 4) != 0) /* validate the magic before trusting it */
    return -1;
  int be = (memcmp (h + 4, "IEEE", 4) == 0); /* EEEI = le, IEEE = be */

  int md; /* format mode (byte 52): components per sample */
  if (h[52] == 'C')
    md = WFM_MODE_COMPLEX;
  else if (h[52] == 'S')
    md = WFM_MODE_SCALAR;
  else
    return -1;

  char fmt = (char)h[53]; /* format type (byte 53): per-component storage */
  int  st  = -1;
  for (int i = 0; i < 5; i++)
    if (FMTCH[i] == fmt)
      st = i;
  if (st < 0)
    return -1;
  double  ds, dsz, xdelta;
  int32_t det, xstart, xsize;
  swab_copy (&det, h + 12, 4, be);
  swab_copy (&xstart, h + 24, 4, be);
  swab_copy (&xsize, h + 28, 4, be);
  swab_copy (&ds, h + 32, 8, be);
  swab_copy (&dsz, h + 40, 8, be);
  swab_copy (&xdelta, h + 264, 8, be);
  swab_copy (&o->timecode, h + 56, 8, be);
  o->stype      = st;
  o->mode       = md;
  o->endian     = be;
  o->fs         = (xdelta != 0.0) ? 1.0 / xdelta : 0.0;
  o->data_start = ds;
  /* data_size is BYTES; a scalar file packs one component per sample, so
     dividing by the complex size would under-count by 2x. Both forms are
     kept: the sample count is what callers want, the unrounded byte count is
     what shows a payload that does not divide into whole samples. */
  o->data_bytes = (dsz > 0.0) ? (size_t)dsz : 0u;
  o->nsamples   = (size_t)(dsz / (double)(comps (md) * ELEM[st]));
  o->detached   = (int)det;
  /* ext_start counts 512-byte BLOCKS from the start of the file; ext_size is
     in BYTES (§3.1.1.7/.8). Both live in the HEADER file, which for a detached
     capture is not the file the samples come from. */
  o->ext_off  = (xstart > 0) ? (long)xstart * 512L : 0L;
  o->ext_size = (xsize > 0) ? (size_t)xsize : 0u;
  return 0;
}

/* Append a keyword to r->kw, taking a copy of the value. Shared by the two
   sources -- the HCB's own keyword area and the extended header -- so a
   caller cannot tell which block a key arrived from. */
static void
kw_append (wfm_reader_state_t *r, const char *tag, char type, size_t count,
           const uint8_t *value)
{
  size_t esz = wfm_kw_elem_size (type);
  if (esz == 0 || count == 0)
    return;
  wfm_keyword_t *p
      = (wfm_keyword_t *)realloc (r->kw, (r->nkw + 1) * sizeof *p);
  if (!p)
    return;
  r->kw            = p;
  wfm_keyword_t *k = &r->kw[r->nkw];
  k->value         = (uint8_t *)malloc (count * esz);
  if (!k->value)
    return;
  snprintf (k->tag, sizeof k->tag, "%s", tag);
  k->type      = type;
  k->elem_size = esz;
  k->count     = count;
  memcpy (k->value, value, count * esz);
  r->nkw++;
}

/* Append one decoded HCB field. `src` is the raw header bytes; numeric types
   are swapped to host order, ASCII is copied verbatim. Best-effort: a failed
   allocation drops that one field rather than the whole header. */
static void
hdr_add (wfm_reader_state_t *r, const char *tag, char type, size_t count,
         const uint8_t *src, int be)
{
  size_t esz = wfm_kw_elem_size (type);
  if (esz == 0 || count == 0)
    return;
  wfm_keyword_t *p
      = (wfm_keyword_t *)realloc (r->hdr, (r->nhdr + 1) * sizeof *p);
  if (!p)
    return;
  r->hdr             = p;
  wfm_keyword_t *k   = &r->hdr[r->nhdr];
  size_t         nby = count * esz;
  k->value           = (uint8_t *)malloc (nby);
  if (!k->value)
    return;
  snprintf (k->tag, sizeof k->tag, "%s", tag);
  k->type      = type;
  k->elem_size = esz;
  k->count     = count;
  if (type == 'A')
    memcpy (k->value, src, nby); /* ASCII is not byte-order dependent */
  else
    for (size_t e = 0; e < count; e++)
      swab_copy (k->value + e * esz, src + e * esz, esz, be);
  r->nhdr++;
}

/* Decode the whole 512-byte HCB into r->hdr under the field names the format
   itself uses (Midas BLUE 1.1 §3.1.1) -- callers asked to see what is in the
   file, not a curated subset, so nothing is dropped or renamed. */
static void
load_header_fields (wfm_reader_state_t *r, const uint8_t h[512], int be)
{
  hdr_add (r, "version", 'A', 4, h + 0, be);
  hdr_add (r, "head_rep", 'A', 4, h + 4, be);
  hdr_add (r, "data_rep", 'A', 4, h + 8, be);
  hdr_add (r, "detached", 'L', 1, h + 12, be);
  hdr_add (r, "protected", 'L', 1, h + 16, be);
  hdr_add (r, "pipe", 'L', 1, h + 20, be);
  hdr_add (r, "ext_start", 'L', 1, h + 24, be);
  hdr_add (r, "ext_size", 'L', 1, h + 28, be);
  hdr_add (r, "data_start", 'D', 1, h + 32, be);
  hdr_add (r, "data_size", 'D', 1, h + 40, be);
  hdr_add (r, "type", 'L', 1, h + 48, be);
  hdr_add (r, "format", 'A', 2, h + 52, be);
  hdr_add (r, "flagmask", 'I', 1, h + 54, be);
  hdr_add (r, "timecode", 'D', 1, h + 56, be);
  hdr_add (r, "inlet", 'I', 1, h + 64, be);
  hdr_add (r, "outlets", 'I', 1, h + 66, be);
  hdr_add (r, "outmask", 'L', 1, h + 68, be);
  hdr_add (r, "pipeloc", 'L', 1, h + 72, be);
  hdr_add (r, "pipesize", 'L', 1, h + 76, be);
  hdr_add (r, "in_byte", 'D', 1, h + 80, be);
  hdr_add (r, "out_byte", 'D', 1, h + 88, be);
  hdr_add (r, "outbytes", 'D', 8, h + 96, be);
  hdr_add (r, "keylength", 'L', 1, h + 160, be);
  /* type-1000 adjunct (bytes 256+). doppler reads and writes type 1000 only
     -- parse_blue_hcb has already rejected anything else -- so the adjunct
     layout is not in question here. */
  hdr_add (r, "xstart", 'D', 1, h + 256, be);
  hdr_add (r, "xdelta", 'D', 1, h + 264, be);
  hdr_add (r, "xunits", 'L', 1, h + 272, be);
}

/* Decode the HCB's own keyword area (`keylength` at 160, `keywords` at 164,
   92 bytes) -- "KEY=VALUE" pairs separated by NUL, all values ASCII. This is
   where X-Midas commonly puts small metadata, and doppler used to ignore it
   entirely: such a file read back with no keywords at all and no indication
   that anything had been skipped. Values land as type 'A', same as an
   extended-header string, so callers cannot tell which block a key came
   from -- which is the point. */
static void
load_hcb_keywords (wfm_reader_state_t *r, const uint8_t h[512], int be)
{
  int32_t klen = 0;
  swab_copy (&klen, h + 160, 4, be);
  if (klen <= 0)
    return;
  size_t n = (size_t)klen;
  if (n > 92u) /* the area is 92 bytes; a larger keylength is malformed */
    n = 92u;
  const char *p   = (const char *)(h + 164);
  size_t      off = 0;
  while (off < n)
    {
      size_t end = off;
      while (end < n && p[end] != '\0')
        end++;
      size_t len = end - off;
      if (len == 0)
        {
          off = end + 1;
          continue;
        }
      const char *eq = (const char *)memchr (p + off, '=', len);
      if (eq)
        {
          size_t klen2 = (size_t)(eq - (p + off));
          size_t vlen  = len - klen2 - 1;
          char   tag[WFM_KW_MAX_TAG + 1];
          if (klen2 > 0 && klen2 <= WFM_KW_MAX_TAG)
            {
              memcpy (tag, p + off, klen2);
              tag[klen2] = '\0';
              kw_append (r, tag, 'A', vlen ? vlen : 1,
                         (const uint8_t *)(eq + 1));
            }
        }
      off = end + 1;
    }
}

/* Decode the extended header at [ext_off, ext_off + ext_size) of @p hf into
   r->kw. Best-effort by design: a file whose keyword region is truncated or
   malformed still yields its samples (and any keywords decoded before the bad
   entry), because metadata must never cost you the capture. Unrecognised
   keyword types are stepped over, per §3.3.1. */
static void
load_keywords (wfm_reader_state_t *r, FILE *hf, long ext_off, size_t ext_size)
{
  if (ext_off <= 0 || ext_size < 8)
    return;
  long save = ftell (hf);
  if (fseek (hf, ext_off, SEEK_SET) != 0)
    return;
  uint8_t *blob = (uint8_t *)malloc (ext_size);
  if (!blob)
    return;
  size_t got = fread (blob, 1, ext_size, hf);
  if (save >= 0)
    fseek (hf, save, SEEK_SET);

  size_t off = 0;
  while (off + 8 <= got)
    {
      wfm_keyword_t kw;
      size_t        used = 0;
      int rc = wfm_kw_decode (blob + off, got - off, r->endian, &kw, &used);
      if (rc < 0 || used == 0)
        break; /* malformed: keep what we have, stop walking */
      off += used;
      if (rc > 0)
        continue; /* unsupported type: skipped, not fatal */
      /* Goes through the SAME append helper as the HCB keyword area. This
         used to grow r->kw with its own doubling `cap`, which was correct
         only while it was the sole writer: once load_hcb_keywords could
         append first, `cap` started at 0 against a non-empty array and the
         next write ran off the end. One array, one append path. */
      kw_append (r, kw.tag, kw.type, kw.count, kw.value);
      free (kw.value);
    }
  free (blob);
}

/* Tags that carry a capture's centre frequency, most-conventional first. The
   array index plus one IS the wfm_fc_source_t value, so the enum and the
   search order cannot drift apart.

   BLUE type-1000 has no HCB field for centre frequency -- the adjunct's
   xstart/xdelta/xunits describe the abscissa, not the RF -- so an RF capture
   has to convey it as a keyword, and no tag for it is standardised: 1.1
   3.1.2.6.4.4 defines FREQ only as a type-6000 COLUMN name, under a heading
   that says those names "are not keyword names". FREQ in the HCB keyword area
   is nonetheless what captures in the wild carry, so it leads. */
static const char *const FC_TAGS[]
    = { "FREQ", "RF_FREQ", "CENTER_FREQ", "F_C" };

/* Read one keyword's value as a double, or -1 if it is not a lone number.

   BLUE has two encodings for the same quantity and a capture may use either.
   The HCB keyword area is ASCII by definition (3.1.1.24.1: "KEY=VALUE" text,
   NUL-terminated, no type field), so a frequency there arrives as characters
   and has to be parsed; an extended-header keyword is typed and binary, and
   has already been swapped to host order by the decoder. Both are accepted
   because both occur. */
static int
kw_as_double (const wfm_keyword_t *k, double *out)
{
  if (!k || !k->value || k->count == 0)
    return -1;
  if (k->type == 'A')
    {
      /* An 'A' value is a character count, not a C string -- copy before
         handing it to strtod. */
      char   buf[64];
      size_t n = (k->count < sizeof buf - 1) ? k->count : sizeof buf - 1;
      memcpy (buf, k->value, n);
      buf[n]     = '\0';
      char  *end = NULL;
      double v   = strtod (buf, &end);
      if (end == buf)
        return -1; /* no number at all */
      while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
        end++;
      if (*end != '\0')
        return -1; /* "2.4e9 Hz" is not a bare frequency; do not guess */
      *out = v;
      return 0;
    }
  if (k->elem_size != wfm_kw_elem_size (k->type))
    return -1;
  switch (k->type)
    {
    case 'B':
      {
        int8_t v;
        memcpy (&v, k->value, 1);
        *out = v;
        return 0;
      }
    case 'I':
      {
        int16_t v;
        memcpy (&v, k->value, 2);
        *out = v;
        return 0;
      }
    case 'L':
    case 'T': /* deprecated 32-bit integer alias */
      {
        int32_t v;
        memcpy (&v, k->value, 4);
        *out = v;
        return 0;
      }
    case 'X':
      {
        int64_t v;
        memcpy (&v, k->value, 8);
        *out = (double)v;
        return 0;
      }
    case 'F':
      {
        float v;
        memcpy (&v, k->value, 4);
        *out = v;
        return 0;
      }
    case 'D':
      {
        double v;
        memcpy (&v, k->value, 8);
        *out = v;
        return 0;
      }
    default:
      return -1;
    }
}

/* Resolve the centre frequency from the decoded keywords, and record WHICH
   tag it came from. Without the provenance, fc == 0.0 is ambiguous: a genuine
   baseband capture and a capture whose frequency we failed to find report the
   same number, and a caller has no way to ask which happened.

   Precedence is tag order first (every FREQ beats any RF_FREQ), then, within
   one tag, a TYPED value over an ASCII one, then the later occurrence over
   the earlier. The two inner rules agree for a file this library wrote: the
   HCB's ASCII copy is appended first and the extended header's typed copy
   last, and the typed copy is the one that did not round-trip through a
   decimal string.

   Note that a keyword saying zero IS a reading -- FREQ=0 declares baseband,
   and fc_source records that it was declared rather than defaulted. */
static void
load_fc (wfm_reader_state_t *r)
{
  for (size_t t = 0; t < sizeof FC_TAGS / sizeof *FC_TAGS; t++)
    {
      double best = 0.0;
      int    have = 0, have_typed = 0;
      for (size_t i = 0; i < r->nkw; i++)
        {
          double v;
          if (strcmp (r->kw[i].tag, FC_TAGS[t]) != 0)
            continue;
          if (kw_as_double (&r->kw[i], &v) != 0)
            continue;
          int typed = (r->kw[i].type != 'A');
          if (have && have_typed && !typed)
            continue; /* an ASCII copy never displaces a typed one */
          best       = v;
          have       = 1;
          have_typed = have_typed || typed;
        }
      if (have)
        {
          r->fc        = best;
          r->fc_source = (int)t + 1; /* FC_TAGS[0] is WFM_FC_FREQ */
          return;
        }
    }
}

/* Does this head look like the CSV this reader parses -- a first line that
   scans as "<number> , <number>"? Deciding by CONTENT rather than by the
   `.csv` extension is what stops a CSV called `capture.dat` from being read
   as binary IQ, which returns plausible garbage and says nothing.

   A NUL anywhere in the head disqualifies it immediately: no text file holds
   one and binary IQ very often does, which is the cheap half of the test. The
   expensive half is the same scan read_csv itself performs, so detection and
   parsing cannot disagree about what CSV is. */
/* Is @p line exactly @p nc comma-separated numbers -- "<number>,<number>" in
   complex mode, one number in scalar? strtod rather than sscanf both because
   it reports where it stopped -- a trailing extra column means this is
   somebody else's CSV, and guessing at that would be worse than reading the
   file as raw -- and because it cannot silently mis-convert. */
static int
is_iq_line (const char *line, unsigned nc)
{
  const char *p = line;
  for (unsigned c = 0; c < nc; c++)
    {
      char *end = NULL;
      if (c > 0)
        {
          while (*p == ' ' || *p == '\t')
            p++;
          if (*p != ',')
            return 0;
          p++;
        }
      (void)strtod (p, &end);
      if (end == p)
        return 0;
      p = end;
    }
  while (*p == ' ' || *p == '\t')
    p++;
  return *p == '\0';
}

static int
looks_like_csv (const uint8_t *h, size_t n, unsigned nc)
{
  if (n == 0)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (h[i] == '\0')
      return 0;

  char   line[256];
  size_t i = 0, k = 0;
  while (i < n && (h[i] == '\n' || h[i] == '\r'))
    i++; /* leading blank lines are not evidence either way */
  while (i < n && h[i] != '\n' && h[i] != '\r' && k + 1 < sizeof line)
    line[k++] = (char)h[i++];
  if (k + 1 >= sizeof line)
    return 0; /* a first "line" this long is not one of ours */
  line[k] = '\0';
  return is_iq_line (line, nc);
}

/* Map a SigMF datatype string ("cf32_le", "ci16_be", "ci8", "rf32_le", …) to
   element type, mode and endianness.

   The leading character is SigMF's own complex/real marker and is the mode:
   `c` is two components per sample, `r` is one. Reading an `rf32_le` sidecar
   as complex would return half as many samples with every other one landing
   in Q -- plausible-looking output from a file that said otherwise. */
static int
sigmf_datatype (const char *dt, int *stype, int *mode, int *endian)
{
  static const char *const ELEMNAME[5] = { "f32", "f64", "i32", "i16", "i8" };
  int                      md;
  if (dt == NULL)
    return -1;
  if (dt[0] == 'c')
    md = WFM_MODE_COMPLEX;
  else if (dt[0] == 'r')
    md = WFM_MODE_SCALAR;
  else
    return -1;
  for (int i = 0; i < 5; i++)
    {
      size_t L = strlen (ELEMNAME[i]);
      if (strncmp (dt + 1, ELEMNAME[i], L) == 0)
        {
          *stype  = i;
          *mode   = md;
          *endian = (strstr (dt + 1 + L, "be") != NULL);
          return 0;
        }
    }
  return -1;
}

/* Parse a SigMF .sigmf-meta sidecar for type/endian/fs/fc. Returns 0 on ok.
   @p has_fc distinguishes a sidecar that declares `core:frequency` from one
   that omits it -- both leave *fc at 0.0, and only the first of those is a
   reading. */
static int
parse_sigmf_meta (const char *meta_path, int *stype, int *mode, int *endian,
                  double *fs, double *fc, int *has_fc)
{
  FILE *mf = fopen (meta_path, "rb");
  if (!mf)
    return -1;
  fseek (mf, 0, SEEK_END);
  long sz = ftell (mf);
  fseek (mf, 0, SEEK_SET);
  if (sz <= 0)
    {
      fclose (mf);
      return -1;
    }
  char *buf = (char *)malloc ((size_t)sz + 1);
  if (!buf)
    {
      fclose (mf);
      return -1;
    }
  size_t got = fread (buf, 1, (size_t)sz, mf);
  buf[got]   = '\0';
  fclose (mf);
  cJSON *root = cJSON_Parse (buf);
  free (buf);
  if (!root)
    return -1;

  int    rc     = -1;
  cJSON *global = cJSON_GetObjectItem (root, "global");
  cJSON *dt = global ? cJSON_GetObjectItem (global, "core:datatype") : NULL;
  if (dt && cJSON_IsString (dt)
      && sigmf_datatype (dt->valuestring, stype, mode, endian) == 0)
    {
      cJSON *sr   = cJSON_GetObjectItem (global, "core:sample_rate");
      *fs         = (sr && cJSON_IsNumber (sr)) ? sr->valuedouble : 0.0;
      *fc         = 0.0;
      *has_fc     = 0;
      cJSON *caps = cJSON_GetObjectItem (root, "captures");
      if (caps && cJSON_GetArraySize (caps) > 0)
        {
          cJSON *c0 = cJSON_GetArrayItem (caps, 0);
          cJSON *fr = cJSON_GetObjectItem (c0, "core:frequency");
          if (fr && cJSON_IsNumber (fr))
            {
              *fc     = fr->valuedouble;
              *has_fc = 1;
            }
        }
      rc = 0;
    }
  cJSON_Delete (root);
  return rc;
}

/* Fill nsamples from the bytes remaining between the current offset and EOF.
 */
static void
fill_nsamples (wfm_reader_state_t *r)
{
  long cur = ftell (r->fp);
  if (cur >= 0 && fseek (r->fp, 0, SEEK_END) == 0)
    {
      long end = ftell (r->fp);
      fseek (r->fp, cur, SEEK_SET);
      if (end >= cur)
        r->num_samples
            = (size_t)(end - cur) / (comps (r->mode) * ELEM[r->sample_type]);
    }
}

/* Copy the parsed HCB fields the reader keeps. Split out so both BLUE entry
   points (header-first and .det-first) agree on what the header decides. */
static void
apply_hcb (wfm_reader_state_t *r, const blue_hcb_t *h)
{
  r->sample_type = h->stype;
  r->mode        = h->mode;
  r->endian      = h->endian;
  r->fs          = h->fs;
  r->num_samples = h->nsamples;
  r->data_bytes  = h->data_bytes;
  r->bounded     = 1;
  r->remaining   = h->nsamples;
  /* parse_blue_hcb derives fs as 1/xdelta and leaves it 0 when xdelta is 0,
     so a rate is declared exactly when it is non-zero. */
  r->fs_source = (h->fs != 0.0) ? WFM_FS_BLUE_XDELTA : WFM_FS_NONE;
  /* A zero timecode is UNSET, not 1950-01-01 -- doppler's own writer leaves
     the field zero, so converting it through would date every capture this
     library produces to 1950. wfm_timecode_is_set is the gate. */
  if (wfm_timecode_is_set (h->timecode))
    {
      r->t0_unix_sec = wfm_j1950_to_unix_sec (h->timecode);
      r->t0_source   = WFM_T0_BLUE_TIMECODE;
    }
  else
    {
      r->t0_unix_sec = 0.0;
      r->t0_source   = WFM_T0_NONE;
    }
}

/* Measure the payload bytes that do not complete a sample.

   Taken from what is actually on disk and then clamped to the declared
   payload, so one measurement catches both failure modes: a stride that does
   not divide the file (the sample_type hint is wrong for a headerless
   file type, or a BLUE data_size was written mid-sample) and a capture that
   was cut short of what its header promises. Called once the file is
   positioned at the first sample, and it leaves the position where it found
   it. */
static void
compute_trailing (wfm_reader_state_t *r)
{
  if (!r->fp || r->file_type == WFM_FT_CSV)
    return; /* CSV is delimited, not strided: there is no partial sample */
  long cur = ftell (r->fp);
  if (cur < 0 || fseek (r->fp, 0, SEEK_END) != 0)
    return;
  long end = ftell (r->fp);
  if (fseek (r->fp, cur, SEEK_SET) != 0 || end < cur)
    return;
  size_t avail  = (size_t)(end - cur);
  size_t stride = comps (r->mode) * ELEM[r->sample_type];
  if (r->data_bytes && r->data_bytes < avail)
    avail = r->data_bytes;
  r->trailing = avail % stride;
}

/* Count the samples a CSV holds, parsing exactly the way read_csv does so the
   count and the read can never disagree, then put the file position back.

   Called lazily from the num_samples getter rather than at open: a CSV has no
   header to declare its length, the only way to know is to walk the whole
   file, and a caller streaming a large capture should not pay for a scan it
   never asked for. Opening stays O(1). */
static void
count_csv (wfm_reader_state_t *r)
{
  long cur = ftell (r->fp);
  if (cur < 0 || fseek (r->fp, r->data_off, SEEK_SET) != 0)
    return;
  size_t         n  = 0;
  const unsigned nc = comps (r->mode);
  double         a, b;
  if (nc == 2u)
    while (fscanf (r->fp, " %lf , %lf", &a, &b) == 2)
      n++;
  else
    while (fscanf (r->fp, " %lf", &a) == 1)
      n++;
  clearerr (r->fp); /* the scan ends at EOF by design; that is not an error */
  if (fseek (r->fp, cur, SEEK_SET) != 0)
    return;
  r->num_samples = n;
  r->csv_counted = 1;
}

/* Open @p path as SigMF, taking its metadata from @p meta. Shared by the two
   ways a SigMF capture is reached: the `.sigmf-data` name, and any other name
   that turns out to have a `<base>.sigmf-meta` sidecar beside it. Returns 0
   on success. */
static int
open_sigmf (wfm_reader_state_t *r, const char *path, const char *meta)
{
  int has_fc = 0;
  if (parse_sigmf_meta (meta, &r->sample_type, &r->mode, &r->endian, &r->fs,
                        &r->fc, &has_fc)
      != 0)
    return -1;
  if (has_fc)
    r->fc_source = WFM_FC_SIGMF;
  /* `core:sample_rate` is optional, so parse_sigmf_meta leaves fs at 0.0 when
     it is absent -- non-zero is exactly "the metadata declared one". */
  r->fs_source = (r->fs != 0.0) ? WFM_FS_SIGMF : WFM_FS_NONE;
  /* `core:datetime` is an ISO 8601 STRING and this reader has no parser for
     one, so a SigMF capture reports WFM_T0_NONE rather than a guess. Wiring
     it up is a parser away, not a redesign. */
  r->t0_source = WFM_T0_NONE;
  r->file_type = WFM_FT_SIGMF;
  if (r->fp)
    fclose (r->fp);
  r->fp = fopen (path, "rb");
  if (!r->fp)
    return -1;
  fill_nsamples (r);
  return 0;
}

/* Record where the samples begin, so reset() can rewind to exactly here. The
   offset differs per file type (512 into an attached BLUE, 0 for a .det, raw
   or SigMF payload), so it is captured once each path is positioned -- which
   is also the moment the payload can be measured. */
static wfm_reader_state_t *
ready (wfm_reader_state_t *r)
{
  long p      = ftell (r->fp);
  r->data_off = (p > 0) ? p : 0;
  compute_trailing (r);
  return r;
}

wfm_reader_state_t *
wfm_reader_create (const char *path, int hint_stype, int hint_endian)
{
  /* The hint names one of the ten wavegen-order sample types. Indices 5..9
     are the SCALAR five, and they are the only way to say "this headerless
     file is real": raw and CSV carry no metadata, so without them a real
     capture could only be read as interleaved I/Q at half the sample count
     and with every other sample landing in Q.

     Split rather than widened. `sample_type` stays the 0..4 ELEMENT index
     that ELEM[], SCALE[] and convert_elem() are all indexed by, and the mode
     goes where the mode goes -- a header-bearing file sets both from its own
     bytes further down, and this is the same two fields set from a hint. */
  if (!path || hint_stype < 0 || hint_stype > 9)
    return NULL;
  wfm_reader_state_t *r = (wfm_reader_state_t *)calloc (1, sizeof *r);
  if (!r)
    return NULL;
  r->sample_type = hint_stype % 5;
  r->mode        = (hint_stype >= 5) ? WFM_MODE_SCALAR : WFM_MODE_COMPLEX;
  r->endian      = hint_endian ? 1 : 0;
  char side[1024];

  /* SigMF named as such: <base>.sigmf-data REQUIRES its <base>.sigmf-meta
     sidecar. A .sigmf-data with no sidecar is not a capture with missing
     metadata, it is half a capture -- the datatype lives only in the sidecar,
     so there is nothing to fall back to. (A file reached under any other name
     that happens to have a sidecar is picked up further down; there, falling
     back to raw is reasonable.) */
  if (ends_with (path, ".sigmf-data"))
    {
      wfm_swap_ext (path, ".sigmf-meta", side, sizeof side);
      if (open_sigmf (r, path, side) != 0)
        goto fail;
      return ready (r);
    }

  /* BLUE detached, entered from the DATA side: <base>.det + its header
     sibling. The header is conventionally <base>.tmp or <base>.prm (spec
     3.1.1.4); doppler's own writer emits <base>.hdr. Try each — the usual
     entry point is the header itself, handled by the magic-peek path below. */
  if (ends_with (path, ".det"))
    {
      static const char *const HDR_EXT[] = { ".hdr", ".prm", ".tmp" };
      FILE                    *hf        = NULL;
      for (size_t i = 0; i < sizeof HDR_EXT / sizeof *HDR_EXT && !hf; i++)
        {
          wfm_swap_ext (path, HDR_EXT[i], side, sizeof side);
          hf = fopen (side, "rb");
        }
      if (!hf)
        goto fail;
      uint8_t    h[512];
      int        ok = (fread (h, 1, 512, hf) == 512);
      blue_hcb_t hcb;
      if (!ok || parse_blue_hcb (h, &hcb) != 0)
        {
          fclose (hf);
          goto fail;
        }
      apply_hcb (r, &hcb);
      /* the extended header lives in the HEADER file, which is not the file
         the samples come from — decode it before letting go of hf. */
      load_header_fields (r, h, hcb.endian);
      load_hcb_keywords (r, h, hcb.endian);
      load_keywords (r, hf, hcb.ext_off, hcb.ext_size);
      load_fc (r); /* after BOTH keyword blocks: either may carry it */
      fclose (hf);
      r->file_type = WFM_FT_BLUE;
      r->fp        = fopen (path, "rb"); /* .det is raw from byte 0 */
      if (!r->fp)
        goto fail;
      return ready (r);
    }

  /* Everything else is decided by looking INSIDE the file. The name is only
     consulted where the content cannot settle it (below), because a capture
     that got renamed, or saved with whatever extension a tool felt like, is
     still the capture -- and the failure mode of guessing wrong here is not
     an error, it is plausible garbage at the wrong stride. */
  r->fp = fopen (path, "rb");
  if (!r->fp)
    goto fail;
  uint8_t h[512];
  size_t  got = fread (h, 1, 512, r->fp);
  if (got >= 4 && memcmp (h, "BLUE", 4) == 0)
    {
      blue_hcb_t hcb;
      if (got != 512 || parse_blue_hcb (h, &hcb) != 0)
        goto fail;
      apply_hcb (r, &hcb);
      r->file_type = WFM_FT_BLUE;
      /* this file IS the header (attached or detached), so its extended
         header is here regardless of where the samples end up. */
      load_header_fields (r, h, hcb.endian);
      load_hcb_keywords (r, h, hcb.endian);
      load_keywords (r, r->fp, hcb.ext_off, hcb.ext_size);
      load_fc (r); /* after BOTH keyword blocks: either may carry it */
      if (hcb.detached != 0)
        {
          /* Detached (spec 3.1.1.4): this file holds ONLY the header +
             extended keywords; the payload is a separate <base>.det. The
             header file is conventionally <base>.tmp or <base>.prm (doppler's
             own writer uses <base>.hdr) — the extension is irrelevant, the
             `detached` field is what decides. det == 1 means the collocated
             .det; 2..127 name an X-Midas auxiliary path (3.1.1.4.1) that
             cannot be resolved without an X-Midas environment, so we try the
             collocated file and fail loudly rather than misread. WITHOUT this
             branch data_start (0 for a detached capture) seeks back to byte 0
             of the HEADER file and the 512-byte HCB is returned as IQ. */
          wfm_swap_ext (path, ".det", side, sizeof side);
          fclose (r->fp);
          r->fp = fopen (side, "rb");
          if (!r->fp)
            goto fail;
          return ready (r); /* .det carries raw payload from byte 0 */
        }
      fseek (r->fp, (long)hcb.data_start, SEEK_SET);
      return ready (r);
    }

  /* No sidecar probe here, deliberately. Sniffing for `<base>.sigmf-meta`
     beside any file would make a SigMF pair readable whatever the data half
     is called -- but the base name is shared, so `cap.csv` sitting next to an
     unrelated `cap.sigmf-meta` would be read with that capture's datatype.
     Verified: it hijacked two files in the very first round of testing. The
     `.sigmf-data` name is part of the format, so it is required on both
     sides; wfm_writer_create rejects a SigMF path that does not use it. */

  /* Text that parses as `I,Q` is CSV, whatever the file is called -- checked
     after BLUE, so a BLUE capture misnamed `.csv` still reads as BLUE.

     The extension stays as a fallback rather than being dropped: a CSV whose
     first line is a column header ("I,Q") fails the content test, and reading
     such a file as binary IQ would be a regression on the old behaviour.
     Content decides; the name still gets a vote. */
  if (looks_like_csv (h, got, comps (r->mode)) || ends_with (path, ".csv"))
    {
      fclose (r->fp);
      r->file_type = WFM_FT_CSV;
      r->fp        = fopen (path, "r");
      if (!r->fp)
        goto fail;
      return ready (r);
    }

  rewind (r->fp);
  r->file_type = WFM_FT_RAW;
  fill_nsamples (r);
  return ready (r);

fail:
  if (r->fp)
    fclose (r->fp);
  free (r);
  return NULL;
}

void
wfm_reader_info (const wfm_reader_state_t *r, wfm_reader_info_t *info)
{
  info->file_type   = r->file_type;
  info->sample_type = reported_stype (r);
  info->mode        = r->mode;
  info->endian      = r->endian;
  info->fs          = r->fs;
  info->fc          = r->fc;
  /* through the getter, so a CSV reports its length here too rather than the
     0 that means "not counted yet" */
  info->num_samples    = wfm_reader_get_num_samples (r);
  info->fc_source      = r->fc_source;
  info->fs_source      = r->fs_source;
  info->t0_unix_sec    = r->t0_unix_sec;
  info->t0_source      = r->t0_source;
  info->trailing_bytes = r->trailing;
}

/* CSV: one "I,Q" per line; integer wire types are divided back by full-scale.
 */
static size_t
read_csv (wfm_reader_state_t *r, float _Complex *out, size_t max)
{
  double         scale = (r->sample_type >= 2) ? SCALE[r->sample_type] : 1.0;
  const unsigned nc    = comps (r->mode);
  size_t         i;
  for (i = 0; i < max; i++)
    {
      double a, b = 0.0;
      if (nc == 2u)
        {
          if (fscanf (r->fp, " %lf , %lf", &a, &b) != 2)
            break;
        }
      else if (fscanf (r->fp, " %lf", &a) != 1)
        break;
      out[i] = (float)(a / scale) + (float)(b / scale) * (float _Complex)I;
    }
  return i;
}

/* Convert up to `max` whole samples from the current position. The one place
   bytes become samples -- wfm_reader_read applies the declared-payload bound
   around it, wfm_reader_read_follow applies its own.

   A PARTIAL sample at the end is un-consumed rather than dropped. fread has
   already taken those bytes from the stream, so dropping them leaves the next
   read one element out of phase and every sample after it wrong -- silently,
   and forever. Only reachable on a file being appended to (a finished capture
   has no partial tail to meet), but the fix belongs here, where the bytes
   are. docs/design/end-of-capture.md section 2b. */
static size_t
read_block (wfm_reader_state_t *r, size_t max, float _Complex *out)
{
  size_t elem = ELEM[r->sample_type], nc = comps (r->mode);
  size_t need = max * nc * elem;
  if (r->scratch_cap < need)
    {
      uint8_t *q = (uint8_t *)realloc (r->scratch, need);
      if (!q)
        return 0;
      r->scratch     = q;
      r->scratch_cap = need;
    }
  size_t got   = fread (r->scratch, 1, need, r->fp);
  size_t nsamp = got / (nc * elem);
  size_t rem   = got - nsamp * (nc * elem);
  if (rem)
    (void)fseek (r->fp, -(long)rem, SEEK_CUR);
  const uint8_t *p = r->scratch;
  for (size_t i = 0; i < nsamp; i++)
    {
      /* scalar mode has no Q component on the wire -- it reads as exactly 0,
         so an 'S' capture surfaces as a real signal on the imaginary axis. */
      float re = convert_elem (p, r->sample_type, r->endian);
      float im = (nc == 2) ? convert_elem (p + elem, r->sample_type, r->endian)
                           : 0.0f;
      out[i]   = re + im * (float _Complex)I;
      p += nc * elem;
    }
  return nsamp;
}

size_t
wfm_reader_read (wfm_reader_state_t *r, size_t max, float _Complex *out,
                 size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). Clamped before
     the CSV branch so read_csv inherits the bound too. */
  if (max > max_out)
    max = max_out;
  if (max == 0)
    return 0;
  if (r->file_type == WFM_FT_CSV)
    return read_csv (r, out, max);

  if (r->bounded)
    {
      if (r->remaining == 0)
        return 0; /* the payload is spent; whatever follows is not samples */
      if (max > r->remaining)
        max = r->remaining;
    }
  size_t nsamp = read_block (r, max, out);
  if (r->bounded)
    r->remaining -= nsamp;
  return nsamp;
}

/* ── Following a capture that is still being written ──────────────────────
 * docs/design/end-of-capture.md.
 *
 * The one rule: a short read means NOTHING. Only an explicit marker ends
 * the wait -- never the file going quiet, which is a slow writer, a stalled
 * writer and a finished writer wearing the same face.
 *
 * Two hazards that bite the NAIVE approach (call read() in a loop) do not
 * arise here, and it is worth saying why rather than defending against them
 * twice. follow_available() seeks and divides: it reports whole samples, so
 * read_block is never asked for a partial one, and the seek clears stdio's
 * latched end-of-file indicator as a side effect of asking. Both hazards
 * are real on the plain read() path -- see read_block, and
 * test_read_never_consumes_a_partial_sample. */

/* Whole samples readable right now. Consumes nothing and leaves the read
   position where it found it. */
static size_t
follow_available (wfm_reader_state_t *r)
{
  long cur = ftell (r->fp);
  if (cur < 0)
    return 0;
  if (fseek (r->fp, 0, SEEK_END) != 0)
    return 0;
  long end = ftell (r->fp);
  if (fseek (r->fp, cur, SEEK_SET) != 0 || end < cur)
    return 0;
  size_t stride = comps (r->mode) * ELEM[r->sample_type];
  size_t avail  = (size_t)(end - cur) / stride;
  /* Once the length is real, anything past the payload (the extended
     header) is not samples. */
  if (r->follow_bounded)
    {
      size_t used = (size_t)(cur - r->data_off);
      size_t left
          = (r->data_bytes > used) ? (r->data_bytes - used) / stride : 0;
      if (avail > left)
        avail = left;
    }
  return avail;
}

/* Has the writer declared the capture finished? Explicit only -- a file that
   has merely stopped growing is NOT an ending, which is the inference this
   whole contract exists to remove.

   BLUE patches data_size at close(), so the placeholder -> real transition
   is the writer having finished, observed.

   Raw and CSV never end, and the reason is DISCARDABILITY rather than a
   lack of room. They do have somewhere to put one: a raw or CSV capture
   already gets a `<path>.sigmf-meta` sidecar carrying the fs/fc/t0 those
   containers cannot hold. A marker could go there too -- and it would not
   be a guarantee, because that sidecar is a SECOND FILE. It is opt-out
   (`sidecar=false`, for a downstream whose glob an extra file would
   break), and it can be moved, dropped or copied away from its data
   independently. Inside the container is no better: raw has no framing, so
   anything written inline is indistinguishable from samples and would be
   read as them.

   What makes DP_ERR_EOF a guarantee on BLUE is that its marker lives in
   the artifact the reader is ALREADY reading -- a header field, patched in
   place at close(). A best-effort marker in a file that may not arrive is
   a worse answer than no marker, because a reader would learn to trust it.
   So only a stop request can finish that wait. */
static int
follow_ended (wfm_reader_state_t *r)
{
  if (r->follow_bounded)
    return 1;
  if (r->file_type != WFM_FT_BLUE)
    return 0;
  long cur = ftell (r->fp);
  if (cur < 0 || fseek (r->fp, 40, SEEK_SET) != 0)
    return 0;
  uint8_t h[8];
  size_t  got = fread (h, 1, 8, r->fp);
  if (fseek (r->fp, cur, SEEK_SET) != 0 || got != 8)
    return 0;
  double ds = 0.0;
  swab_copy (&ds, h, 8, r->endian);
  if (!(ds > 0.0))
    return 0; /* still the placeholder the writer opened with */
  r->data_bytes     = (size_t)ds;
  r->follow_bounded = 1;
  return 1;
}

/* Sleep one interrupt slice, or the remaining budget if that is shorter. */
static uint32_t
follow_nap (uint32_t remaining_ms)
{
  /* The same slice every other doppler wait uses. The MACRO, not the
     accessor: a compile-time constant costs no link dependency, which is the
     whole point of stop_fn above. */
  uint32_t slice = DP_INTERRUPT_LATENCY_DEFAULT_MS;
  if (remaining_ms && slice > remaining_ms)
    slice = remaining_ms;
  struct timespec ts
      = { (time_t)(slice / 1000u), (long)(slice % 1000u) * 1000000L };
  nanosleep (&ts, NULL);
  return slice;
}

size_t
wfm_reader_read_follow_max_out (wfm_reader_state_t *r, size_t n)
{
  (void)r;
  /* The caller's ask, NOT what is available: jm's binding calls this to size
     the output array BEFORE the kernel runs, so answering "available now"
     would hand a blocking read a zero-length buffer at exactly the moment it
     is supposed to wait. */
  return n;
}

int
wfm_reader_get_ending (const wfm_reader_state_t *r)
{
  return r->ending;
}

uint32_t
wfm_reader_get_follow_timeout_ms (const wfm_reader_state_t *r)
{
  return r->follow_timeout_ms;
}

void
wfm_reader_set_follow_timeout_ms (wfm_reader_state_t *r, uint32_t val)
{
  r->follow_timeout_ms = val;
}

uint32_t
wfm_reader_get_follow_grace_ms (const wfm_reader_state_t *r)
{
  return r->follow_grace_ms;
}

void
wfm_reader_set_follow_grace_ms (wfm_reader_state_t *r, uint32_t val)
{
  r->follow_grace_ms = val;
}

void
wfm_reader_set_stop_fn (wfm_reader_state_t *r, int (*fn) (void))
{
  r->stop_fn = fn;
}

size_t
wfm_reader_read_follow (wfm_reader_state_t *r, size_t n, float complex *out,
                        size_t max_out)
{
  if (max_out < n)
    n = max_out;
  r->ending = WFM_FOLLOW_NONE;
  if (n == 0)
    return 0;

  /* One clock with two values: the caller's wait budget until a stop is
     requested, then the grace budget for the writer to land its marker.
     0 means forever in both -- the escape is the stop, not a clock. */
  uint32_t budget   = r->follow_timeout_ms;
  int      stopping = 0;

  for (;;)
    {
      size_t avail = follow_available (r);
      if (avail > 0)
        {
          /* Drain outranks the stop: data already in the file is returned
             even once a stop has been requested, or Ctrl+C discards a tail
             that is already safely on disk. */
          if (avail < n)
            n = avail;
          return read_block (r, n, out);
        }
      if (follow_ended (r))
        {
          /* The marker may have landed with samples still unread. */
          if (follow_available (r) > 0)
            continue;
          r->ending = WFM_FOLLOW_EOF;
          return 0;
        }
      if (!stopping && r->stop_fn && r->stop_fn ())
        {
          stopping = 1;
          budget   = r->follow_grace_ms;
        }
      if (budget == 0)
        {
          follow_nap (0);
          continue;
        }
      uint32_t slept = follow_nap (budget);
      budget         = (slept >= budget) ? 0 : budget - slept;
      if (budget == 0)
        {
          r->ending = stopping ? WFM_FOLLOW_INTERRUPTED : WFM_FOLLOW_TIMEOUT;
          return 0;
        }
    }
}

size_t
wfm_reader_read_max_out (wfm_reader_state_t *r, size_t n)
{
  /* gh-607: a read(n) yields at most n samples (fewer at EOF), so report n
     as the per-call bound; the binding sizes its buffer to this and resizes
     down to the actual count read. A reader streams, so it never needs the
     whole capture pre-allocated. */
  (void)r;
  return n;
}

void
wfm_reader_reset (wfm_reader_state_t *r)
{
  if (!r || !r->fp)
    return;
  fseek (r->fp, r->data_off, SEEK_SET);
  r->remaining = r->num_samples; /* only consulted when `bounded` */
}

/* Property accessors for the generated binding (the "computed" property kind).
   Keeping these instead of exposing the struct is what lets the layout above
   stay private -- jm only needs a pointer to an incomplete type. */
int
wfm_reader_get_file_type (const wfm_reader_state_t *r)
{
  return r->file_type;
}

int
wfm_reader_get_sample_type (const wfm_reader_state_t *r)
{
  return reported_stype (r);
}

int
wfm_reader_get_mode (const wfm_reader_state_t *r)
{
  return r->mode;
}

int
wfm_reader_get_endian (const wfm_reader_state_t *r)
{
  return r->endian;
}

double
wfm_reader_get_fs (const wfm_reader_state_t *r)
{
  return r->fs;
}

double
wfm_reader_get_fc (const wfm_reader_state_t *r)
{
  return r->fc;
}

size_t
wfm_reader_get_num_samples (const wfm_reader_state_t *r)
{
  /* Every other file type declares its length in a header; a CSV's has to be
     counted, so it is counted here, once, on the first caller who asks. The
     const cast is the price of that laziness behind jm's const getter --
     nothing observable changes, count_csv restores the file position, and the
     alternative is either a full scan of every CSV at open or the 0 this used
     to return, which reads as "empty capture". */
  if (r->file_type == WFM_FT_CSV && !r->csv_counted)
    count_csv ((wfm_reader_state_t *)r);
  return r->num_samples;
}

int
wfm_reader_get_fc_source (const wfm_reader_state_t *r)
{
  return r->fc_source;
}

int
wfm_reader_get_fs_source (const wfm_reader_state_t *r)
{
  return r->fs_source;
}

double
wfm_reader_get_t0 (const wfm_reader_state_t *r)
{
  return r->t0_unix_sec;
}

int
wfm_reader_get_t0_source (const wfm_reader_state_t *r)
{
  return r->t0_source;
}

size_t
wfm_reader_get_trailing_bytes (const wfm_reader_state_t *r)
{
  return r->trailing;
}

size_t
wfm_reader_num_keywords (const wfm_reader_state_t *r)
{
  return r->nkw;
}

const wfm_keyword_t *
wfm_reader_keyword (const wfm_reader_state_t *r, size_t i)
{
  return (i < r->nkw) ? &r->kw[i] : NULL;
}

/* key_fn for the `.keywords` dict property (gh-543): the tag of the i-th
   keyword. jm's generated loop calls it for 0 <= i < wfm_reader_num_keywords,
   so the index is always in range. */
const char *
wfm_reader_keyword_tag (const wfm_reader_state_t *r, size_t i)
{
  return r->kw[i].tag;
}

size_t
wfm_reader_num_header_fields (const wfm_reader_state_t *r)
{
  return r->nhdr;
}

/* entry_fn for the `.header` dict property: the i-th decoded HCB field. */
const wfm_keyword_t *
wfm_reader_header_field (const wfm_reader_state_t *r, size_t i)
{
  return (i < r->nhdr) ? &r->hdr[i] : NULL;
}

/* key_fn for `.header`: the field's name as the format spells it. */
const char *
wfm_reader_header_tag (const wfm_reader_state_t *r, size_t i)
{
  return r->hdr[i].tag;
}

/* Look a header field up by name, for callers that want one value rather
   than the whole dict. */
const wfm_keyword_t *
wfm_reader_find_header_field (const wfm_reader_state_t *r, const char *name)
{
  for (size_t i = 0; i < r->nhdr; i++)
    if (strcmp (r->hdr[i].tag, name) == 0)
      return &r->hdr[i];
  return NULL;
}

const wfm_keyword_t *
wfm_reader_find_keyword (const wfm_reader_state_t *r, const char *tag)
{
  for (size_t i = 0; i < r->nkw; i++)
    if (strcmp (r->kw[i].tag, tag) == 0)
      return &r->kw[i];
  return NULL;
}

void
wfm_reader_destroy (wfm_reader_state_t *r)
{
  if (!r)
    return;
  if (r->fp)
    fclose (r->fp);
  for (size_t i = 0; i < r->nkw; i++)
    free (r->kw[i].value);
  free (r->kw);
  for (size_t i = 0; i < r->nhdr; i++)
    free (r->hdr[i].value);
  free (r->hdr);
  free (r->scratch);
  free (r);
}
