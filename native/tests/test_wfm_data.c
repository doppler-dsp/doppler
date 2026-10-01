/**
 * @file test_wfm_data.c
 * @brief The data source's contract (wfm/wfm_data.h), rule by rule.
 *
 * Each block pins one rule of docs/design/payload-data-source.md §4. A
 * paced stream's "nothing yet" is driven through a pipe the test writes to
 * itself: a read of an empty pipe with no timeout is deterministically
 * nothing yet, so no test here sleeps or reads a clock.
 */
#include "doppler/dp_hash64.h"
#include "doppler/wfm/wfm_data.h"
#include "doppler/wfm/wfm_frame.h"
#include "dp_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The pipe and temp-file calls this test makes, per platform: the source
   reads a Windows anonymous pipe through PeekNamedPipe, so the same
   no-clock "nothing yet" is driven there through _pipe. */
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#define pipe(p) _pipe ((p), 4096, _O_BINARY)
/* A function, not a function-like macro: a call passes compound literals
   such as (const uint8_t[]){ 0xC3, 0x5A }, whose commas would split a
   macro's arguments -- clang-cl refused exactly that. */
static int
write_shim (int f, const void *b, size_t n)
{
  return _write (f, b, (unsigned)n);
}
#define write write_shim
#define close(f) _close (f)
#define unlink(p) _unlink (p)
static int
mkstemp (char *tmpl)
{
  if (_mktemp_s (tmpl, strlen (tmpl) + 1u) != 0)
    return -1;
  return _open (tmpl, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
                _S_IREAD | _S_IWRITE);
}
#define TMP_ENV "TEMP"
#else
#include <unistd.h>
#define TMP_ENV "TMPDIR"
#endif

static const char *why;

/* Bits of an octet string, MSB first: the expected unpacking. */
static void
unpack (const uint8_t *o, size_t n, uint8_t *bits)
{
  for (size_t i = 0; i < 8u * n; i++)
    bits[i] = (uint8_t)((o[i / 8u] >> (7u - i % 8u)) & 1u);
}

/* A regular file holding @p o, created in TMPDIR; its path in @p path. */
static int
temp_file (const uint8_t *o, size_t n, char *path, size_t cap)
{
  const char *dir = getenv (TMP_ENV);
  (void)snprintf (path, cap, "%s/test_wfm_data.XXXXXX", dir ? dir : "/tmp");
  const int fd = mkstemp (path);
  if (fd < 0)
    return -1;
  const int ok = n == 0 || (long long)write (fd, o, n) == (long long)n;
  close (fd);
  return ok ? 0 : -1;
}

/* A finite Field splits into LEN-bit frames, then ends. */
static int
test_finite_split (void)
{
  wfm_data_src_t *s = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t want[16], b[8];
  unpack ((const uint8_t[]){ 0xAB, 0xCD }, 2, want);
  DP_CHECK (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_FRAME
            && memcmp (b, want, 8) == 0);
  DP_CHECK (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_FRAME
            && memcmp (b, want + 8, 8) == 0);
  DP_CHECK_MSG (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_END,
                "a finite source ends after ceil(bits / LEN) frames");
  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK (st.frames == 2 && st.bits == 16 && st.total_bits == 16
            && st.pad_bits == 0 && !st.stream && !st.hashed);
  dp_wfm_data_destroy (s);

  /* A generated Field with LEN > 0 is finite the same way. */
  uint8_t pn[20], c[10];
  DP_REQUIRE (dp_wfm_field_bits ("pn:20:5", pn, sizeof pn, NULL) == 20);
  s = dp_wfm_data_create ("pn:20:5", NULL, 10, NULL, &why);
  DP_REQUIRE_MSG (s != NULL, why);
  DP_CHECK (dp_wfm_data_next (s, 1, c, sizeof c, -1) == WFM_DATA_FRAME
            && memcmp (c, pn, 10) == 0);
  DP_CHECK (dp_wfm_data_next (s, 1, c, sizeof c, -1) == WFM_DATA_FRAME
            && memcmp (c, pn + 10, 10) == 0);
  DP_CHECK (dp_wfm_data_next (s, 1, c, sizeof c, -1) == WFM_DATA_END);
  dp_wfm_data_destroy (s);
  return 0;
}

