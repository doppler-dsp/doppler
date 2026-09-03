/**
 * @file dp_event_log_core.c
 * @brief Event log: JSON Lines while the run is live, SigMF when it is over.
 *
 * The whole object is the two halves of dp_event_log_core.h's contract: an
 * append that renders ONE annotation and flushes it, and a finalize that
 * hands the accumulated lines to the writer's SigMF emitter. There is no
 * third thing here, and deliberately no second JSON document builder --
 * `global` and `captures` are wfm_sigmf_meta_json_ex()'s to spell.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* fstat: a device is not an event log */

#include "cJSON.h"
#include "dp_event_log/dp_event_log_core.h"
#include "dp_tlm/dp_tlm_core.h" /* DP_TLM_REC_DTYPE_JSON -- one dtype */
#include "wfm_writer/wfm_writer_core.h"

/* One staged field. `is_str` picks the arm; the two payloads are separate
   rather than a union so a name reused across kinds cannot read the wrong
   one, and the whole table is 16 of these -- 1.6 kB, allocated once with the
   log and never again (design 11.5: nothing allocates per event). */
typedef struct
{
  char   name[DP_EVENT_LOG_NAME_MAX];
  char   str[DP_EVENT_LOG_STR_MAX];
  double num;
  int    is_str;
} dp_event_field_t;

struct dp_event_log
{
  FILE            *fp;
  char            *path;
  char            *dataset;   /* set once per run; NULL means metadata_only */
  char            *telemetry; /* set once per run; NULL means no record file */
  double           fc;
  size_t           count;
  size_t           n_fields;
  int              err; /* sticky: DP_OK, or the first failure of the run */
  dp_event_field_t fields[DP_EVENT_LOG_MAX_FIELDS];
};

/* ── construction ───────────────────────────────────────────────────────── */

dp_event_log_t *
dp_event_log_open (const char *path, double fc)
{
  if (!path || !*path)
    return NULL;
  /* dp_xcalloc / dp_xmalloc: fixed and path-sized, arguments already
     validated, so the only failure is genuine OOM -- which the library-wide
     convention aborts on rather than threading an unwind path no test can
     reach (clib_common.h). */
  dp_event_log_t *log = (dp_event_log_t *)dp_xcalloc (1, sizeof *log);
  size_t          n   = strlen (path) + 1u;
  log->path           = (char *)dp_xmalloc (n);
  memcpy (log->path, path, n);
  /* "w", not "a": a log names one run. Appending to a previous run's file
     would merge two runs into one sidecar with nothing in it to tell them
     apart -- and the recovery case that wants the old file does not want a
     writer at all, it wants dp_event_log_write_meta(). */
  log->fp = fopen (path, "w");
  if (!log->fp)
    {
      /* Reachable and the common case -- an unwritable directory, a bad
         name -- so it unwinds and returns NULL, where the allocations
         above abort. */
      free (log->path);
      free (log);
      return NULL;
    }
  log->fc  = fc;
  log->err = DP_OK;
  return log;
}

int
dp_event_log_close (dp_event_log_t *log)
{
  if (!log)
    return DP_ERR_INVALID;
  if (log->fp)
    {
      /* ferror() first, for the reason wfm_writer_close() documents: a
         rejected write leaves nothing buffered, so the fclose that follows
         reports success over the top of it. */
      if (ferror (log->fp))
        log->err = DP_ERR_SEND;
      if (fclose (log->fp) != 0)
        log->err = DP_ERR_SEND;
      log->fp = NULL;
    }
  return log->err;
}

int
dp_event_log_destroy (dp_event_log_t *log)
{
  if (!log)
    return DP_OK;
  int rc = dp_event_log_close (log);
  free (log->path);
  free (log->dataset);
  free (log->telemetry);
  free (log);
  return rc;
}

size_t
dp_event_log_count (const dp_event_log_t *log)
{
  return log ? log->count : 0u;
}

/* Replace one owned, optional run-scoped string. Empty and NULL both mean
   "none", because a caller clearing a name and a caller never setting one are
   asking for the same document. */
