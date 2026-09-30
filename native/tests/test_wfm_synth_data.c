/**
 * @file test_wfm_synth_data.c
 * @brief The pull that replaces the cycle: a synth fed frame by frame from a
 * data source (dp_wfm_synth_attach_data, docs/design/payload-data-source.md
 * §7 step 5).
 *
 * Every synth here is type=bits with one sample per bit and no modulation,
 * so each output sample IS a bit (a 0/1 amplitude line) and the frames on
 * the air can be read back and compared with dp_wfm_frame_assemble_data over
 * the same chunks. A pipe the test writes to itself makes "nothing yet"
 * deterministic, so no test sleeps.
 */
#include "doppler/dp_crc16.h"
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_data.h"
#include "doppler/wfm/wfm_frame.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
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
#else
#include <unistd.h>
#endif

static const char *why;

/* A type=bits synth at one sample per bit: its output is the bit stream. */
static dp_wfm_synth_state_t *
line_synth (void)
{
  return dp_wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0,
                              0, 0.0);
}

/* n samples, sliced back to bits: > 0.5 is a one. Returns 0, or -1 when a
   sample is neither a clean 0 nor a clean 1 (not a bit at all). */
static int
read_bits (dp_wfm_synth_state_t *s, uint8_t *bits, size_t n)
{
  float _Complex *x = malloc (n * sizeof *x);
  dp_wfm_synth_steps (s, x, n);
  int bad = 0;
  for (size_t i = 0; i < n; i++)
    {
      const float a = crealf (x[i]);
      bits[i]       = a > 0.5f;
      if (fabsf (a - (float)bits[i]) > 1e-3f)
        bad = 1;
    }
  free (x);
  return bad ? -1 : 0;
}

/* A description [data:LEN | crc16] or, with crc 0, [data:LEN]. */
static void
data_frame (wfm_frame_desc_t *d, size_t len, int crc)
{
  const wfm_seq_t data = { .kind = WFM_SEQ_DATA, .len = len };
  (void)dp_wfm_frame_fixed (d, NULL, 0, NULL, &data, crc);
}

/* Frames in order from a finite source, each its own chunk and its own CRC;
   then silence, and the end latched. */
static int
test_frames_in_order (void)
{
  wfm_frame_desc_t d;
  data_frame (&d, 16, 1);
  wfm_data_src_t *src
      = dp_wfm_data_create ("0x0123456789AB", NULL, 16, NULL, &why);
  DP_REQUIRE_MSG (src != NULL, why);
  dp_wfm_synth_state_t *s = line_synth ();
  DP_REQUIRE_MSG (
      dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_UNPACED, 0) == 0,
      "a bits synth takes a frame with a data field");

  /* The truth: the same three chunks, assembled by the frame code. */
  static const uint8_t oct[6] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB };
  uint8_t              want[3 * 32], got[3 * 32 + 40];
  for (size_t f = 0; f < 3; f++)
    {
      uint8_t chunk[16];
      for (size_t i = 0; i < 16; i++)
        chunk[i] = (uint8_t)((oct[2 * f + i / 8] >> (7 - i % 8)) & 1u);
      DP_REQUIRE (
          dp_wfm_frame_assemble_data (&d, NULL, chunk, want + 32 * f, 32)
          == 32);
    }
  DP_REQUIRE_MSG (read_bits (s, got, sizeof got) == 0,
                  "every sample is a clean bit or clean silence");
  DP_CHECK_MSG (memcmp (got, want, sizeof want) == 0,
                "three frames, in order, each over its own chunk with its "
                "own CRC -- not the first frame cycled");
  for (size_t i = sizeof want; i < sizeof got; i++)
    DP_CHECK_MSG (got[i] == 0, "after the data: silence");
  DP_CHECK_MSG (dp_wfm_synth_data_ended (s), "and the end is latched");

  wfm_data_stats_t st;
  dp_wfm_data_stats (dp_wfm_synth_data_source (s), &st);
  DP_CHECK_MSG (st.frames == 3 && st.bits == 48 && st.idle_frames == 0,
                "the source's stats are the run's truth");
  dp_wfm_synth_destroy (s);
  return 0;
}

/* Silence after the end, not a held symbol: at bpsk, a held last symbol is
   a line of amplitude 1 nobody sent. */