/* A repeated data field is ONE draw per frame, sent REPS times. */
static int
test_one_draw_per_frame (void)
{
  wfm_data_src_t *s = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t want[16], b[24];
  unpack ((const uint8_t[]){ 0xAB, 0xCD }, 2, want);
  DP_REQUIRE (dp_wfm_data_next (s, 3, b, sizeof b, -1) == WFM_DATA_FRAME);
  for (size_t r = 0; r < 3; r++)
    DP_CHECK_MSG (memcmp (b + 8 * r, want, 8) == 0,
                  "every repetition is the frame's one chunk");
  DP_REQUIRE (dp_wfm_data_next (s, 3, b, sizeof b, -1) == WFM_DATA_FRAME);
  for (size_t r = 0; r < 3; r++)
    DP_CHECK_MSG (memcmp (b + 8 * r, want + 8, 8) == 0,
                  "and the next frame draws the NEXT chunk, not the third");
  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK_MSG (st.bits == 16 && st.frames == 2,
                "the source advances LEN bits per frame, whatever REPS is");
  DP_CHECK_MSG (dp_wfm_data_next (s, 3, b, 23, -1) == WFM_DATA_ERROR,
                "an output smaller than LEN * REPS is refused");
  dp_wfm_data_destroy (s);
  return 0;
}

/* The last chunk: padded from the fill, or refused before any sample. */
static int
test_fill (void)
{
  why               = NULL;
  wfm_data_src_t *s = dp_wfm_data_create ("0xABC", NULL, 8, NULL, &why);
  DP_CHECK_MSG (s == NULL, "12 bits in 8-bit frames with no fill: refused");
  DP_CHECK_MSG (why && strstr (why, "--fill"),
                "and the reason names what would fix it");
  DP_CHECK_MSG (dp_wfm_data_length_bits ("0xABC", NULL) == 12,
                "and the length is there to state the remainder in: 12 bits, "
                "4 into the last 8-bit frame, 4 short");
  dp_wfm_data_destroy (s);

  s = dp_wfm_data_create ("0xABC", NULL, 8, "01", &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t b[8];
  DP_REQUIRE (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_FRAME);
  DP_REQUIRE (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_FRAME);
  static const uint8_t last[8]
      = { 1, 1, 0, 0, 0, 1, 0, 1 }; /* C, then 01 01 */
  DP_CHECK_MSG (memcmp (b, last, 8) == 0,
                "the last frame is the data, then the fill tiled from bit 0");
  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK_MSG (st.pad_bits == 4 && st.frames == 2 && st.bits == 12,
                "the pad is counted, as truth for scoring");
  DP_CHECK (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_END);
  dp_wfm_data_destroy (s);
  return 0;
}

/* A pipe: octets straddle frames, the end is padded, the hash is kept. */
static int
test_pipe_straddle (void)
{
  int p[2];
  DP_REQUIRE (pipe (p) == 0);
  const uint8_t oct[3] = { 0xA5, 0x0F, 0xFF };
  DP_REQUIRE (write (p[1], oct, 3) == 3);
  close (p[1]);

  wfm_data_src_t *s = dp_wfm_data_create_fd (p[0], 10, "0", &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t want[24], got[30], b[10];
  unpack (oct, 3, want);
  for (size_t f = 0; f < 3; f++)
    {
      DP_REQUIRE (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_FRAME);
      memcpy (got + 10 * f, b, 10);
    }
  DP_CHECK_MSG (memcmp (got, want, 24) == 0,
                "24 bits in 10-bit frames come back in order across the "
                "octet boundaries");
  static const uint8_t zeros[6] = { 0 };
  DP_CHECK_MSG (memcmp (got + 24, zeros, 6) == 0,
                "a pipe that ends mid-frame is padded with the fill");
  DP_CHECK (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_END);

  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK (st.stream && st.frames == 3 && st.pad_bits == 6 && st.bits == 24
            && st.total_bits == 0);
  DP_CHECK_MSG (st.hashed && st.hash == dp_hash64 (DP_HASH64_INIT, oct, 3),
                "the hash of what was read, computed while reading");
  dp_wfm_data_destroy (s);
  close (p[0]);

  DP_REQUIRE (pipe (p) == 0);
  DP_CHECK_MSG (dp_wfm_data_create_fd (p[0], 10, NULL, &why) == NULL,
                "a pipe with no fill is refused before anything is read");
  DP_CHECK (strstr (why, "fill") != NULL);
  close (p[0]);
  close (p[1]);

  /* One bit a frame has no remainder to pad: a one-bit pipe (continuous
     dsss reads a bit per data symbol) needs no fill. */
  DP_REQUIRE (pipe (p) == 0);
  s = dp_wfm_data_create_fd (p[0], 1, NULL, &why);
  DP_CHECK_MSG (s != NULL, "a one-bit pipe with no fill is a source");
  dp_wfm_data_destroy (s);
  close (p[0]);
  close (p[1]);
  return 0;
}

/* Nothing yet: kept bits, an all-fill idle frame, then the data resumes. */
static int
test_not_yet (void)
{
  int p[2];
  DP_REQUIRE (pipe (p) == 0);
  wfm_data_src_t *s = dp_wfm_data_create_fd (p[0], 16, "10", &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t b[16], want[16];

  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0x12, 0x34 }, 2) == 2);
  unpack ((const uint8_t[]){ 0x12, 0x34 }, 2, want);
  DP_CHECK (dp_wfm_data_next (s, 1, b, sizeof b, 0) == WFM_DATA_FRAME
            && memcmp (b, want, 16) == 0);

  DP_CHECK_MSG (dp_wfm_data_next (s, 1, b, sizeof b, 0) == WFM_DATA_NOT_YET,
                "an empty pipe, asked with no wait, is nothing yet");
  DP_REQUIRE (dp_wfm_data_idle (s, 1, b, sizeof b) == WFM_DATA_FRAME);
  for (size_t i = 0; i < 16; i++)
    DP_CHECK_MSG (b[i] == (uint8_t)((i & 1u) ^ 1u),
                  "an idle frame is all fill, tiled from its first bit");

  /* Half a frame arrives: still nothing yet, and those bits are KEPT. */
  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0x56 }, 1) == 1);
  DP_CHECK_MSG (dp_wfm_data_next (s, 1, b, sizeof b, 0) == WFM_DATA_NOT_YET,
                "a partial chunk is nothing yet");
  DP_REQUIRE (dp_wfm_data_idle (s, 1, b, sizeof b) == WFM_DATA_FRAME);
  for (size_t i = 0; i < 16; i++)
    DP_CHECK_MSG (b[i] == (uint8_t)((i & 1u) ^ 1u),
                  "§4.1: an idle frame is ALL fill, even with 8 bits of the "
                  "next chunk buffered -- it never carries a partial chunk");
  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0x78 }, 1) == 1);
  unpack ((const uint8_t[]){ 0x56, 0x78 }, 2, want);
  DP_CHECK_MSG (dp_wfm_data_next (s, 1, b, sizeof b, 0) == WFM_DATA_FRAME
                    && memcmp (b, want, 16) == 0,
                "the bits read before the idle frame open the next data "
                "frame; the idle frame consumed none");

  close (p[1]);
  DP_CHECK_MSG (dp_wfm_data_next (s, 1, b, sizeof b, 0) == WFM_DATA_END,
                "a closed pipe with nothing left is the end, not a pause");
  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK (st.frames == 2 && st.idle_frames == 2 && st.bits == 32
            && st.pad_bits == 0);
  DP_CHECK (st.hash
            == dp_hash64 (DP_HASH64_INIT,
                          (const uint8_t[]){ 0x12, 0x34, 0x56, 0x78 }, 4));
  dp_wfm_data_destroy (s);
  close (p[0]);
  return 0;
}

