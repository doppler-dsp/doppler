/*
 * test_dp_event_log_core.c — the event log's claims, one test per claim.
 *
 * The header (dp_event_log/dp_event_log_core.h) makes a small number of
 * promises that are easy to state and easy to break silently: a key omitted
 * rather than guessed, a staged field consumed by exactly one event, a
 * truncated last line that must not cost the run. Each has a check here, and
 * each check was proven by sabotage — see the report in the PR.
 *
 * Files are written into the test's cwd with a `dp_evlog_` prefix, the
 * convention test_wfm_reader_core.c already uses, and removed at the end.
 */
#include "dp_test.h"

#include "cJSON.h"
#include "dp_event_log/dp_event_log_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_PATH "dp_evlog_run.events"
#define META_PATH "dp_evlog_run.sigmf-meta"

/* Whole file as a NUL-terminated string; NULL if it cannot be read. */
static char *
slurp (const char *path)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  size_t cap = 4096, len = 0;
  char  *buf = (char *)malloc (cap);
  if (!buf)
    {
      fclose (f);
      return NULL;
    }
  for (;;)
    {
      if (len + 1024u >= cap)
        {
          cap *= 2u;
          char *nb = (char *)realloc (buf, cap);
          if (!nb)
            {
              free (buf);
              fclose (f);
              return NULL;
            }
          buf = nb;
        }
      size_t got = fread (buf + len, 1, 1024, f);
      len += got;
      if (got < 1024u)
        break;
    }
  buf[len] = '\0';
  fclose (f);
  return buf;
}

static size_t
count_lines (const char *s)
{
  size_t n = 0;
  for (; *s; s++)
    if (*s == '\n')
      n++;
  return n;
}

/* ── 1. the live file: one JSON object per line, flushed as it goes ─────── */
static int
test_jsonl_is_one_line_per_event (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 2.4e9);
  DP_REQUIRE_MSG (log != NULL, "open failed");

  DP_CHECK (dp_event_log_append (log, 100, "seeded", 0, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_append (log, 200, "tracking", 50, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_count (log) == 2u);

  /* Read WHILE the log is still open: the file is tail-able, which is only
     true if every append flushed. Without the flush this reads nothing. */
  char *live = slurp (LOG_PATH);
  DP_REQUIRE_MSG (live != NULL, "log unreadable while open");
  DP_CHECK_MSG (count_lines (live) == 2u,
                "the two events are not two lines in the live file");
  free (live);
  dp_event_log_destroy (log);
  return 0;
}

/* ── 2. omitted, never guessed: span, label, band ───────────────────────── */
static int
test_omits_what_it_was_not_told (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 0.0); /* fc unknown */
  DP_REQUIRE_MSG (log != NULL, "open failed");
  /* An instant with a band, on a stream whose centre nobody stated. */
  DP_CHECK (dp_event_log_append (log, 7, "lost", 0, 1.25e3, 4.0e6) == DP_OK);
  dp_event_log_destroy (log);

  char *s = slurp (LOG_PATH);
  DP_REQUIRE (s != NULL);
  DP_CHECK_MSG (strstr (s, "\"core:sample_start\":7") != NULL,
                "the position is the one key that is never optional");
  DP_CHECK_MSG (strstr (s, "core:sample_count") == NULL,
                "a zero span must be an omitted key, not a written 0");
  DP_CHECK_MSG (strstr (s, "\"core:label\":\"lost\"") != NULL,
                "the label is the event");
  /* The centre is unknown, so the ABSOLUTE edges cannot be stated -- but the
     offset and the width are what the caller knew, and they survive. */
  DP_CHECK_MSG (strstr (s, "core:freq_lower_edge") == NULL,
                "absolute edges without a centre frequency are a guess");
  DP_CHECK_MSG (strstr (s, "core:freq_upper_edge") == NULL,
                "absolute edges without a centre frequency are a guess");
  DP_CHECK_MSG (strstr (s, "\"doppler:freq_hz\":1250") != NULL,
                "the offset is known whether or not the centre is");
  DP_CHECK_MSG (strstr (s, "\"doppler:bandwidth_hz\":4000000") != NULL,
                "the width is known whether or not the centre is");
  free (s);

  /* No band stated at all: no frequency key of any kind. A stream gap has no
     frequency, and a written 0 Hz offset would read as an on-centre emitter.
   */
  log = dp_event_log_open (LOG_PATH, 2.4e9);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_append (log, 9, "gap", 128, 0.0, 0.0) == DP_OK);
  dp_event_log_destroy (log);
  s = slurp (LOG_PATH);
  DP_REQUIRE (s != NULL);
  DP_CHECK_MSG (strstr (s, "\"core:sample_count\":128") != NULL,
                "a stated span is written");
  DP_CHECK_MSG (strstr (s, "freq") == NULL,
                "no band stated must mean no frequency key at all");
  free (s);
  return 0;
}

