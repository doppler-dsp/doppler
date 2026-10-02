/**
 * @file wfm_data.c
 * @brief A frame's data source (wfm/wfm_data.h).
 *
 * Three shapes behind one "next chunk": a finite Field rendered once and
 * walked, a `pn:0` generator kept running, and an fd read on demand. The
 * rules they share -- the fill, the counts, one draw per frame -- live in
 * dp_wfm_data_next / dp_wfm_data_idle, once.
 */
#include "doppler/wfm/wfm_data.h"

#include "doppler/clib_common.h"  /* dp_xmalloc */
#include "doppler/cvt/cvt_core.h" /* dp_bytes_to_bin */
#include "doppler/dp_hash64.h"
#include "doppler/pn/pn_core.h"
#include "doppler/wfm/wfm_frame.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* The fd layer, once per platform: open, read, close, is-it-a-file, and
   "is there anything to read within the timeout". Windows has no poll(2)
   for a pipe, so availability comes from PeekNamedPipe, which works on
   anonymous pipes (stdin from a shell pipe is one); a file or a console is
   always ready, as poll(2) says of a regular file. */
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#define STDIN_FD 0
static int
fd_open (const char *path)
{
  return _open (path, _O_RDONLY | _O_BINARY);
}
static long long
fd_read (int fd, void *b, size_t n)
{
  return _read (fd, b, (unsigned)n);
}
static void
fd_close (int fd)
{
  (void)_close (fd);
}
static int
fd_stat (int fd, int *regular, unsigned long long *bytes)
{
  struct _stat64 sb;
  if (_fstat64 (fd, &sb) != 0)
    return -1;
  *regular = (sb.st_mode & _S_IFMT) == _S_IFREG;
  *bytes   = (unsigned long long)sb.st_size;
  if (!*regular)
    (void)_setmode (fd, _O_BINARY); /* a pipe's bytes, not text: no CRLF,
                                       no ^Z as end of file */
  return 0;
}
/* Move a regular file's read position to @p off octets from its start, 0
   or -1, and say where it is now. Only a regular file is ever moved: a
   restore re-reads its prefix (dp_wfm_data_set_state). */
static int
fd_seek (int fd, long long off)
{
  return _lseeki64 (fd, off, SEEK_SET) < 0 ? -1 : 0;
}
static long long
fd_tell (int fd)
{
  return _lseeki64 (fd, 0, SEEK_CUR);
}
static int
fd_wait (int fd, int timeout_ms)
{
  HANDLE h = (HANDLE)_get_osfhandle (fd);
  if (GetFileType (h) != FILE_TYPE_PIPE)
    return 1;
  for (int waited = 0;; waited++)
    {
      DWORD avail = 0;
      if (!PeekNamedPipe (h, NULL, 0, NULL, &avail, NULL))
        return 1; /* broken: the read reports the end */
      if (avail)
        return 1;
      if (waited >= timeout_ms)
        return 0;
      Sleep (1);
    }
}
#else
#include <poll.h>
#include <unistd.h>
#define STDIN_FD STDIN_FILENO
static int
fd_open (const char *path)
{
  return open (path, O_RDONLY);
}
static long long
fd_read (int fd, void *b, size_t n)
{
  return (long long)read (fd, b, n);
}
static void
fd_close (int fd)
{
  (void)close (fd);
}
static int
fd_stat (int fd, int *regular, unsigned long long *bytes)
{
  struct stat sb;
  if (fstat (fd, &sb) != 0)
    return -1;
  *regular = S_ISREG (sb.st_mode);
  *bytes   = (unsigned long long)sb.st_size;
  return 0;
}
static int
fd_seek (int fd, long long off)
{
  return lseek (fd, (off_t)off, SEEK_SET) < 0 ? -1 : 0;
}
static long long
fd_tell (int fd)
{
  return (long long)lseek (fd, 0, SEEK_CUR);
}
static int
fd_wait (int fd, int timeout_ms)
{
  for (;;)
    {
      struct pollfd p = { .fd = fd, .events = POLLIN };
      const int     r = poll (&p, 1, timeout_ms);
      if (r < 0 && errno == EINTR)
        continue;
      return r < 0 ? -1 : r > 0;
    }
}
#endif