/* A regular file: finite, its length from fstat, its hash as it is read. */
static int
test_file (void)
{
  uint8_t oct[300];
  for (size_t i = 0; i < sizeof oct; i++)
    oct[i] = (uint8_t)(i * 37u + 11u);
  char path[512];
  DP_REQUIRE (temp_file (oct, sizeof oct, path, sizeof path) == 0);

  wfm_data_src_t *s = dp_wfm_data_create (NULL, path, 96, NULL, &why);
  DP_REQUIRE_MSG (s != NULL, why); /* 2400 bits = 25 frames of 96 */
  uint8_t          b[96], want[2400];
  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK_MSG (st.total_bits == 2400 && !st.stream,
                "a regular file is finite, its length known up front");
  unpack (oct, sizeof oct, want);
  size_t f = 0;
  while (dp_wfm_data_next (s, 1, b, sizeof b, -1) == WFM_DATA_FRAME)
    {
      DP_CHECK (memcmp (b, want + 96 * f, 96) == 0);
      f++;
    }
  DP_CHECK (f == 25);
  dp_wfm_data_stats (s, &st);
  DP_CHECK_MSG (st.hashed
                    && st.hash == dp_hash64 (DP_HASH64_INIT, oct, sizeof oct),
                "the file's hash, read once, equals the whole file's");
  dp_wfm_data_destroy (s);

  why = NULL;
  DP_CHECK_MSG (dp_wfm_data_create (NULL, path, 128, NULL, &why) == NULL,
                "a file that does not divide, with no fill: refused");
  DP_CHECK_MSG (why && strstr (why, "--fill")
                    && dp_wfm_data_length_bits (NULL, path) == 2400,
                "before a sample, with its length from fstat");
  unlink (path);

  DP_REQUIRE (temp_file (NULL, 0, path, sizeof path) == 0);
  DP_CHECK_MSG (dp_wfm_data_create (NULL, path, 8, "0", &why) == NULL,
                "an empty file is refused, fill or not");
  unlink (path);
  DP_CHECK_MSG (dp_wfm_data_create (NULL, path, 8, "0", &why) == NULL,
                "a file that cannot be opened is refused");
  return 0;
}