/* ── 3. the edges, when the centre IS known ────────────────────────────── */
static int
test_edges_when_the_centre_is_known (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 2.4e9);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_append (log, 0, "seeded", 0, 1.0e4, 4.0e6) == DP_OK);
  dp_event_log_destroy (log);

  char *s = slurp (LOG_PATH);
  DP_REQUIRE (s != NULL);
  cJSON *a = cJSON_Parse (s);
  DP_REQUIRE_MSG (a != NULL, "the appended line is not JSON");
  cJSON *lo = cJSON_GetObjectItemCaseSensitive (a, "core:freq_lower_edge");
  cJSON *hi = cJSON_GetObjectItemCaseSensitive (a, "core:freq_upper_edge");
  DP_REQUIRE_MSG (lo && hi, "the edges are missing with a known centre");
  /* fc + offset -+ bw/2 = 2.4e9 + 1e4 -+ 2e6 */
  DP_CHECK (dp_near (lo->valuedouble, 2.4e9 + 1.0e4 - 2.0e6, 1e-3));
  DP_CHECK (dp_near (hi->valuedouble, 2.4e9 + 1.0e4 + 2.0e6, 1e-3));
  cJSON_Delete (a);
  free (s);
  return 0;
}

/* ── 4. staged fields: namespaced, and consumed by exactly one event ────── */
static int
test_fields_are_namespaced_and_consumed (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 0.0);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_field (log, "emitter", 3) == DP_OK);
  DP_CHECK (dp_event_log_field (log, "cn0_db_hz", 47.5) == DP_OK);
  DP_CHECK (dp_event_log_field_str (log, "state", "tracking") == DP_OK);
  DP_CHECK (dp_event_log_append (log, 10, "seeded", 0, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_append (log, 20, "released", 0, 0.0, 0.0) == DP_OK);
  dp_event_log_destroy (log);

  char *s = slurp (LOG_PATH);
  DP_REQUIRE (s != NULL);
  char *nl = strchr (s, '\n');
  DP_REQUIRE_MSG (nl != NULL, "expected two lines");
  *nl           = '\0';
  cJSON *first  = cJSON_Parse (s);
  cJSON *second = cJSON_Parse (nl + 1);
  DP_REQUIRE (first && second);

  cJSON *em = cJSON_GetObjectItemCaseSensitive (first, "doppler:emitter");
  DP_REQUIRE_MSG (em != NULL, "a staged field must reach the event");
  DP_CHECK (dp_near (em->valuedouble, 3.0, 1e-12));
  /* An integral value prints as an integer: an emitter id is not 3.0. */
  DP_CHECK_MSG (strstr (s, "\"doppler:emitter\":3") != NULL,
                "an integral field must not render as a float");
  cJSON *st = cJSON_GetObjectItemCaseSensitive (first, "doppler:state");
  DP_REQUIRE (st != NULL && cJSON_IsString (st));
  DP_CHECK (strcmp (st->valuestring, "tracking") == 0);
  cJSON *cn = cJSON_GetObjectItemCaseSensitive (first, "doppler:cn0_db_hz");
  DP_REQUIRE (cn != NULL);
  DP_CHECK (dp_near (cn->valuedouble, 47.5, 1e-9));

  /* Consumed: the SECOND event carries none of them. A leak here would
     attach a lost receiver's C/N0 to whatever happened next. */
  DP_CHECK_MSG (cJSON_GetObjectItemCaseSensitive (second, "doppler:emitter")
                    == NULL,
                "staged fields leaked into the next event");
  DP_CHECK_MSG (cJSON_GetObjectItemCaseSensitive (second, "doppler:state")
                    == NULL,
                "staged fields leaked into the next event");
  cJSON_Delete (first);
  cJSON_Delete (second);
  free (s);
  return 0;
}