typedef enum
{
  SRC_FIELD = 0, /* a finite Field, rendered at create */
  SRC_PN    = 1, /* pn:0 -- a generator kept running */
  SRC_FD    = 2  /* a file or a pipe, read on demand */
} src_kind_t;

struct wfm_data_src
{
  src_kind_t kind;
  size_t     len; /* bits per frame */

  /* SRC_FIELD */
  uint8_t *bits;
  size_t   nbits, pos;

  /* SRC_PN */
  dp_pn_state_t *pn;

  /* SRC_FD: `res` holds bits read but not yet sent -- at most len + 7,
     because a read asks only for the octets the frame still needs. */
  int      fd, own_fd, eof;
  uint8_t *res, *oct;
  size_t   have;

  uint8_t *fill; /* NULL when none was declared */
  size_t   nfill;

  int              ended; /* the padded last frame has been sent */
  wfm_data_stats_t st;
};

/* A refusal's reason: STATIC, as every refusal in doppler is
   (`const char **why`). The one number a face may want -- a finite source's
   length, to state a remainder -- is dp_wfm_data_length_bits(), not a
   second form of `why`. */
static void
say (const char **why, const char *msg)
{
  if (why)
    *why = msg;
}

static wfm_data_src_t *
refuse (wfm_data_src_t *s, const char **why, const char *msg)
{
  if (msg)
    say (why, msg);
  dp_wfm_data_destroy (s);
  return NULL;
}

/* A Field's bits through the one door (dp_wfm_field_bits), which refuses
   data:LEN by name. Returns the count, or 0 with the parser's reason. */
static size_t
field_to_bits (const char *spec, uint8_t **out, const char **why)
{
  const size_t n = dp_wfm_field_bits (spec, NULL, 0, why);
  if (n == 0)
    return 0;
  *out = dp_xmalloc (n);
  (void)dp_wfm_field_bits (spec, *out, n, NULL);
  return n;
}

/* Every refusal decidable before the first sample (§4.3, §4.4), once. */
static wfm_data_src_t *
check_length (wfm_data_src_t *s, const char **why)
{
  if (s->st.stream)
    {
      /* A one-bit frame has no remainder to pad, and an idle frame with no
         fill is an error that ends the run (dp_wfm_data_idle), so only a
         wider frame needs one. */
      if (s->kind == SRC_FD && !s->fill && s->len > 1)
        return refuse (s, why,
                       "a stream's length is unknown until it ends, so it "
                       "needs a fill (--fill) for its last frame and for "
                       "idle frames");
      return s;
    }
  const uint64_t n = s->st.total_bits;
  if (n == 0)
    return refuse (s, why, "the data is empty: a burst of no frames");
  if (n % s->len && !s->fill)
    return refuse (s, why,
                   "the data does not fill its last frame and no fill "
                   "(--fill) is declared; dp_wfm_data_length_bits() gives "
                   "its length, so the remainder can be stated in bits");
  return s;
}

/* A source with its fill already rendered to bits (owned from here). */
static wfm_data_src_t *
alloc_src_bits (size_t len, uint8_t *fill, size_t nfill)
{
  wfm_data_src_t *s = dp_xcalloc (1, sizeof *s);
  s->len            = len;
  s->fd             = -1;
  s->fill           = fill;
  s->nfill          = nfill;
  return s;
}

static wfm_data_src_t *
alloc_src (size_t len, const char *fill, const char **why)
{
  uint8_t *f  = NULL;
  size_t   nf = 0;
  if (fill)
    {
      nf = field_to_bits (fill, &f, why);
      if (nf == 0)
        return NULL;
    }
  return alloc_src_bits (len, f, nf);
}

/* A sequence's bits through the one renderer, dp_wfm_seq_bits: a data
   field is refused by name, since it has no bits of its own to supply. */
