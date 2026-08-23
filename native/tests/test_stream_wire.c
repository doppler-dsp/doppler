/*
 * test_stream_wire.c — the wire format itself, with no broker involved.
 *
 * The transport's round-trip tests need a live nats-server and skip without
 * one, which meant that until v2 the LAYOUT — the thing a third party
 * implements from `stream.h` — was asserted by nothing at all. That is how
 * the v1 header came to document a `version` value it did not write, and to
 * describe `flags`/`reserved[]` as "do not interpret" while chunking used
 * both. This test runs everywhere, needs nothing, and fails the moment a
 * field moves.
 */
#define DP_TEST_VERBOSE 1
#include "dp_test.h"
#include "stream/stream.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------
 * The 64-byte envelope, field by field.
 * ------------------------------------------------------------------ */
static void
test_header_layout (void)
{
  printf ("-- header layout is the published one\n");

  DP_CHECK (sizeof (dp_header_t) == 64);
  DP_CHECK (offsetof (dp_header_t, magic) == 0);
  DP_CHECK (offsetof (dp_header_t, data_rep) == 8);
  DP_CHECK (offsetof (dp_header_t, format) == 12);
  DP_CHECK (offsetof (dp_header_t, kind) == 14);
  DP_CHECK (offsetof (dp_header_t, version) == 16);
  DP_CHECK (offsetof (dp_header_t, flags) == 18);
  DP_CHECK (offsetof (dp_header_t, payload_bytes) == 20);
  DP_CHECK (offsetof (dp_header_t, sequence) == 24);
  DP_CHECK (offsetof (dp_header_t, timestamp_ns) == 32);
  DP_CHECK (offsetof (dp_header_t, sample_rate) == 40);
  DP_CHECK (offsetof (dp_header_t, center_freq) == 48);
  DP_CHECK (offsetof (dp_header_t, num_samples) == 56);

  DP_CHECK (sizeof (dp_chunk_t) == 24);
  DP_CHECK (offsetof (dp_chunk_t, index) == 0);
  DP_CHECK (offsetof (dp_chunk_t, count) == 4);
  DP_CHECK (offsetof (dp_chunk_t, total_bytes) == 8);
  DP_CHECK (offsetof (dp_chunk_t, offset) == 16);
}

/* ------------------------------------------------------------------
 * The magic is eight readable characters, and it is an integer so that a
 * byte-swapped read fails to match.
 * ------------------------------------------------------------------ */
static void
test_magic (void)
{
  printf ("-- magic spells DPSTREAM and detects a byte swap\n");

  uint64_t m = DP_STREAM_MAGIC;
  char     c[9];
  memcpy (c, &m, 8);
  c[8] = '\0';

  const uint16_t one    = 1u;
  const int      little = *(const unsigned char *)&one;
  if (little)
    DP_CHECK (strcmp (c, "DPSTREAM") == 0);

  /* Byte-swapped, it must NOT match -- that property is the whole reason
     the magic is a uint64_t rather than a char[8]. */
  uint64_t swapped = 0;
  for (int i = 0; i < 8; i++)
    swapped |= ((m >> (8 * i)) & 0xFFu) << (8 * (7 - i));
  DP_CHECK (swapped != m);
}

/* ------------------------------------------------------------------
 * Formats ARE their BLUE codes: mode 'C', then the element type from
 * Midas BLUE 1.1 Table 6. This is the check that keeps one vocabulary.
 * ------------------------------------------------------------------ */
static void
test_blue_format_codes (void)
{
  printf ("-- sample formats are BLUE two-character codes\n");

  const struct
  {
    dp_sample_type_t type;
    char             mode, elem;
    size_t           bytes;
  } table[] = {
    { CI8, 'C', 'B', 2 },  { CI16, 'C', 'I', 4 },  { CI32, 'C', 'L', 8 },
    { CF32, 'C', 'F', 8 }, { CF64, 'C', 'D', 16 },
  };

  for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
    {
      char out[2];
      dp_format_chars (table[i].type, out);
      DP_CHECK (out[0] == table[i].mode);
      DP_CHECK (out[1] == table[i].elem);
      DP_CHECK (dp_sample_size (table[i].type) == table[i].bytes);
      DP_CHECK (dp_sample_type_is_valid (table[i].type));
      DP_CHECK (dp_element_size (DP_KIND_IQ, table[i].type) == table[i].bytes);
    }

  /* A BLUE code doppler does not send is not a doppler format, and neither
     is a value from the enum this format replaced -- v1's CF64 was 1. */
  DP_CHECK (!dp_sample_type_is_valid ((dp_sample_type_t)DP_FMT ('S', 'D')));
  DP_CHECK (!dp_sample_type_is_valid ((dp_sample_type_t)1));
  DP_CHECK (dp_sample_size ((dp_sample_type_t)0) == 0);
}

/* ------------------------------------------------------------------
 * A telemetry frame is a KIND, and its element size does not come from a
 * format code, because it has none.
 * ------------------------------------------------------------------ */
static void
test_frame_kinds (void)
{
  printf ("-- telemetry is a kind, not a format\n");

  DP_CHECK (dp_element_size (DP_KIND_TLM, (dp_sample_type_t)0) == 16);
  /* The format field is ignored for a TLM frame rather than consulted. */
  DP_CHECK (dp_element_size (DP_KIND_TLM, CF64) == 16);
  DP_CHECK (dp_element_size (DP_KIND_IQ, (dp_sample_type_t)0) == 0);
  DP_CHECK (dp_element_size ((dp_frame_kind_t)99, CF64) == 0);
}

/* ------------------------------------------------------------------
 * The byte-order tag is derived from the machine, not compiled in.
 * ------------------------------------------------------------------ */
static void
test_host_rep (void)
{
  printf ("-- data_rep is BLUE's own token for this machine\n");

  const uint16_t one    = 1u;
  const int      little = *(const unsigned char *)&one;
  DP_CHECK (memcmp (dp_host_rep (), little ? DP_REP_LE : DP_REP_BE, 4) == 0);
}

/* ------------------------------------------------------------------
 * The flag mask is what makes a later additive block safe: a receiver
 * must be able to tell "a bit I do not know" from "no bit".
 * ------------------------------------------------------------------ */
static void
test_flag_mask (void)
{
  printf ("-- every known flag is inside DP_FLAG_KNOWN\n");

  DP_CHECK ((DP_FLAG_CHUNKED & DP_FLAG_KNOWN) == DP_FLAG_CHUNKED);
  /* An unassigned bit is outside the mask, which is what a receiver
     rejects on. If a new flag is added without extending the mask, this
     stays true and the receiver refuses the new frames -- the safe way
     round. */
  DP_CHECK ((0x8000u & DP_FLAG_KNOWN) == 0u);
  DP_CHECK (DP_WIRE_VERSION == 2u);
}

int
main (void)
{
  test_header_layout ();
  test_magic ();
  test_blue_format_codes ();
  test_frame_kinds ();
  test_host_rep ();
  test_flag_mask ();

  printf ("\n");
  DP_TEST_END ("test_stream_wire");
}