/* ── 5. what a field REFUSES ────────────────────────────────────────────── */
static int
test_field_rejects (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 0.0);
  DP_REQUIRE (log != NULL);

  char longname[DP_EVENT_LOG_NAME_MAX + 8];
  memset (longname, 'x', sizeof longname - 1u);
  longname[sizeof longname - 1u] = '\0';
  char longval[DP_EVENT_LOG_STR_MAX + 8];
  memset (longval, 'y', sizeof longval - 1u);
  longval[sizeof longval - 1u] = '\0';

  DP_CHECK (dp_event_log_field (NULL, "a", 1.0) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_field (log, NULL, 1.0) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_field (log, "", 1.0) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_field (log, longname, 1.0) == DP_ERR_INVALID);
  /* JSON has no NaN: an unmeasured quantity is refused, not written as
     `null` for every reader to special-case. */
  DP_CHECK (dp_event_log_field (log, "cn0", 0.0 / 0.0) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_field (log, "cn0", 1.0 / 0.0) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_field_str (log, "s", NULL) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_field_str (log, "s", longval) == DP_ERR_INVALID);

  /* Nothing above was staged, so the table is empty and exactly
     DP_EVENT_LOG_MAX_FIELDS more fit; the next one is refused rather than
     dropped. */
  char nm[8];
  for (int i = 0; i < DP_EVENT_LOG_MAX_FIELDS; i++)
    {
      snprintf (nm, sizeof nm, "f%d", i);
      DP_CHECK (dp_event_log_field (log, nm, (double)i) == DP_OK);
    }
  DP_CHECK_MSG (dp_event_log_field (log, "one_too_many", 0.0)
                    == DP_ERR_INVALID,
                "a full staging table must refuse, not drop");
  DP_CHECK (dp_event_log_append (log, 1, "full", 0, 0.0, 0.0) == DP_OK);
  /* And the table is empty again afterwards. */
  DP_CHECK (dp_event_log_field (log, "after", 1.0) == DP_OK);
  dp_event_log_destroy (log);
  return 0;
}