static void
set_owned (char **slot, const char *value)
{
  free (*slot);
  *slot = NULL;
  if (!value || !*value)
    return;
  size_t n = strlen (value) + 1u;
  *slot    = (char *)dp_xmalloc (n);
  memcpy (*slot, value, n);
}

int
dp_event_log_set_dataset (dp_event_log_t *log, const char *name)
{
  if (!log)
    return DP_ERR_INVALID;
  set_owned (&log->dataset, name);
  return DP_OK;
}

int
dp_event_log_set_telemetry (dp_event_log_t *log, const char *path)
{
  if (!log)
    return DP_ERR_INVALID;
  set_owned (&log->telemetry, path);
  return DP_OK;
}

/* ── staged fields ──────────────────────────────────────────────────────── */

/* Shared entry test for both field faces: one name check, one table check,
   and the slot they both fill. Returns NULL when the field is refused. */
static dp_event_field_t *
stage (dp_event_log_t *log, const char *name)
{
  if (!log || !name || !*name)
    return NULL;
  if (strlen (name) >= DP_EVENT_LOG_NAME_MAX)
    return NULL;
  if (log->n_fields >= DP_EVENT_LOG_MAX_FIELDS)
    return NULL;
  dp_event_field_t *f = &log->fields[log->n_fields];
  memset (f, 0, sizeof *f);
  /* Length-checked above, so this cannot truncate. */
  memcpy (f->name, name, strlen (name) + 1u);
  return f;
}

int
dp_event_log_field (dp_event_log_t *log, const char *name, double value)
{
  /* JSON has no NaN and no Infinity. cJSON renders either as `null`, which a
     strict parser accepts and a reader then has to special-case -- so an
     unmeasured quantity is REFUSED here and simply absent from the event,
     which is the same answer the rest of this file gives. */
  if (!isfinite (value))
    return DP_ERR_INVALID;
  dp_event_field_t *f = stage (log, name);
  if (!f)
    return DP_ERR_INVALID;
  f->num    = value;
  f->is_str = 0;
  log->n_fields++;
  return DP_OK;
}

int
dp_event_log_field_str (dp_event_log_t *log, const char *name,
                        const char *value)
{
  if (!value || strlen (value) >= DP_EVENT_LOG_STR_MAX)
    return DP_ERR_INVALID;
  dp_event_field_t *f = stage (log, name);
  if (!f)
    return DP_ERR_INVALID;
  memcpy (f->str, value, strlen (value) + 1u);
  f->is_str = 1;
  log->n_fields++;
  return DP_OK;
}

/* ── append ─────────────────────────────────────────────────────────────── */

/* Render one annotation. Split out from the append so the write path holds
   exactly the file handling and this holds exactly the document. */
static cJSON *
build_annotation (const dp_event_log_t *log, uint64_t sample_start,
                  uint64_t sample_count, const char *label, double freq_hz,
                  double bandwidth_hz)
{
  cJSON *a = cJSON_CreateObject ();
  if (!a)
    return NULL;
  cJSON_AddNumberToObject (a, "core:sample_start", (double)sample_start);
  /* 0 is an INSTANT, and the key is omitted for it. A transition happens at
     a sample; writing `"core:sample_count": 0` would state a measured span,
     and a span of zero samples is not what a state change means. */
  if (sample_count > 0u)
    cJSON_AddNumberToObject (a, "core:sample_count", (double)sample_count);
  if (label && *label)
    cJSON_AddStringToObject (a, "core:label", label);
  /* The band, and the two-step rule the header states: the OFFSET and the
     width are what the caller knows and are always recorded; the absolute
     edges need the channel centre, so they appear only when it is known.
     Nothing is guessed and nothing the caller knew is dropped. */
  if (bandwidth_hz > 0.0 && isfinite (bandwidth_hz) && isfinite (freq_hz))
    {
      cJSON_AddNumberToObject (a, "doppler:freq_hz", freq_hz);
      cJSON_AddNumberToObject (a, "doppler:bandwidth_hz", bandwidth_hz);
      if (log->fc != 0.0)
        {
          cJSON_AddNumberToObject (a, "core:freq_lower_edge",
                                   log->fc + freq_hz - bandwidth_hz / 2.0);
          cJSON_AddNumberToObject (a, "core:freq_upper_edge",
                                   log->fc + freq_hz + bandwidth_hz / 2.0);
        }
    }
  for (size_t i = 0; i < log->n_fields; i++)
    {
      const dp_event_field_t *f = &log->fields[i];
      char                    key[DP_EVENT_LOG_NAME_MAX + 9];
      snprintf (key, sizeof key, "doppler:%s", f->name);
      if (f->is_str)
        cJSON_AddStringToObject (a, key, f->str);
      else
        cJSON_AddNumberToObject (a, key, f->num);
    }
  return a;
}