/* pn:0 is a seeded stream: the m-sequence, chunk after chunk, for ever. */
static int
test_pn_stream (void)
{
  wfm_data_src_t *s = dp_wfm_data_create ("pn:0:9:0x7", NULL, 50, NULL, &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t want[150], got[150];
  DP_REQUIRE (dp_wfm_field_bits ("pn:150:9:0x7", want, sizeof want, NULL)
              == 150);
  for (size_t f = 0; f < 3; f++)
    DP_REQUIRE (dp_wfm_data_next (s, 1, got + 50 * f, 50, 0)
                == WFM_DATA_FRAME);
  DP_CHECK_MSG (memcmp (got, want, 150) == 0,
                "chunk k is bits [kLEN, (k+1)LEN) of the same generator, "
                "so a receiver regenerates it from the Field");
  wfm_data_stats_t st;
  dp_wfm_data_stats (s, &st);
  DP_CHECK (st.stream && st.total_bits == 0 && !st.hashed);
  DP_CHECK_MSG (dp_wfm_data_length_bits ("pn:0:9:0x7", NULL) == 0
                    && dp_wfm_data_length_bits ("pn:150:9", NULL) == 150
                    && dp_wfm_data_length_bits (NULL, "-") == 0
                    && dp_wfm_data_length_bits ("0xAG", NULL) == 0
                    && dp_wfm_data_length_bits ("0xAB", "-") == 0,
                "a stream, bad text and both-given have no length");
  dp_wfm_data_destroy (s);

  DP_CHECK_MSG (dp_wfm_data_create ("pn:0:9*2", NULL, 50, NULL, &why) == NULL,
                "a stream has no end to repeat: *REPS is refused");
  DP_CHECK_MSG (dp_wfm_data_create ("pn:0:65", NULL, 50, NULL, &why) == NULL,
                "and the rest of the Field is still the parser's to refuse");
  return 0;
}

/* What a source is built from: exactly one of the pair, and a real one. */
static int
test_refusals (void)
{
  DP_CHECK_MSG (dp_wfm_data_create ("0xAB", "-", 8, NULL, &why) == NULL,
                "--data and --data-from-file together: refused");
  DP_CHECK (strstr (why, "--data") && strstr (why, "--data-from-file"));
  DP_CHECK_MSG (dp_wfm_data_create (NULL, NULL, 8, NULL, &why) == NULL,
                "neither: refused");
  DP_CHECK_MSG (dp_wfm_data_create ("0xAB", NULL, 0, NULL, &why) == NULL,
                "LEN 0: refused");
  DP_CHECK_MSG (dp_wfm_data_create ("data:8", NULL, 8, NULL, &why) == NULL,
                "a data field cannot be its own source");
  DP_CHECK_MSG (dp_wfm_data_create ("0xAB", NULL, 8, "data:8", &why) == NULL,
                "nor a fill");
  DP_CHECK_MSG (dp_wfm_data_create ("0xAB", NULL, 8, "0x1*", &why) == NULL,
                "a fill outside the grammar is the parser's refusal");
  DP_CHECK_MSG (dp_wfm_data_create ("none", NULL, 8, NULL, &why) == NULL,
                "`none` is no source: a face that reads it builds none");
  DP_CHECK_MSG (dp_wfm_data_create ("0xAG", NULL, 8, NULL, &why) == NULL,
                "so is data outside it");

  wfm_data_src_t *s = dp_wfm_data_create ("0xAB", NULL, 8, NULL, &why);
  uint8_t         b[8];
  DP_CHECK_MSG (dp_wfm_data_idle (s, 1, b, sizeof b) == WFM_DATA_ERROR,
                "no fill, no idle frame");
  dp_wfm_data_destroy (s);
  dp_wfm_data_destroy (NULL);
  return 0;
}

/* From a source's own members: a sequence (a bit array, or a generated
   finite Field) and a fill sequence, rendered once. */
static int
test_from_a_sequence (void)
{
  static const uint8_t bits[12] = { 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0 };
  const wfm_seq_t data = { .kind = WFM_SEQ_LITERAL, .bits = bits, .len = 12 };
  const wfm_seq_t fill = { .kind = WFM_SEQ_DOTTED, .len = 2 };
  wfm_data_src_t *s    = dp_wfm_data_create_seq (&data, NULL, 8, &fill, &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t b[8];
  DP_CHECK (dp_wfm_data_next (s, 1, b, 8, -1) == WFM_DATA_FRAME
            && memcmp (b, bits, 8) == 0);
  DP_CHECK (dp_wfm_data_next (s, 1, b, 8, -1) == WFM_DATA_FRAME);
  static const uint8_t last[8] = { 1, 1, 0, 0, 1, 0, 1, 0 };
  DP_CHECK_MSG (memcmp (b, last, 8) == 0,
                "the last frame padded from a GENERATED fill (dotted, 10...)");
  dp_wfm_data_destroy (s);

  /* A generated finite Field is the same bits as its text form. */
  const wfm_seq_t pn = { .kind = WFM_SEQ_PN, .len = 20, .reg_bits = 5 };
  uint8_t         want[20];
  DP_REQUIRE (dp_wfm_field_bits ("pn:20:5", want, 20, NULL) == 20);
  s = dp_wfm_data_create_seq (&pn, NULL, 20, NULL, &why);
  DP_REQUIRE_MSG (s != NULL, why);
  uint8_t g[20];
  DP_CHECK (dp_wfm_data_next (s, 1, g, 20, -1) == WFM_DATA_FRAME
            && memcmp (g, want, 20) == 0);
  dp_wfm_data_destroy (s);

  const wfm_seq_t dq = { .kind = WFM_SEQ_DATA, .len = 8 };
  DP_CHECK_MSG (dp_wfm_data_create_seq (&dq, NULL, 8, NULL, &why) == NULL,
                "data:LEN is not a source");
  DP_CHECK_MSG (dp_wfm_data_create_seq (&data, "-", 8, &fill, &why) == NULL
                    && strstr (why, "--data-from-file"),
                "a sequence and a path together: refused");
  DP_CHECK_MSG (dp_wfm_data_create_seq (&data, NULL, 8, NULL, &why) == NULL,
                "12 bits in 8-bit frames with no fill: refused");
  DP_CHECK_MSG (dp_wfm_data_create_seq (NULL, NULL, 8, NULL, &why) == NULL
                    && strstr (why, "no data source"),
                "neither a sequence nor a path: refused by name");
  DP_CHECK_MSG (dp_wfm_data_create_seq (&data, NULL, 0, &fill, &why) == NULL
                    && strstr (why, "LEN must be > 0"),
                "LEN 0: refused by name");
  /* A 1-bit register has no m-sequence to default to: a fill that cannot
     be built is refused, never rendered as zeros. */
  const wfm_seq_t nopoly = { .kind = WFM_SEQ_PN, .len = 4, .reg_bits = 1 };
  DP_CHECK_MSG (dp_wfm_data_create_seq (&data, NULL, 8, &nopoly, &why) == NULL
                    && strstr (why, "cannot be built"),
                "an unbuildable fill: refused by name");
  DP_CHECK_MSG (dp_wfm_data_create_fd (-1, 8, "0", &why) == NULL
                    && strstr (why, "cannot be read"),
                "a bad fd: refused by name");
  return 0;
}

int
main (void)
{
  if (test_finite_split ())
    return 1;
  if (test_one_draw_per_frame ())
    return 1;
  if (test_fill ())
    return 1;
  if (test_pipe_straddle ())
    return 1;
  if (test_not_yet ())
    return 1;
  if (test_file ())
    return 1;
  if (test_pn_stream ())
    return 1;
  if (test_refusals ())
    return 1;
  if (test_from_a_sequence ())
    return 1;
  DP_TEST_END ("wfm_data");
}