/* ── 6. the sidecar: the writer's emitter, with our annotations ─────────── */
static int
test_finalize_writes_a_sigmf_sidecar (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 2.4e9);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_set_dataset (log, "capture.sigmf-data") == DP_OK);
  DP_CHECK (dp_event_log_set_telemetry (log, "run.tlm") == DP_OK);
  DP_CHECK (dp_event_log_field_str (log, "state", "tracking") == DP_OK);
  DP_CHECK (dp_event_log_append (log, 100, "seeded", 0, 0.0, 4.0e6) == DP_OK);
  DP_CHECK (dp_event_log_append (log, 900, "lost", 64, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_finalize (log, META_PATH, 0 /* cf32 */, 0 /* le */,
                                   1.0e7, 0.0)
            == DP_OK);
  /* The log survives its own finalize: a long run writes a sidecar an hour
     and keeps going. */
  DP_CHECK (dp_event_log_append (log, 1000, "released", 0, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_close (log) == DP_OK);
  DP_CHECK_MSG (dp_event_log_close (log) == DP_OK, "close is idempotent");
  dp_event_log_destroy (log);

  char *s = slurp (META_PATH);
  DP_REQUIRE_MSG (s != NULL, "no sidecar written");
  cJSON *root = cJSON_Parse (s);
  DP_REQUIRE_MSG (root != NULL, "the sidecar is not JSON");

  cJSON *g = cJSON_GetObjectItemCaseSensitive (root, "global");
  DP_REQUIRE_MSG (g != NULL, "no global");
  /* From the writer's emitter, unchanged -- this is the point of going
     through it rather than writing a second one. */
  cJSON *dt = cJSON_GetObjectItemCaseSensitive (g, "core:datatype");
  DP_REQUIRE (dt != NULL && cJSON_IsString (dt));
  DP_CHECK (strcmp (dt->valuestring, "cf32_le") == 0);
  cJSON *ver = cJSON_GetObjectItemCaseSensitive (g, "core:version");
  DP_REQUIRE (ver != NULL && cJSON_IsString (ver));
  DP_CHECK (strcmp (ver->valuestring, "1.0.0") == 0);
  cJSON *sr = cJSON_GetObjectItemCaseSensitive (g, "core:sample_rate");
  DP_REQUIRE (sr != NULL);
  DP_CHECK (dp_near (sr->valuedouble, 1.0e7, 1e-6));

  /* Ours, merged into the same global. */
  cJSON *ds = cJSON_GetObjectItemCaseSensitive (g, "core:dataset");
  DP_REQUIRE_MSG (ds != NULL && cJSON_IsString (ds), "no core:dataset");
  DP_CHECK (strcmp (ds->valuestring, "capture.sigmf-data") == 0);
  DP_CHECK_MSG (cJSON_GetObjectItemCaseSensitive (g, "core:metadata_only")
                    == NULL,
                "a sidecar that names a dataset is not metadata-only");
  cJSON *tl = cJSON_GetObjectItemCaseSensitive (g, "doppler:telemetry");
  DP_REQUIRE_MSG (tl != NULL, "no doppler:telemetry");
  cJSON *tp = cJSON_GetObjectItemCaseSensitive (tl, "path");
  DP_REQUIRE (tp != NULL && cJSON_IsString (tp));
  DP_CHECK (strcmp (tp->valuestring, "run.tlm") == 0);
  cJSON *tdt = cJSON_GetObjectItemCaseSensitive (tl, "dtype");
  DP_REQUIRE_MSG (tdt != NULL && cJSON_IsArray (tdt),
                  "the telemetry pointer without its dtype is not readable");
  DP_CHECK (cJSON_GetArraySize (tdt) == 4);

  cJSON *caps = cJSON_GetObjectItemCaseSensitive (root, "captures");
  DP_REQUIRE (caps != NULL && cJSON_IsArray (caps));
  DP_CHECK (cJSON_GetArraySize (caps) == 1);
  cJSON *fc = cJSON_GetObjectItemCaseSensitive (cJSON_GetArrayItem (caps, 0),
                                                "core:frequency");
  DP_REQUIRE_MSG (fc != NULL, "the log's fc must reach captures[0]");
  DP_CHECK (dp_near (fc->valuedouble, 2.4e9, 1.0));

  cJSON *anns = cJSON_GetObjectItemCaseSensitive (root, "annotations");
  DP_REQUIRE (anns != NULL && cJSON_IsArray (anns));
  DP_CHECK_MSG (cJSON_GetArraySize (anns) == 2,
                "the sidecar describes the events appended BEFORE it");
  cJSON *a0 = cJSON_GetArrayItem (anns, 0);
  cJSON *s0 = cJSON_GetObjectItemCaseSensitive (a0, "core:sample_start");
  DP_REQUIRE (s0 != NULL);
  DP_CHECK (dp_near (s0->valuedouble, 100.0, 1e-9));
  cJSON *st0 = cJSON_GetObjectItemCaseSensitive (a0, "doppler:state");
  DP_REQUIRE_MSG (st0 != NULL, "the event's own fields must survive");
  cJSON *a1 = cJSON_GetArrayItem (anns, 1);
  cJSON *l1 = cJSON_GetObjectItemCaseSensitive (a1, "core:label");
  DP_REQUIRE (l1 != NULL && cJSON_IsString (l1));
  DP_CHECK_MSG (strcmp (l1->valuestring, "lost") == 0,
                "annotations keep the order they were appended in");
  cJSON_Delete (root);
  free (s);

  /* The log kept running and the event after the first finalize is in the
     file, so a second render -- with no live log at all, which is the
     recovery path -- picks up all three. */
  DP_CHECK (dp_event_log_write_meta (LOG_PATH, META_PATH, 0, 0, 1.0e7, 2.4e9,
                                     0.0, NULL, NULL)
            == DP_OK);
  s = slurp (META_PATH);
  DP_REQUIRE (s != NULL);
  root = cJSON_Parse (s);
  DP_REQUIRE (root != NULL);
  anns = cJSON_GetObjectItemCaseSensitive (root, "annotations");
  DP_REQUIRE (anns != NULL && cJSON_IsArray (anns));
  DP_CHECK_MSG (cJSON_GetArraySize (anns) == 3,
                "a re-render must see the events appended since the last");
  cJSON *a2 = cJSON_GetArrayItem (anns, 2);
  cJSON *l2 = cJSON_GetObjectItemCaseSensitive (a2, "core:label");
  DP_REQUIRE (l2 != NULL && cJSON_IsString (l2));
  DP_CHECK (strcmp (l2->valuestring, "released") == 0);
  cJSON_Delete (root);
  free (s);
  return 0;
}

/* ── 7. no dataset: SigMF's own word for it ─────────────────────────────── */
static int
test_metadata_only_when_nothing_was_recorded (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 0.0);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_append (log, 1, "seeded", 0, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_finalize (log, META_PATH, 0, 0, 0.0, 0.0) == DP_OK);
  dp_event_log_destroy (log);

  char *s = slurp (META_PATH);
  DP_REQUIRE (s != NULL);
  DP_CHECK_MSG (strstr (s, "\"core:metadata_only\":true") != NULL,
                "a live stream nobody recorded is metadata_only");
  DP_CHECK (strstr (s, "core:dataset") == NULL);
  /* fs and fc were not stated, so neither key appears -- the emitter's own
     rule, reached through it rather than restated here. */
  DP_CHECK_MSG (strstr (s, "core:sample_rate") == NULL,
                "an unstated rate must be omitted, not defaulted");
  DP_CHECK_MSG (strstr (s, "core:frequency") == NULL,
                "an unstated centre must be omitted, not written as DC");
  DP_CHECK (strstr (s, "doppler:telemetry") == NULL);
  free (s);
  return 0;
}