static size_t
seq_to_bits (const wfm_seq_t *q, uint8_t **out, const char **why)
{
  if (q->kind == WFM_SEQ_DATA)
    {
      say (why, "data:LEN has no bits of its own: it cannot be a data "
                "source or a fill");
      return 0;
    }
  uint8_t *b = dp_xmalloc (q->len);
  if (dp_wfm_seq_bits (q, b, q->len) != q->len)
    {
      free (b);
      say (why, "a data source or fill sequence that cannot be built");
      return 0;
    }
  *out = b;
  return q->len;
}

/* Make @p s a source over @p fd: a regular file is finite, its length from
   fstat; anything else is a stream. Consumes @p s on refusal. */
static wfm_data_src_t *
src_on_fd (wfm_data_src_t *s, int fd, const char **why)
{
  int                regular;
  unsigned long long bytes;
  if (fd < 0 || fd_stat (fd, &regular, &bytes) != 0)
    return refuse (s, why, "the data source cannot be read (errno says why)");
  s->kind      = SRC_FD;
  s->fd        = fd;
  s->st.hashed = 1;
  s->st.hash   = DP_HASH64_INIT;
  s->res       = dp_xmalloc (s->len + 8u);
  s->oct       = dp_xmalloc (s->len / 8u + 2u);
  if (regular)
    s->st.total_bits = (uint64_t)bytes * 8u;
  else
    s->st.stream = 1;
  return check_length (s, why);
}

/* A source over @p path, `-` for stdin; the file is the source's to close.
   Consumes @p s on refusal. */
static wfm_data_src_t *
src_on_path (wfm_data_src_t *s, const char *path, const char **why)
{
  if (strcmp (path, "-") == 0)
    return src_on_fd (s, STDIN_FD, why);
  const int fd = fd_open (path);
  if (fd < 0)
    return refuse (s, why, "the data file cannot be opened (errno says why)");
  s->own_fd = 1;
  s->fd     = fd; /* closed by destroy even if src_on_fd refuses */
  return src_on_fd (s, fd, why);
}

/* Where `pn:0:` stops being LEN: the colon after a LEN that reads as 0, or
   NULL when @p data is not a pn stream. */
static const char *
pn_stream_rest (const char *data)
{
  if (strncmp (data, "pn:", 3) != 0)
    return NULL;
  const char *colon = strchr (data + 3, ':');
  uint64_t    len;
  if (!colon || dp_wfm_parse_u64 (data + 3, (size_t)(colon - data - 3), &len)
      || len != 0)
    return NULL;
  return colon;
}

/* `pn:0:REST` -- LEN 0 is a stream. The one parser reads the rest, through
   the same text with LEN 1, so a register, seed or poly it would refuse is
   refused here too. Returns 1 if @p data is a pn stream, 0 if not, -1 on a
   refusal (reason in `why`). */
static int
pn_stream (wfm_data_src_t *s, const char *data, const char **why)
{
  const char *colon = pn_stream_rest (data);
  if (!colon)
    return 0;
  const size_t n    = strlen (colon) + 5u;
  char        *text = dp_xmalloc (n);
  (void)snprintf (text, n, "pn:1%s", colon);
  wfm_field_t f;
  uint8_t    *owned = NULL;
  const int   rc    = dp_wfm_field_parse (text, &f, &owned, why);
  free (text);
  if (rc != 0)
    return -1;
  if (f.reps > 1)
    {
      say (why, "a stream has no end to repeat, so *REPS is refused");
      return -1;
    }
  s->pn = dp_wfm_seq_pn_create (&f.seq);
  if (!s->pn)
    {
      say (why, "no generator for this register");
      return -1;
    }
  s->kind      = SRC_PN;
  s->st.stream = 1;
  return 1;
}

uint64_t
dp_wfm_data_length_bits (const char *data, const char *path)
{
  if ((data != NULL) == (path != NULL))
    return 0;
  if (data)
    return pn_stream_rest (data)
               ? 0u
               : (uint64_t)dp_wfm_field_bits (data, NULL, 0, NULL);
  if (strcmp (path, "-") == 0)
    return 0;
  const int fd = fd_open (path);
  if (fd < 0)
    return 0;
  int                regular = 0;
  unsigned long long bytes   = 0;
  const int          ok      = fd_stat (fd, &regular, &bytes) == 0;
  fd_close (fd);
  return ok && regular ? (uint64_t)bytes * 8u : 0u;
}

