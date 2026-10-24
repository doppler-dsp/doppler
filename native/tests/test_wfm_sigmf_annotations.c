/*
 * test_wfm_sigmf_annotations.c -- a scene written as a capture carries its
 * annotations, and reads them back (doppler#2564, doppler#2565).
 *
 * The writer is handed the composer whose samples it holds
 * (dp_wfm_writer_annotate) and its sidecar gets the scene's rows; the reader
 * returns the sidecar's `annotations` array as written
 * (dp_wfm_reader_get_annotations). The reference is the builder
 * Composer.to_sigmf() and wfmgen use, dp_wfm_sigmf_meta_json(), called with
 * the WRITER's own fc, so the frequency edges prove the writer is the one
 * source for them.
 */
#include "doppler/clib_common.h"
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm_reader/wfm_reader_core.h"
#include "doppler/wfm_writer/wfm_writer_core.h"
#include "dp_test.h"

#include "cJSON.h"
#include "doppler/dp_complex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define dp_getpid _getpid
#else
#include <unistd.h>
#define dp_getpid getpid
#endif

/* Three tone bursts, each at its own frequency drawn from [90, 110] kHz: the
   rows differ per instance, so they come from the draws and not from the
   spec alone. */
static const char *SCENE
    = "{\"version\":1,\"repeat\":false,\"continuous\":false,\"segments\":[{"
      "\"fs\":1000000,\"num_samples\":1000,\"off_samples\":500,\"repeats\":3,"
      "\"type\":\"tone\",\"freq\":[90000,110000],\"snr\":20,"
      "\"snr_mode\":\"auto\",\"seed\":1,\"sps\":1,\"pn_length\":15,"
      "\"pn_poly\":0,\"lfsr\":\"galois\"}]}";

#define FS 1e6
#define FC 915e6
#define N_TOTAL 4500 /* 3 x (1000 on + 500 off) */

static char path_buf[3][512];

static const char *
tmp_path (int slot, const char *name)
{
  snprintf (path_buf[slot], sizeof path_buf[slot], "%s/dp_sigmf_ann_%d_%s",
            dp_test_tmpdir (), (int)dp_getpid (), name);
  return path_buf[slot];
}

/* The `annotations` array of a SigMF document, re-printed unformatted: the
   one canonical text both sides are compared in. */
static char *
annotations_of (const char *doc)
{
  cJSON *root = cJSON_Parse (doc);
  if (!root)
    return NULL;
  cJSON *a   = cJSON_GetObjectItem (root, "annotations");
  char  *out = a ? cJSON_PrintUnformatted (a) : NULL;
  cJSON_Delete (root);
  return out;
}

/* Re-print a JSON array unformatted, so two texts compare by content. */
static char *
canonical (const char *json)
{
  cJSON *a = cJSON_Parse (json);
  if (!a)
    return NULL;
  char *out = cJSON_PrintUnformatted (a);
  cJSON_Delete (a);
  return out;
}

/* Render the scene into a writer of @p file_type, annotate it, close it. */
static int
write_scene (const char *path, int file_type, int annotate,
             char **expected_out)
{
  dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (SCENE);
  DP_REQUIRE_MSG (c, "compose the scene");
  float _Complex *x = calloc (N_TOTAL, sizeof *x);
  DP_REQUIRE_MSG (x, "alloc");
  DP_REQUIRE_MSG (dp_wfm_compose_execute (c, x, N_TOTAL) == N_TOTAL,
                  "render the whole scene");
  dp_wfm_writer_state_t *w = dp_wfm_writer_create (
      path, FS, file_type, 0, 0, FC, N_TOTAL, 0.0, 0.0, true);
  DP_REQUIRE_MSG (w, "writer create");
  if (annotate)
    DP_REQUIRE_MSG (dp_wfm_writer_annotate (w, c) == DP_OK, "annotate");
  DP_REQUIRE_MSG (dp_wfm_writer_write (w, x, N_TOTAL) == N_TOTAL, "write");
  DP_REQUIRE_MSG (dp_wfm_writer_close (w) == 0, "close");
  if (expected_out)
    {
      size_t               n    = 0;
      const wfm_segment_t *segs = dp_wfm_compose_segments (c, &n, NULL, NULL);
      char *doc = dp_wfm_sigmf_meta_json (0, 0, FS, FC, 0.0, segs, n);
      DP_REQUIRE_MSG (doc, "reference document");
      *expected_out = annotations_of (doc);
      free (doc);
      DP_REQUIRE_MSG (*expected_out, "reference annotations");
    }
  free (x);
  dp_wfm_compose_destroy (c);
  return 0;
}

static char *
read_annotations (const char *path)
{
  dp_wfm_reader_state_t *r = dp_wfm_reader_create (path, -1, 0);
  if (!r)
    return NULL;
  char *json = dp_wfm_reader_get_annotations (r);
  dp_wfm_reader_destroy (r);
  return json;
}

/* SigMF: the sidecar carries one row per burst, equal to to_sigmf()'s with
   the writer's fc, and the reader returns them unchanged. Sabotage: drop the
   scene in write_sigmf_sidecar -> the rows are empty; skip the reader's
   `annotations` capture -> "[]". */