int
dp_event_log_append (dp_event_log_t *log, uint64_t sample_start,
                     const char *label, uint64_t sample_count, double freq_hz,
                     double bandwidth_hz)
{
  if (!log || !log->fp)
    return DP_ERR_INVALID;

  cJSON *a = build_annotation (log, sample_start, sample_count, label, freq_hz,
                               bandwidth_hz);
  /* Cleared here, before any early return: the fields belong to THIS event,
     and an event that failed to reach the disk must not leak them into the
     next one -- which would attach a lost receiver's C/N0 to whatever
     happened next. */
  log->n_fields = 0;
  if (!a)
    return DP_ERR_MEMORY;

  char *line = cJSON_PrintUnformatted (a);
  cJSON_Delete (a);
  if (!line)
    return DP_ERR_MEMORY;
  /* The one number the reader also holds: a line this long is refused here
     so that no file this writer produced can ever be refused there. */
  if (strlen (line) >= DP_EVENT_LOG_LINE_MAX)
    {
      free (line);
      return DP_ERR_INVALID;
    }

  int rc = DP_OK;
  if (fprintf (log->fp, "%s\n", line) < 0)
    rc = DP_ERR_SEND;
  /* Flushed per event, not per block. Events are transitions -- a handful a
     minute per receiver -- so the cost is nothing, and it is what makes the
     file tail-able live and what bounds a crash's loss to the event being
     written. */
  else if (fflush (log->fp) != 0)
    rc = DP_ERR_SEND;
  free (line);

  if (rc != DP_OK)
    {
      if (log->err == DP_OK)
        log->err = rc;
      return rc;
    }
  log->count++;
  return DP_OK;
}

/* ── finalize ───────────────────────────────────────────────────────────── */

/* The caller's `global` additions, as one JSON object for the emitter to
   merge. Built here because these three keys are the event log's business;
   the document they land in is not. */
static char *
build_extra_global (const char *dataset, const char *telemetry)
{
  cJSON *g = cJSON_CreateObject ();
  if (!g)
    return NULL;
  if (dataset && *dataset)
    cJSON_AddStringToObject (g, "core:dataset", dataset);
  else
    /* SigMF's own word for "these annotations index a stream nobody kept" --
       a live NATS feed. Saying it is better than an absent dataset key,
       which a reader has to interpret. */
    cJSON_AddBoolToObject (g, "core:metadata_only", 1);
  if (telemetry && *telemetry)
    {
      cJSON *t = cJSON_AddObjectToObject (g, "doppler:telemetry");
      if (t)
        {
          cJSON_AddStringToObject (t, "path", telemetry);
          cJSON *dt = cJSON_Parse (DP_TLM_REC_DTYPE_JSON);
          if (dt)
            cJSON_AddItemToObject (t, "dtype", dt);
        }
    }
  char *out = cJSON_PrintUnformatted (g);
  cJSON_Delete (g);
  return out;
}

/* Read the flat log into an array of NUL-terminated lines; `*n_out` counts
   the non-empty ones.  A line grows on demand up to DP_EVENT_LOG_LINE_MAX --
   the writer never emits one that long, so reaching it means this is not an
   event log, and the answer is NULL rather than a buffer that keeps doubling
   over whatever the file turns out to be.  Allocation goes through the
   abort-on-OOM helpers like the rest of this file; an unreadable file is the
   caller's business and is handled before this is reached. */