wfm_data_src_t *
dp_wfm_data_create (const char *data, const char *path, size_t len,
                    const char *fill, const char **why)
{
  if ((data != NULL) == (path != NULL))
    {
      say (why, data ? "--data and --data-from-file both name the payload's "
                       "source; give one"
                     : "no data source: give --data or --data-from-file");
      return NULL;
    }
  if (len == 0)
    {
      say (why, "a data field carries bits: LEN must be > 0");
      return NULL;
    }

  wfm_data_src_t *s = alloc_src (len, fill, why);
  if (!s)
    return NULL;
  if (path)
    return src_on_path (s, path, why);
  const int ps = pn_stream (s, data, why);
  if (ps < 0)
    return refuse (s, why, NULL);
  if (ps == 0)
    {
      s->kind  = SRC_FIELD;
      s->nbits = field_to_bits (data, &s->bits, why);
      if (s->nbits == 0)
        return refuse (s, why, NULL);
      s->st.total_bits = s->nbits;
    }
  return check_length (s, why);
}

wfm_data_src_t *
dp_wfm_data_create_fd (int fd, size_t len, const char *fill, const char **why)
{
  if (len == 0)
    {
      say (why, "a data field carries bits: LEN must be > 0");
      return NULL;
    }
  wfm_data_src_t *s = alloc_src (len, fill, why);
  if (!s)
    return NULL;
  return src_on_fd (s, fd, why);
}

wfm_data_src_t *
dp_wfm_data_create_seq (const wfm_seq_t *data, const char *path, size_t len,
                        const wfm_seq_t *fill, const char **why)
{
  const int have = data && data->len;
  if (have == (path != NULL))
    {
      say (why, have ? "--data and --data-from-file both name the payload's "
                       "source; give one"
                     : "no data source: give --data or --data-from-file");
      return NULL;
    }
  if (len == 0)
    {
      say (why, "a data field carries bits: LEN must be > 0");
      return NULL;
    }
  uint8_t *f  = NULL;
  size_t   nf = 0;
  if (fill && fill->len)
    {
      nf = seq_to_bits (fill, &f, why);
      if (nf == 0)
        return NULL;
    }
  wfm_data_src_t *s = alloc_src_bits (len, f, nf);
  if (path)
    return src_on_path (s, path, why);
  s->kind  = SRC_FIELD;
  s->nbits = seq_to_bits (data, &s->bits, why);
  if (s->nbits == 0)
    return refuse (s, why, NULL);
  s->st.total_bits = s->nbits;
  return check_length (s, why);
}

void
dp_wfm_data_destroy (wfm_data_src_t *s)
{
  if (!s)
    return;
  if (s->own_fd && s->fd >= 0)
    fd_close (s->fd);
  if (s->pn)
    dp_pn_destroy (s->pn);
  free (s->bits);
  free (s->res);
  free (s->oct);
  free (s->fill);
  free (s);
}

/* The fill, tiled from its first bit over out[0, n). */
static void
tile_fill (const wfm_data_src_t *s, uint8_t *out, size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = s->fill[i % s->nfill];
}

/* Read until `res` holds a chunk, the input ends, or (with a timeout) the
   pipe has nothing more. Returns 0, or -1 on a read error. */
static int
fill_residue (wfm_data_src_t *s, int timeout_ms)
{
  int wait = timeout_ms;
  while (s->have < s->len && !s->eof)
    {
      if (wait >= 0)
        {
          const int r = fd_wait (s->fd, wait);
          if (r < 0)
            return -1;
          if (r == 0)
            return 0; /* nothing yet; what was read stays in `res` */
          wait = 0;   /* the timeout bounds the WAIT, not every read */
        }
      const size_t    need = (s->len - s->have + 7u) / 8u;
      const long long got  = fd_read (s->fd, s->oct, need);
      if (got < 0 && errno == EINTR)
        continue;
      if (got < 0)
        return -1;
      if (got == 0)
        {
          s->eof = 1;
          break;
        }
      s->st.hash = dp_hash64 (s->st.hash, s->oct, (size_t)got);
      (void)dp_bytes_to_bin (s->oct, (size_t)got, s->res + s->have,
                             8u * (size_t)got, DP_BITORDER_BIG);
      s->have += 8u * (size_t)got;
    }
  return 0;
}