static int
test_sigmf_round_trip (void)
{
  const char *path     = tmp_path (0, "rt.sigmf-data");
  char       *expected = NULL;
  DP_CHECK (write_scene (path, WFM_FT_SIGMF, 1, &expected) == 0);
  char *got = read_annotations (path);
  DP_REQUIRE_MSG (got, "reader open + annotations");
  char *got_c = canonical (got);
  DP_REQUIRE_MSG (got_c, "the reader's annotations are a JSON array");
  cJSON *arr = cJSON_Parse (got_c);
  DP_CHECK_MSG (cJSON_GetArraySize (arr) == 3, "one row per burst");
  /* The writer's fc reached the edges: 915 MHz plus a 90-110 kHz tone. */
  cJSON *e = cJSON_GetObjectItem (cJSON_GetArrayItem (arr, 0),
                                  "core:freq_lower_edge");
  DP_CHECK_MSG (e && e->valuedouble > FC + 8e4 && e->valuedouble < FC + 1.2e5,
                "edges carry the writer's fc");
  cJSON_Delete (arr);
  DP_CHECK_MSG (strcmp (got_c, expected) == 0,
                "read back == Composer.to_sigmf() rows");
  free (got);
  free (got_c);
  free (expected);
  remove (path);
  remove (tmp_path (1, "rt.sigmf-meta"));
  return 0;
}

/* A raw capture keeps the rows in the SigMF-shaped sidecar this library's
   writer leaves beside it, and the reader adopts them like its fs and fc.
   Sabotage: skip the adoption in adopt_sidecar -> "[]". */
static int
test_raw_sidecar_round_trip (void)
{
  const char *path     = tmp_path (0, "rt.cf32");
  char       *expected = NULL;
  DP_CHECK (write_scene (path, WFM_FT_RAW, 1, &expected) == 0);
  char *got   = read_annotations (path);
  char *got_c = got ? canonical (got) : NULL;
  DP_CHECK_MSG (got_c && strcmp (got_c, expected) == 0,
                "raw: read back == Composer.to_sigmf() rows");
  free (got);
  free (got_c);
  free (expected);
  remove (path);
  snprintf (path_buf[1], sizeof path_buf[1], "%s.sigmf-meta", path);
  remove (path_buf[1]);
  return 0;
}

/* No scene, no rows: a plain writer's capture reads "[]", and so does a BLUE
   one, whose header has no annotations. */
static int
test_no_annotations_reads_empty (void)
{
  const char *sig = tmp_path (0, "none.sigmf-data");
  DP_CHECK (write_scene (sig, WFM_FT_SIGMF, 0, NULL) == 0);
  char *got = read_annotations (sig);
  DP_CHECK_MSG (got && strcmp (got, "[]") == 0, "no scene -> []");
  free (got);
  remove (sig);
  remove (tmp_path (1, "none.sigmf-meta"));

  const char *blue = tmp_path (2, "none.blue");
  DP_CHECK (write_scene (blue, WFM_FT_BLUE, 0, NULL) == 0);
  got = read_annotations (blue);
  DP_CHECK_MSG (got && strcmp (got, "[]") == 0, "BLUE -> []");
  free (got);
  remove (blue);
  return 0;
}

/* annotate refuses where the rows would be dropped: BLUE (no sidecar), a
   raw writer with the sidecar turned off, and NULL. Sabotage: accept them
   -> red. */
static int
test_annotate_refuses_without_a_sidecar (void)
{
  dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (SCENE);
  DP_REQUIRE_MSG (c, "compose");
  const char            *blue = tmp_path (0, "ref.blue");
  dp_wfm_writer_state_t *w = dp_wfm_writer_create (blue, FS, WFM_FT_BLUE, 0, 0,
                                                   FC, 0, 0.0, 0.0, true);
  DP_REQUIRE_MSG (w, "blue writer");
  DP_CHECK_MSG (dp_wfm_writer_annotate (w, c) == DP_ERR_INVALID,
                "BLUE has nowhere to put annotations");
  DP_CHECK_MSG (dp_wfm_writer_annotate (w, NULL) == DP_ERR_INVALID,
                "NULL scene");
  DP_CHECK (dp_wfm_writer_close (w) == 0);
  remove (blue);

  const char *raw = tmp_path (1, "ref.cf32");
  w = dp_wfm_writer_create (raw, FS, WFM_FT_RAW, 0, 0, FC, 0, 0.0, 0.0, false);
  DP_REQUIRE_MSG (w, "raw writer, no sidecar");
  DP_CHECK_MSG (dp_wfm_writer_annotate (w, c) == DP_ERR_INVALID,
                "sidecar=false has nowhere to put annotations");
  DP_CHECK (dp_wfm_writer_close (w) == 0);
  remove (raw);
  DP_CHECK (dp_wfm_writer_annotate (NULL, c) == DP_ERR_INVALID);
  dp_wfm_compose_destroy (c);
  return 0;
}

int
main (void)
{
  DP_CHECK (test_sigmf_round_trip () == 0);
  DP_CHECK (test_raw_sidecar_round_trip () == 0);
  DP_CHECK (test_no_annotations_reads_empty () == 0);
  DP_CHECK (test_annotate_refuses_without_a_sidecar () == 0);
  DP_TEST_END ("test_wfm_sigmf_annotations");
}