/* ── 8. the crashed run: a truncated last line costs only itself ────────── */
static int
test_write_meta_survives_a_truncated_tail (void)
{
  /* Exactly what a killed process leaves: two whole lines and half of a
     third, with no terminating newline. */
  FILE *f = fopen (LOG_PATH, "w");
  DP_REQUIRE (f != NULL);
  fputs ("{\"core:sample_start\":1,\"core:label\":\"seeded\"}\n", f);
  fputs ("{\"core:sample_start\":2,\"core:label\":\"tracking\"}\n", f);
  fputs ("{\"core:sample_start\":3,\"core:la", f);
  fclose (f);

  DP_CHECK (dp_event_log_write_meta (LOG_PATH, META_PATH, 0, 0, 1.0e6, 0.0,
                                     0.0, NULL, NULL)
            == DP_OK);
  char *s = slurp (META_PATH);
  DP_REQUIRE (s != NULL);
  cJSON *root = cJSON_Parse (s);
  DP_REQUIRE (root != NULL);
  cJSON *anns = cJSON_GetObjectItemCaseSensitive (root, "annotations");
  DP_REQUIRE (anns != NULL && cJSON_IsArray (anns));
  DP_CHECK_MSG (cJSON_GetArraySize (anns) == 2,
                "the two whole events must survive the truncated third");
  cJSON_Delete (root);
  free (s);

  /* An empty log is a run in which nothing happened, not a failure. */
  f = fopen (LOG_PATH, "w");
  DP_REQUIRE (f != NULL);
  fclose (f);
  DP_CHECK (dp_event_log_write_meta (LOG_PATH, META_PATH, 0, 0, 0.0, 0.0, 0.0,
                                     NULL, NULL)
            == DP_OK);
  s = slurp (META_PATH);
  DP_REQUIRE (s != NULL);
  DP_CHECK (strstr (s, "\"annotations\":[]") != NULL);
  free (s);
  return 0;
}