/* One chunk into out[0, len): the source's own step. Returns FRAME with
   `*got` source bits (< len only for the padded last), END or NOT_YET. */
static wfm_data_status_t
draw (wfm_data_src_t *s, uint8_t *out, size_t *got, int timeout_ms)
{
  switch (s->kind)
    {
    case SRC_PN:
      *got = dp_pn_generate (s->pn, s->len, out, s->len);
      return *got == s->len ? WFM_DATA_FRAME : WFM_DATA_ERROR;

    case SRC_FIELD:
      if (s->pos >= s->nbits)
        return WFM_DATA_END;
      *got = s->nbits - s->pos < s->len ? s->nbits - s->pos : s->len;
      memcpy (out, s->bits + s->pos, *got);
      s->pos += *got;
      return WFM_DATA_FRAME;

    case SRC_FD:
      if (fill_residue (s, timeout_ms) != 0)
        return WFM_DATA_ERROR;
      if (s->have < s->len && !s->eof)
        return WFM_DATA_NOT_YET;
      if (s->have == 0)
        return WFM_DATA_END;
      /* A short last chunk needs a fill; create saw to that for every
         source whose length it knew, so this is a file that shrank after
         its fstat -- an error, not a pad nobody declared. */
      if (s->have < s->len && !s->fill)
        return WFM_DATA_ERROR;
      *got = s->have < s->len ? s->have : s->len;
      memcpy (out, s->res, *got);
      memmove (s->res, s->res + *got, s->have - *got);
      s->have -= *got;
      return WFM_DATA_FRAME;
    }
  return WFM_DATA_ERROR;
}

/* One chunk, written `reps` times: a repeated data field is one draw. */
static void
repeat (uint8_t *out, size_t len, size_t reps)
{
  for (size_t r = 1; r < reps; r++)
    memcpy (out + r * len, out, len);
}

wfm_data_status_t
dp_wfm_data_next (wfm_data_src_t *s, size_t reps, uint8_t *out, size_t max_out,
                  int timeout_ms)
{
  if (reps == 0)
    reps = 1;
  if (!s || !out || s->len > max_out / reps)
    return WFM_DATA_ERROR;
  if (s->ended)
    return WFM_DATA_END;

  size_t                  got = 0;
  const wfm_data_status_t st  = draw (s, out, &got, timeout_ms);
  if (st != WFM_DATA_FRAME)
    return st;

  /* The last chunk is padded -- create refused a source that would need a
     fill it does not have -- and it IS the last: nothing follows it. */
  if (got < s->len)
    {
      tile_fill (s, out + got, s->len - got);
      s->st.pad_bits = s->len - got;
      s->ended       = 1;
    }
  s->st.bits += got;
  s->st.frames++;
  repeat (out, s->len, reps);
  return WFM_DATA_FRAME;
}

wfm_data_status_t
dp_wfm_data_idle (wfm_data_src_t *s, size_t reps, uint8_t *out, size_t max_out)
{
  if (reps == 0)
    reps = 1;
  if (!s || !out || !s->fill || s->len > max_out / reps)
    return WFM_DATA_ERROR;
  tile_fill (s, out, s->len);
  repeat (out, s->len, reps);
  s->st.idle_frames++;
  return WFM_DATA_FRAME;
}

wfm_data_status_t
dp_wfm_data_frame (wfm_data_src_t *s, wfm_data_pacing_t pacing, size_t reps,
                   uint8_t *out, size_t max_out)
{
  const int               paced = pacing == WFM_DATA_PACED;
  const wfm_data_status_t st
      = dp_wfm_data_next (s, reps, out, max_out, paced ? 0 : -1);
  if (st != WFM_DATA_NOT_YET)
    return st;
  return dp_wfm_data_idle (s, reps, out, max_out) == WFM_DATA_FRAME
             ? WFM_DATA_IDLE
             : WFM_DATA_ERROR;
}