static int
test_silence_not_held (void)
{
  wfm_frame_desc_t d;
  data_frame (&d, 8, 0);
  wfm_data_src_t *src = dp_wfm_data_create ("0xFF", NULL, 8, NULL, &why);
  DP_REQUIRE_MSG (src != NULL, why);
  dp_wfm_synth_state_t *s = line_synth ();
  DP_REQUIRE (dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_UNPACED, 1)
              == 0);
  float _Complex x[24];
  dp_wfm_synth_steps (s, x, 24);
  for (size_t i = 0; i < 8; i++)
    DP_CHECK_MSG (fabsf (crealf (x[i]) + 1.0f) < 1e-3f,
                  "bpsk: eight ones are eight -1 symbols");
  for (size_t i = 8; i < 24; i++)
    DP_CHECK_MSG (cabsf (x[i]) < 1e-3f,
                  "after the end, zero -- not the last symbol held");
  dp_wfm_synth_destroy (s);
  return 0;
}

/* Paced: an empty pipe is an idle frame, all fill; a half-delivered chunk
   is kept and opens the next DATA frame (§4.1). Unpaced never idles. */
static int
test_idle_frames (void)
{
  int p[2];
  DP_REQUIRE (pipe (p) == 0);
  wfm_frame_desc_t d;
  data_frame (&d, 16, 0);
  wfm_data_src_t *src = dp_wfm_data_create_fd (p[0], 16, "10", &why);
  DP_REQUIRE_MSG (src != NULL, why);
  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0xC3, 0x5A }, 2) == 2);
  dp_wfm_synth_state_t *s = line_synth ();
  DP_REQUIRE (dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_PACED, 0)
              == 0);

  /* Frame 1 was drawn at attach. A frame is drawn when its first bit is
     due, so what the pipe holds at THAT moment decides it. */
  uint8_t b[16];
  DP_REQUIRE (read_bits (s, b, 16) == 0);
  static const uint8_t f1[16]
      = { 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };
  DP_CHECK_MSG (memcmp (b, f1, 16) == 0, "frame 1: the data");

  /* Half of the next chunk arrives before frame 2 is due: nothing yet. */
  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0x81 }, 1) == 1);
  DP_REQUIRE (read_bits (s, b, 16) == 0);
  for (size_t i = 0; i < 16; i++)
    DP_CHECK_MSG (b[i] == (uint8_t)((i & 1u) ^ 1u),
                  "frame 2: idle, ALL fill, though 8 bits were waiting");

  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0x7E }, 1) == 1);
  DP_REQUIRE (read_bits (s, b, 16) == 0);
  static const uint8_t f3[16]
      = { 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0 };
  DP_CHECK_MSG (memcmp (b, f3, 16) == 0,
                "frame 3: the bits that waited through the idle frame, "
                "then the rest");

  wfm_data_stats_t st;
  dp_wfm_data_stats (dp_wfm_synth_data_source (s), &st);
  DP_CHECK_MSG (st.frames == 2 && st.idle_frames == 1,
                "the counts are the frames started: frame 4 is not drawn "
                "until its first bit is due");
  close (p[1]);
  dp_wfm_synth_destroy (s);
  close (p[0]);
  return 0;
}

/* 24 bits in 10-bit frames through the synth: octets straddle frames, and
   the pipe's end pads the last (§4.2). */
static int
test_straddle (void)
{
  int p[2];
  DP_REQUIRE (pipe (p) == 0);
  DP_REQUIRE (write (p[1], (const uint8_t[]){ 0xA5, 0x0F, 0xFF }, 3) == 3);
  close (p[1]);
  wfm_frame_desc_t d;
  data_frame (&d, 10, 0);
  wfm_data_src_t *src = dp_wfm_data_create_fd (p[0], 10, "0", &why);
  DP_REQUIRE_MSG (src != NULL, why);
  dp_wfm_synth_state_t *s = line_synth ();
  DP_REQUIRE (dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_UNPACED, 0)
              == 0);
  uint8_t got[36];
  DP_REQUIRE (read_bits (s, got, sizeof got) == 0);
  static const uint8_t want[36]
      = { 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
          1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  DP_CHECK_MSG (memcmp (got, want, 30) == 0,
                "24 bits in 10-bit frames, in order, the last padded");
  DP_CHECK_MSG (dp_wfm_synth_data_ended (s), "then the end");
  dp_wfm_synth_destroy (s);
  close (p[0]);
  return 0;
}