static char **
read_lines (FILE *f, size_t *n_out)
{
  char **lines = NULL;
  size_t n = 0, cap = 0;
  char  *buf  = NULL;
  size_t blen = 0, bcap = 0;
  int    c;

  for (;;)
    {
      c = fgetc (f);
      if (c != EOF && c != '\n')
        {
          if (blen + 1u >= DP_EVENT_LOG_LINE_MAX)
            {
              for (size_t i = 0; i < n; i++)
                free (lines[i]);
              free (lines);
              free (buf);
              *n_out = 0;
              return NULL;
            }
          if (blen + 1u >= bcap)
            {
              bcap = bcap ? bcap * 2u : 256u;
              buf  = (char *)dp_xrealloc (buf, bcap);
            }
          buf[blen++] = (char)c;
          continue;
        }
      if (blen > 0u)
        {
          buf[blen] = '\0';
          if (n >= cap)
            {
              cap   = cap ? cap * 2u : 64u;
              lines = (char **)dp_xrealloc (lines, cap * sizeof *lines);
            }
          lines[n] = (char *)dp_xmalloc (blen + 1u);
          memcpy (lines[n], buf, blen + 1u);
          n++;
          blen = 0;
        }
      if (c == EOF)
        break;
    }
  free (buf);
  *n_out = n;
  /* An empty log is a legitimate run: nothing happened. A one-element
     allocation rather than NULL keeps the caller's free loop uniform. */
  if (!lines)
    lines = (char **)dp_xcalloc (1, sizeof *lines);
  return lines;
}

int
dp_event_log_write_meta (const char *log_path, const char *meta_path,
                         int sample_type, int endian, double fs, double fc,
                         double t0_unix_sec, const char *dataset,
                         const char *telemetry)
{
  if (!log_path || !*log_path || !meta_path || !*meta_path)
    return DP_ERR_INVALID;

  FILE *f = fopen (log_path, "r");
  if (!f)
    return DP_ERR_SEND;
  /* Only a regular file can be an event log. A character device reads as an
     endless stream -- `/dev/full` is NUL bytes forever -- and the line cap
     below would catch that too, but a file that is not a file is refused on
     its type, before a byte of it is read. */
  struct stat st;
  if (fstat (fileno (f), &st) != 0 || !S_ISREG (st.st_mode))
    {
      fclose (f);
      return DP_ERR_INVALID;
    }
  size_t n     = 0;
  char **lines = read_lines (f, &n);
  fclose (f);
  if (!lines)
    return DP_ERR_INVALID;

  char *extra = build_extra_global (dataset, telemetry);
  char *json  = extra ? wfm_sigmf_meta_json_ex (sample_type, endian, fs, fc,
                                                t0_unix_sec, NULL, 0, extra,
                                                (const char *const *)lines, n)
                      : NULL;
  free (extra);
  for (size_t i = 0; i < n; i++)
    free (lines[i]);
  free (lines);
  if (!json)
    return DP_ERR_MEMORY;

  FILE *mf = fopen (meta_path, "w");
  int   rc = DP_ERR_SEND;
  if (mf)
    {
      rc = (fputs (json, mf) >= 0) ? DP_OK : DP_ERR_SEND;
      if (fclose (mf) != 0)
        rc = DP_ERR_SEND;
    }
  free (json);
  return rc;
}

int
dp_event_log_finalize (dp_event_log_t *log, const char *meta_path,
                       int sample_type, int endian, double fs,
                       double t0_unix_sec)
{
  if (!log)
    return DP_ERR_INVALID;
  /* The file on disk IS the input, so it has to be current before it is
     read. Every append already flushed; this covers a log closed early and
     costs nothing otherwise. */
  if (log->fp && fflush (log->fp) != 0)
    {
      if (log->err == DP_OK)
        log->err = DP_ERR_SEND;
      return DP_ERR_SEND;
    }
  return dp_event_log_write_meta (log->path, meta_path, sample_type, endian,
                                  fs, log->fc, t0_unix_sec, log->dataset,
                                  log->telemetry);
}