void
dp_wfm_data_stats (const wfm_data_src_t *s, wfm_data_stats_t *out)
{
  if (!out)
    return;
  if (!s)
    {
      memset (out, 0, sizeof *out);
      return;
    }
  *out = s->st;
}

/* ── state (dp_state.h) ─────────────────────────────────────────────────
   [hdr][kind ended eof 0x5][len][total_bits][frames idle pad bits][kind's]
   where the kind's own part is a Field's cursor, `pn:0`'s register as a
   nested dp_pn blob, or a file's running hash, residue count and residue
   (len + 8 one-bit bytes, zero past the count, so the size is config). The
   octet offset is not stored: every read is whole octets, so it is
   (bits + have) / 8 by construction. */

/* A pipe cannot resume: the octets it delivered are gone. pn:0 is a stream
   too, but its next bit is its register's, so it resumes. */
static int
is_pipe (const wfm_data_src_t *s)
{
  return s->kind == SRC_FD && s->st.stream;
}

const char *
dp_wfm_data_state_refusal (const wfm_data_src_t *s)
{
  if (s && is_pipe (s))
    return "a pipe cannot resume: the octets it has delivered are gone, so "
           "a blob could only restart it from wherever the pipe is now";
  return NULL;
}

/* The bytes ahead of the kind's own part. */
#define DATA_STATE_HEAD (sizeof (dp_state_hdr_t) + 8u + 6u * sizeof (uint64_t))

/* The residue region: at most len + 7 bits are ever held (src_on_fd). */
static size_t
res_cap (const wfm_data_src_t *s)
{
  return s->len + 8u;
}

size_t
dp_wfm_data_state_bytes (const wfm_data_src_t *s)
{
  if (!s || is_pipe (s))
    return 0;
  switch (s->kind)
    {
    case SRC_FIELD:
      return DATA_STATE_HEAD + sizeof (uint64_t);
    case SRC_PN:
      return DATA_STATE_HEAD + dp_pn_state_bytes (s->pn);
    case SRC_FD:
      return DATA_STATE_HEAD + 2u * sizeof (uint64_t) + res_cap (s);
    }
  return 0;
}

void
dp_wfm_data_get_state (const wfm_data_src_t *s, void *blob)
{
  const size_t bytes = dp_wfm_data_state_bytes (s);
  if (bytes == 0)
    return; /* refused: state_bytes said 0 */
  DP_GET_OPEN (WFM_DATA_STATE_MAGIC, WFM_DATA_STATE_VERSION, bytes);
  const uint8_t flags[8]
      = { (uint8_t)s->kind, (uint8_t)(s->ended != 0), (uint8_t)(s->eof != 0) };
  dp_w_bytes (&_w, flags, sizeof flags);
  dp_w_u64 (&_w, s->len);
  dp_w_u64 (&_w, s->st.total_bits);
  dp_w_u64 (&_w, s->st.frames);
  dp_w_u64 (&_w, s->st.idle_frames);
  dp_w_u64 (&_w, s->st.pad_bits);
  dp_w_u64 (&_w, s->st.bits);
  switch (s->kind)
    {
    case SRC_FIELD:
      dp_w_u64 (&_w, s->pos);
      break;
    case SRC_PN:
      DP_W_CHILD (&_w, dp_pn, s->pn);
      break;
    case SRC_FD:
      {
        dp_w_u64 (&_w, s->st.hash);
        dp_w_u64 (&_w, s->have);
        uint8_t *r = dp_w_reserve (&_w, res_cap (s));
        if (r)
          {
            memcpy (r, s->res, s->have);
            memset (r + s->have, 0, res_cap (s) - s->have);
          }
        break;
      }
    }
}

/* The dp_hash64 of a file's first @p n octets, read from its start; the
   read position is left at @p n. 0, or -1 for a read error or a file
   shorter than @p n. */