/* A symbol that straddles two frames (a 7-bit frame at qpsk) reads the next
   frame's first bit, exactly as a synth fed the concatenation would. */
static int
test_symbol_straddle (void)
{
  wfm_frame_desc_t d;
  data_frame (&d, 7, 0);
  wfm_data_src_t *src
      = dp_wfm_data_create ("0101100111001011111", NULL, 7, "0", &why);
  DP_REQUIRE_MSG (src != NULL, why);
  dp_wfm_synth_state_t *s = line_synth ();
  DP_REQUIRE (dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_UNPACED, 2)
              == 0);

  /* The reference: 19 bits and 2 of fill, as one pattern, at qpsk. */
  static const uint8_t cat[21]
      = { 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0 };
  dp_wfm_synth_state_t *r = line_synth ();
  DP_REQUIRE (dp_wfm_synth_set_bits (r, cat, 21, 2) == 0);
  float _Complex a[10], b[10];
  dp_wfm_synth_steps (s, a, 10);
  dp_wfm_synth_steps (r, b, 10);
  for (size_t i = 0; i < 10; i++)
    DP_CHECK_MSG (cabsf (a[i] - b[i]) < 1e-3f,
                  "the frames' bits run on across the boundary, symbol "
                  "for symbol");
  dp_wfm_synth_destroy (s);
  dp_wfm_synth_destroy (r);
  return 0;
}

/* What attach refuses, what detaches it, and the state it cannot carry. */
static int
test_contract (void)
{
  wfm_frame_desc_t      plain;
  const wfm_seq_t       lit = { .kind = WFM_SEQ_DOTTED, .len = 8 };
  dp_wfm_synth_state_t *s   = line_synth ();
  DP_REQUIRE (dp_wfm_frame_fixed (&plain, NULL, 0, NULL, &lit, 0) == 0);
  DP_CHECK_MSG (dp_wfm_synth_attach_data (
                    s, &plain, NULL,
                    dp_wfm_data_create ("0xAB", NULL, 8, NULL, &why),
                    WFM_DATA_UNPACED, 0)
                    == -1,
                "a frame with no data field is refused (and the source "
                "freed)");

  dp_wfm_synth_state_t *pn = dp_wfm_synth_create (
      WFM_SYNTH_PN, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  wfm_frame_desc_t d;
  data_frame (&d, 8, 0);
  DP_CHECK_MSG (dp_wfm_synth_attach_data (
                    pn, &d, NULL,
                    dp_wfm_data_create ("0xAB", NULL, 8, NULL, &why),
                    WFM_DATA_UNPACED, 0)
                    == -1,
                "only a type=bits synth is fed frames");
  dp_wfm_synth_destroy (pn);

  DP_REQUIRE (dp_wfm_synth_attach_data (
                  s, &d, NULL,
                  dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why),
                  WFM_DATA_UNPACED, 0)
              == 0);
  DP_CHECK_MSG (dp_wfm_synth_state_bytes (s) == 0,
                "a synth pulling from a source refuses to serialize "
                "(doppler#1681)");
  uint8_t blob[8] = { 0 };
  DP_CHECK_MSG (dp_wfm_synth_set_state (s, blob) == DP_ERR_INVALID,
                "and refuses a blob");
  DP_REQUIRE (dp_wfm_synth_set_bits (s, (const uint8_t[]){ 1, 0 }, 2, 0) == 0);
  DP_CHECK_MSG (dp_wfm_synth_data_source (s) == NULL
                    && dp_wfm_synth_state_bytes (s) > 0,
                "a new pattern detaches the source; the synth serializes "
                "again");
  dp_wfm_synth_destroy (s);
  return 0;
}

int
main (void)
{
  if (test_frames_in_order ())
    return 1;
  if (test_silence_not_held ())
    return 1;
  if (test_idle_frames ())
    return 1;
  if (test_straddle ())
    return 1;
  if (test_symbol_straddle ())
    return 1;
  if (test_contract ())
    return 1;
  DP_TEST_END ("wfm_synth_data");
}