/* ── 9. refusals at the edges of the object ─────────────────────────────── */
static int
test_open_and_argument_refusals (void)
{
  DP_CHECK (dp_event_log_open (NULL, 0.0) == NULL);
  DP_CHECK (dp_event_log_open ("", 0.0) == NULL);
  DP_CHECK_MSG (dp_event_log_open ("dp_evlog_no_such_dir/x.events", 0.0)
                    == NULL,
                "an unopenable path must be a NULL, not a live log");
  DP_CHECK (dp_event_log_close (NULL) == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_count (NULL) == 0u);
  dp_event_log_destroy (NULL); /* must not crash */

  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 0.0);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_close (log) == DP_OK);
  DP_CHECK_MSG (dp_event_log_append (log, 1, "x", 0, 0.0, 0.0)
                    == DP_ERR_INVALID,
                "appending to a closed log must fail, not be swallowed");
  DP_CHECK (dp_event_log_count (log) == 0u);
  dp_event_log_destroy (log);

  DP_CHECK (dp_event_log_write_meta (NULL, META_PATH, 0, 0, 0.0, 0.0, 0.0,
                                     NULL, NULL)
            == DP_ERR_INVALID);
  DP_CHECK (
      dp_event_log_write_meta (LOG_PATH, NULL, 0, 0, 0.0, 0.0, 0.0, NULL, NULL)
      == DP_ERR_INVALID);
  DP_CHECK_MSG (dp_event_log_write_meta ("dp_evlog_absent.events", META_PATH,
                                         0, 0, 0.0, 0.0, 0.0, NULL, NULL)
                    == DP_ERR_SEND,
                "an absent event file is a failure, not an empty sidecar");
  DP_CHECK (dp_event_log_finalize (NULL, META_PATH, 0, 0, 0.0, 0.0)
            == DP_ERR_INVALID);
  return 0;
}

/* ── 10. a full disk: every write fails, and the log says so ────────────
 *
 * The error paths are not decoration. A log is the record of a run that
 * nobody is watching, so "the events did not reach the disk" has to be
 * something the holder can be told — and the telling is sticky, because the
 * append that failed is long past by the time anyone closes.
 *
 * `/dev/full` is the honest way to arrange it: open succeeds, every write
 * returns ENOSPC. Linux only, so the case is compiled where it exists and
 * the rest of the file does not depend on it.
 */
#ifdef __linux__
static int
test_a_failed_write_is_reported_and_sticky (void)
{
  dp_event_log_t *log = dp_event_log_open ("/dev/full", 0.0);
  DP_REQUIRE_MSG (log != NULL, "/dev/full must open");
  DP_CHECK_MSG (dp_event_log_append (log, 1, "seeded", 0, 0.0, 0.0)
                    == DP_ERR_SEND,
                "a write that cannot land must not report success");
  DP_CHECK_MSG (dp_event_log_count (log) == 0u,
                "an event that never reached the disk is not an event");
  /* Sticky: the failure is reported at close, long after the append. */
  DP_CHECK (dp_event_log_close (log) == DP_ERR_SEND);
  DP_CHECK_MSG (dp_event_log_destroy (log) == DP_ERR_SEND,
                "the destructor reaches the same verdict as close()");
  /* And finalize over it. `/dev/full` READS like `/dev/zero`, and the first
     draft of this test rendered a sidecar from it, never reached EOF, and
     took the machine down. It is asked again on purpose: write_meta now
     refuses a non-regular file on its type before reading a byte, and the
     line cap behind that would stop the read at DP_EVENT_LOG_LINE_MAX if it
     did not. Each guard has its own test below; this is the pair together,
     on the input that found the hole. */
  log = dp_event_log_open ("/dev/full", 0.0);
  DP_REQUIRE (log != NULL);
  remove (META_PATH);
  DP_CHECK_MSG (dp_event_log_finalize (log, META_PATH, 0, 0, 1.0e6, 0.0)
                    == DP_ERR_INVALID,
                "a character device is not an event log");
  DP_CHECK_MSG (fopen (META_PATH, "r") == NULL,
                "no sidecar is written for a refused file");
  dp_event_log_destroy (log);
  return 0;
}
#endif