static int
hash_prefix (int fd, uint64_t n, uint64_t *h)
{
  if (fd_seek (fd, 0) != 0)
    return -1;
  uint8_t  buf[4096];
  uint64_t acc = DP_HASH64_INIT;
  while (n)
    {
      const size_t    want = n < sizeof buf ? (size_t)n : sizeof buf;
      const long long got  = fd_read (fd, buf, want);
      if (got < 0 && errno == EINTR)
        continue;
      if (got <= 0)
        return -1;
      acc = dp_hash64 (acc, buf, (size_t)got);
      n -= (uint64_t)got;
    }
  *h = acc;
  return 0;
}

int
dp_wfm_data_file_identity (const char *path, uint64_t *bits, uint64_t *h)
{
  if (!path || strcmp (path, "-") == 0)
    return -1;
  const int fd = fd_open (path);
  if (fd < 0)
    return -1;
  int                regular = 0;
  unsigned long long bytes   = 0;
  /* The same reader a restore checks its prefix with, over the whole file:
     one way to hash a file, so the record and the checkpoint agree. */
  const int rc = fd_stat (fd, &regular, &bytes) == 0 && regular
                         && hash_prefix (fd, (uint64_t)bytes, h) == 0
                     ? 0
                     : -1;
  fd_close (fd);
  if (rc == 0 && bits)
    *bits = (uint64_t)bytes * 8u;
  return rc;
}

/* Put a file where a blob's position says, checking that its prefix is
   the one the blob read: 0, or -1 with the read position where it was. */
static int
seek_checked (wfm_data_src_t *s, uint64_t octets, uint64_t hash)
{
  const long long was = fd_tell (s->fd);
  uint64_t        h   = 0;
  if (was < 0)
    return -1;
  if (hash_prefix (s->fd, octets, &h) != 0 || h != hash)
    {
      (void)fd_seek (s->fd, was);
      return -1;
    }
  return 0;
}

int
dp_wfm_data_set_state (wfm_data_src_t *s, const void *blob)
{
  if (!s || !blob || is_pipe (s))
    return DP_ERR_INVALID;
  DP_SET_OPEN (WFM_DATA_STATE_MAGIC, WFM_DATA_STATE_VERSION,
               dp_wfm_data_state_bytes (s));
  uint8_t flags[8];
  dp_r_bytes (&_r, flags, sizeof flags);
  const uint64_t   len   = dp_r_u64 (&_r);
  const uint64_t   total = dp_r_u64 (&_r);
  wfm_data_stats_t st    = s->st; /* config members kept: total, stream */
  st.frames              = dp_r_u64 (&_r);
  st.idle_frames         = dp_r_u64 (&_r);
  st.pad_bits            = dp_r_u64 (&_r);
  st.bits                = dp_r_u64 (&_r);
  /* The same kind over the same data in the same LEN, or the blob is
     another source's: refused, never reinterpreted. */
  if (flags[0] != (uint8_t)s->kind || len != s->len
      || total != s->st.total_bits || flags[1] > 1 || flags[2] > 1)
    return DP_ERR_INVALID;

  /* The kind's own part is validated whole before anything changes. */
  uint64_t pos = 0, have = 0;
  switch (s->kind)
    {
    case SRC_FIELD:
      pos = dp_r_u64 (&_r);
      if (pos > s->nbits || st.bits != pos)
        return DP_ERR_INVALID;
      break;
    case SRC_PN:
      /* The last check: the child validates its own envelope and changes
         nothing when it refuses. */
      DP_R_CHILD (&_r, dp_pn, s->pn);
      break;
    case SRC_FD:
      {
        st.hash          = dp_r_u64 (&_r);
        have             = dp_r_u64 (&_r);
        const uint8_t *r = dp_r_reserve (&_r, res_cap (s));
        if (!r || have > s->len + 7u || (st.bits + have) % 8u
            || st.bits + have > total
            || seek_checked (s, (st.bits + have) / 8u, st.hash) != 0)
          return DP_ERR_INVALID;
        memcpy (s->res, r, (size_t)have);
        break;
      }
    }
  s->pos   = (size_t)pos;
  s->have  = (size_t)have;
  s->ended = flags[1];
  s->eof   = flags[2];
  s->st    = st;
  return DP_OK;
}
