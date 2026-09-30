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
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void
say (char *why, size_t cap, const char *fmt, ...)
{
  if (!why || cap == 0)
    return;
  va_list ap;
  va_start (ap, fmt);
  (void)vsnprintf (why, cap, fmt, ap);
  va_end (ap);
}

static wfm_data_src_t *
refuse (wfm_data_src_t *s, char *why, size_t cap, const char *msg)
{
  say (why, cap, "%s", msg);
  dp_wfm_data_destroy (s);
  return NULL;
}

/* A Field's bits through the one door (dp_wfm_field_bits), which refuses
   data:LEN by name. Returns the count, or 0 with the reason in `why`. */
static size_t
field_to_bits (const char *spec, uint8_t **out, char *why, size_t cap,
               const char *what)
{
  const char  *r = NULL;
  const size_t n = dp_wfm_field_bits (spec, NULL, 0, &r);
  if (n == 0)
    {
      say (why, cap, "%s %s: %s", what, spec, r ? r : "not a Field");
      return 0;
    }
  *out = dp_xmalloc (n);
  (void)dp_wfm_field_bits (spec, *out, n, NULL);
  return n;
}

/* Every refusal decidable before the first sample (§4.3, §4.4), once. */
static wfm_data_src_t *
check_length (wfm_data_src_t *s, char *why, size_t cap)
{
  if (s->st.stream)
    {
      if (s->kind == SRC_FD && !s->fill)
        return refuse (s, why, cap,
                       "a stream's length is unknown until it ends, so it "
                       "needs a fill (--fill) for its last frame and for "
                       "idle frames");
      return s;
    }
  const uint64_t n = s->st.total_bits;
  if (n == 0)
    return refuse (s, why, cap, "the data is empty: a burst of no frames");
  const uint64_t tail = n % s->len;
  if (tail && !s->fill)
    {
      say (why, cap,
           "the data is %llu bits: its last %zu-bit frame holds %llu and "
           "is %llu bits short, and no fill (--fill) is declared",
           (unsigned long long)n, s->len, (unsigned long long)tail,
           (unsigned long long)(s->len - tail));
      dp_wfm_data_destroy (s);
      return NULL;
    }
  return s;
}

static wfm_data_src_t *
alloc_src (size_t len, const char *fill, char *why, size_t cap)
{
  wfm_data_src_t *s = dp_xcalloc (1, sizeof *s);
  s->len            = len;
  s->fd             = -1;
  if (fill)
    {
      s->nfill = field_to_bits (fill, &s->fill, why, cap, "--fill");
      if (s->nfill == 0)
        return refuse (s, NULL, 0, NULL);
    }
  return s;
}

/* `pn:0:REST` -- LEN 0 is a stream. The one parser reads the rest, through
   the same text with LEN 1, so a register, seed or poly it would refuse is
   refused here too. Returns 1 if @p data is a pn stream, 0 if not, -1 on a
   refusal (reason in `why`). */
static int
pn_stream (wfm_data_src_t *s, const char *data, char *why, size_t cap)
{
  if (strncmp (data, "pn:", 3) != 0)
    return 0;
  const char *colon = strchr (data + 3, ':');
  uint64_t    len;
  if (!colon || dp_wfm_parse_u64 (data + 3, (size_t)(colon - data - 3), &len)
      || len != 0)
    return 0;

  const size_t n    = strlen (colon) + 5u;
  char        *text = dp_xmalloc (n);
  (void)snprintf (text, n, "pn:1%s", colon);
  wfm_field_t f;
  uint8_t    *owned = NULL;
  const char *r     = NULL;
  const int   rc    = dp_wfm_field_parse (text, &f, &owned, &r);
  free (text);
  if (rc != 0)
    {
      say (why, cap, "--data %s: %s", data, r);
      return -1;
    }
  if (f.reps > 1)
    {
      say (why, cap,
           "--data %s: a stream has no end to repeat, so *REPS "
           "is refused",
           data);
      return -1;
    }
  s->pn = dp_wfm_seq_pn_create (&f.seq);
  if (!s->pn)
    {
      say (why, cap, "--data %s: no generator for this register", data);
      return -1;
    }
  s->kind      = SRC_PN;
  s->st.stream = 1;
  return 1;
}

wfm_data_src_t *
dp_wfm_data_create (const char *data, const char *path, size_t len,
                    const char *fill, char *why, size_t why_cap)
{
  if ((data != NULL) == (path != NULL))
    {
      say (why, why_cap,
           data ? "--data and --data-from-file both name the payload's "
                  "source; give one"
                : "no data source: give --data or --data-from-file");
      return NULL;
    }
  if (len == 0)
    {
      say (why, why_cap, "a data field carries bits: LEN must be > 0");
      return NULL;
    }

  if (path)
    {
      if (strcmp (path, "-") == 0)
        return dp_wfm_data_create_fd (STDIN_FILENO, len, fill, why, why_cap);
      const int fd = open (path, O_RDONLY);
      if (fd < 0)
        {
          say (why, why_cap, "--data-from-file %s: %s", path,
               strerror (errno));
          return NULL;
        }
      wfm_data_src_t *s = dp_wfm_data_create_fd (fd, len, fill, why, why_cap);
      if (!s)
        close (fd);
      else
        s->own_fd = 1;
      return s;
    }

  wfm_data_src_t *s = alloc_src (len, fill, why, why_cap);
  if (!s)
    return NULL;
  const int ps = pn_stream (s, data, why, why_cap);
  if (ps < 0)
    return refuse (s, NULL, 0, NULL);
  if (ps == 0)
    {
      s->kind  = SRC_FIELD;
      s->nbits = field_to_bits (data, &s->bits, why, why_cap, "--data");
      if (s->nbits == 0)
        return refuse (s, NULL, 0, NULL);
      s->st.total_bits = s->nbits;
    }
  return check_length (s, why, why_cap);
}

wfm_data_src_t *
dp_wfm_data_create_fd (int fd, size_t len, const char *fill, char *why,
                       size_t why_cap)
{
  if (len == 0)
    {
      say (why, why_cap, "a data field carries bits: LEN must be > 0");
      return NULL;
    }
  struct stat sb;
  if (fd < 0 || fstat (fd, &sb) != 0)
    {
      say (why, why_cap, "the data source cannot be read: %s",
           strerror (errno));
      return NULL;
    }
  wfm_data_src_t *s = alloc_src (len, fill, why, why_cap);
  if (!s)
    return NULL;
  s->kind      = SRC_FD;
  s->fd        = fd;
  s->st.hashed = 1;
  s->st.hash   = DP_HASH64_INIT;
  s->res       = dp_xmalloc (len + 8u);
  s->oct       = dp_xmalloc (len / 8u + 2u);
  if (S_ISREG (sb.st_mode))
    s->st.total_bits = (uint64_t)sb.st_size * 8u;
  else
    s->st.stream = 1;
  return check_length (s, why, why_cap);
}

void
dp_wfm_data_destroy (wfm_data_src_t *s)
{
  if (!s)
    return;
  if (s->own_fd && s->fd >= 0)
    close (s->fd);
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
          struct pollfd p = { .fd = s->fd, .events = POLLIN };
          const int     r = poll (&p, 1, wait);
          if (r < 0 && errno == EINTR)
            continue;
          if (r < 0)
            return -1;
          if (r == 0)
            return 0; /* nothing yet; what was read stays in `res` */
          wait = 0;   /* the timeout bounds the WAIT, not every read */
        }
      const size_t  need = (s->len - s->have + 7u) / 8u;
      const ssize_t got  = read (s->fd, s->oct, need);
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