/* ── 12. one line ceiling, held on both sides ───────────────────────────── */
static int
test_a_line_is_bounded_on_both_sides (void)
{
  /* The writer: an event that renders to the ceiling is refused, and one
     well past any real label is not -- the cap is the LINE, not a smaller
     number hiding behind it. */
  char *big = (char *)malloc (DP_EVENT_LOG_LINE_MAX + 1u);
  DP_REQUIRE (big != NULL);
  memset (big, 'x', DP_EVENT_LOG_LINE_MAX);
  big[DP_EVENT_LOG_LINE_MAX] = '\0';
  dp_event_log_t *log        = dp_event_log_open (LOG_PATH, 0.0);
  DP_REQUIRE (log != NULL);
  DP_CHECK_MSG (dp_event_log_append (log, 1, big, 0, 0.0, 0.0)
                    == DP_ERR_INVALID,
                "a line at the ceiling must be refused by the writer");
  DP_CHECK (dp_event_log_count (log) == 0u);
  big[1000] = '\0';
  DP_CHECK_MSG (dp_event_log_append (log, 1, big, 0, 0.0, 0.0) == DP_OK,
                "a long label under the ceiling is an ordinary event");
  DP_CHECK (dp_event_log_close (log) == DP_OK);
  dp_event_log_destroy (log);

  /* The reader: a regular file whose one line reaches the ceiling is not an
     event log. No newline, so a reader without the cap would keep growing
     until EOF -- here a few KiB, on a capture a few GiB. */
  FILE *f = fopen (LOG_PATH, "w");
  DP_REQUIRE (f != NULL);
  memset (big, 'x', DP_EVENT_LOG_LINE_MAX);
  DP_REQUIRE (fwrite (big, 1, DP_EVENT_LOG_LINE_MAX, f)
              == DP_EVENT_LOG_LINE_MAX);
  fclose (f);
  free (big);
  remove (META_PATH);
  DP_CHECK_MSG (dp_event_log_write_meta (LOG_PATH, META_PATH, 0, 0, 1.0e6, 0.0,
                                         0.0, NULL, NULL)
                    == DP_ERR_INVALID,
                "a line at the ceiling must be refused by the reader");
  DP_CHECK_MSG (fopen (META_PATH, "r") == NULL,
                "no sidecar is written for a refused file");

  /* The reader, on type alone: `/dev/null` reads instant EOF, so a reader
     that only capped the line would happily describe it as an empty run. */
  DP_CHECK_MSG (dp_event_log_write_meta ("/dev/null", META_PATH, 0, 0, 1.0e6,
                                         0.0, 0.0, NULL, NULL)
                    == DP_ERR_INVALID,
                "a file that is not a regular file is not an event log");
  DP_CHECK (fopen (META_PATH, "r") == NULL);
  return 0;
}

/* ── 11. clearing a run-scoped name ─────────────────────────────────────── */
static int
test_a_cleared_name_is_no_name (void)
{
  dp_event_log_t *log = dp_event_log_open (LOG_PATH, 0.0);
  DP_REQUIRE (log != NULL);
  DP_CHECK (dp_event_log_set_dataset (log, "capture.sigmf-data") == DP_OK);
  /* NULL and "" both mean "none": a caller clearing a name and a caller who
     never set one are asking for the same document. */
  DP_CHECK (dp_event_log_set_dataset (log, NULL) == DP_OK);
  DP_CHECK (dp_event_log_set_telemetry (log, "run.tlm") == DP_OK);
  DP_CHECK (dp_event_log_set_telemetry (log, "") == DP_OK);
  DP_CHECK (dp_event_log_set_dataset (NULL, "x") == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_set_telemetry (NULL, "x") == DP_ERR_INVALID);
  DP_CHECK (dp_event_log_append (log, 1, "seeded", 0, 0.0, 0.0) == DP_OK);
  DP_CHECK (dp_event_log_finalize (log, META_PATH, 0, 0, 0.0, 0.0) == DP_OK);
  dp_event_log_destroy (log);

  char *s = slurp (META_PATH);
  DP_REQUIRE (s != NULL);
  DP_CHECK_MSG (strstr (s, "core:dataset") == NULL,
                "a cleared dataset must leave no key behind");
  DP_CHECK (strstr (s, "\"core:metadata_only\":true") != NULL);
  DP_CHECK (strstr (s, "doppler:telemetry") == NULL);
  free (s);
  return 0;
}

int
main (void)
{
  (void)test_jsonl_is_one_line_per_event ();
  (void)test_omits_what_it_was_not_told ();
  (void)test_edges_when_the_centre_is_known ();
  (void)test_fields_are_namespaced_and_consumed ();
  (void)test_field_rejects ();
  (void)test_finalize_writes_a_sigmf_sidecar ();
  (void)test_metadata_only_when_nothing_was_recorded ();
  (void)test_write_meta_survives_a_truncated_tail ();
  (void)test_open_and_argument_refusals ();
  (void)test_a_cleared_name_is_no_name ();
#ifdef __linux__
  (void)test_a_failed_write_is_reported_and_sticky ();
#endif
  (void)test_a_line_is_bounded_on_both_sides ();

  remove (LOG_PATH);
  remove (META_PATH);
  DP_TEST_END ("test_dp_event_log_core");
}
