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

/* dp_frame_parse: the receive-side rules, reachable without a broker. */
#include "../src/stream/stream_internal.h"

#include <complex.h>
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
    { CI8, 'C', 'B', 2 },
    { CI16, 'C', 'I', 4 },
    { CI32, 'C', 'L', 8 },
    { CF32, 'C', 'F', 8 },
    { CF64, 'C', 'D', 16 },
    /* The scalar half of the mode axis: same five element encodings, one
       component instead of two, so each is exactly half the bytes
       (doppler#1032). */
    { SI8, 'S', 'B', 1 },
    { SI16, 'S', 'I', 2 },
    { SI32, 'S', 'L', 4 },
    { SF32, 'S', 'F', 4 },
    { SF64, 'S', 'D', 8 },
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
     is a value from the enum this format replaced -- v1's CF64 was 1.
     `DP_FMT('S','D')` used to be the example here and is a real format now,
     so the two examples below are one unknown MODE and one unknown ELEMENT
     -- both halves of the code, since either alone can be wrong. */
  DP_CHECK (!dp_sample_type_is_valid ((dp_sample_type_t)DP_FMT ('V', 'F')));
  DP_CHECK (!dp_sample_type_is_valid ((dp_sample_type_t)DP_FMT ('C', 'X')));
  DP_CHECK (!dp_sample_type_is_valid ((dp_sample_type_t)1));
  DP_CHECK (dp_sample_size ((dp_sample_type_t)0) == 0);

  /* Every scalar format is exactly half its complex twin. */
  DP_CHECK (dp_sample_size (SF32) * 2u == dp_sample_size (CF32));
  DP_CHECK (dp_sample_size (SI8) * 2u == dp_sample_size (CI8));
  DP_CHECK (dp_format_components (SF32) == 1u);
  DP_CHECK (dp_format_components (CF32) == 2u);
  /* An unknown code has no component count either -- 0 is distinguishable
     from both 1 and 2, which is why it is not defaulted to complex. */
  DP_CHECK (dp_format_components ((dp_sample_type_t)DP_FMT ('V', 'F')) == 0u);

  /* Full scale is a property of the ELEMENT, so a scalar format and its
     complex twin agree: the divisor that puts an integer format on the same
     footing as a float one does not care how many components a sample has. */
  DP_CHECK (dp_format_full_scale (SI8) == dp_format_full_scale (CI8));
  DP_CHECK (dp_format_full_scale (SI16) == dp_format_full_scale (CI16));
  DP_CHECK (dp_format_full_scale (SI32) == dp_format_full_scale (CI32));
  DP_CHECK (dp_format_full_scale (SF32) == 1.0);
  DP_CHECK (dp_format_full_scale (SF64) == 1.0);
  DP_CHECK (dp_format_full_scale ((dp_sample_type_t)DP_FMT ('V', 'F')) == 0.0);
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

  printf ("-- end of stream is a kind too, and carries nothing\n");

  /* EOS has no format and no elements: it is a statement, so asking its
     element size is asking the wrong question and gets 0 rather than a
     plausible number. */
  DP_CHECK (dp_element_size (DP_KIND_EOS, (dp_sample_type_t)0) == 0);
  DP_CHECK (dp_element_size (DP_KIND_EOS, CF64) == 0);

  /* A KIND rather than a flag, and the numbering is part of the wire
     contract: a receiver validates `flags` against DP_FLAG_KNOWN and
     refuses anything outside it, but does NOT validate `kind` -- which is
     precisely what makes a new kind additive and a new flag breaking. */
  DP_CHECK (DP_KIND_IQ == 0);
  DP_CHECK (DP_KIND_TLM == 1);
  DP_CHECK (DP_KIND_EOS == 2);

  /* Announcing the end of a stream that does not exist is a caller error,
     not a silent success: there is no context to say it on. */
  DP_CHECK (dp_pub_send_eos (NULL) == DP_ERR_INVALID);

  printf ("-- an ending is checked, not merely exempted\n");

  /* EOS is the one kind that skips the element-size arithmetic, so every
     field the kind FIXES has to be checked here instead -- otherwise it
     would also be the one kind whose header nothing validates at all. */
  char        buf[sizeof (dp_header_t) + 64];
  dp_header_t h;
  dp_chunk_t  ch;
  int         chunked = 0;
  const void *body    = NULL;
  size_t      blen    = 0;

  dp_header_t e = { 0 };
  e.magic       = DP_STREAM_MAGIC;
  memcpy (e.data_rep, dp_host_rep (), 4);
  e.kind    = (uint16_t)DP_KIND_EOS;
  e.version = DP_WIRE_VERSION;

  /* The well-formed ending: nothing claimed, nothing carried. */
  memset (buf, 0, sizeof buf);
  memcpy (buf, &e, sizeof e);
  DP_CHECK_MSG (dp_frame_parse (buf, sizeof e, &h, &ch, &chunked, &body, &blen)
                    == DP_OK,
                "a zero-payload end-of-stream frame is well-formed");
  DP_CHECK (blen == 0);

  /* Claiming samples it cannot be carrying. */
  dp_header_t bad = e;
  bad.num_samples = 4;
  memset (buf, 0, sizeof buf);
  memcpy (buf, &bad, sizeof bad);
  DP_CHECK_MSG (
      dp_frame_parse (buf, sizeof bad, &h, &ch, &chunked, &body, &blen)
          == DP_ERR_INVALID,
      "an ending that claims samples is a contradiction, not a "
      "frame to interpret");

  /* Carrying a payload it says nothing about. */
  bad               = e;
  bad.payload_bytes = 16;
  memset (buf, 0, sizeof buf);
  memcpy (buf, &bad, sizeof bad);
  DP_CHECK_MSG (
      dp_frame_parse (buf, sizeof bad + 16, &h, &ch, &chunked, &body, &blen)
          == DP_ERR_INVALID,
      "an ending carries nothing, so a payload on one is refused");

  /* Naming a sample type. The header says format is 0 for an ending; that
     claim is only worth making if something enforces it. */
  bad        = e;
  bad.format = (uint16_t)CF64;
  memset (buf, 0, sizeof buf);
  memcpy (buf, &bad, sizeof bad);
  DP_CHECK_MSG (
      dp_frame_parse (buf, sizeof bad, &h, &ch, &chunked, &body, &blen)
          == DP_ERR_INVALID,
      "an ending has no sample type, so a frame naming one is not "
      "the ending it claims to be");
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

/* ------------------------------------------------------------------
 * dp_mean_power: one formula, every wire format, and the normalisation
 * that makes the answers comparable.
 * ------------------------------------------------------------------ */
static void
test_mean_power (void)
{
  printf ("-- mean power is full-scale normalised in every format\n");

  /* A unit-magnitude complex value is full scale in every format, so all
     five must report 1.0 -- that IS the normalisation's contract. */
  double _Complex cf64[4] = { 1, I, -1, -I };
  DP_CHECK (dp_near (dp_mean_power (CF64, cf64, 4), 1.0, 1e-12));

  float _Complex cf32[4] = { 1, I, -1, -I };
  DP_CHECK (dp_near (dp_mean_power (CF32, cf32, 4), 1.0, 1e-6));

  int8_t ci8[4] = { 127, 0, 0, -127 };
  DP_CHECK (dp_near (dp_mean_power (CI8, ci8, 2), 1.0, 1e-9));

  int16_t ci16[4] = { 32767, 0, 0, -32767 };
  DP_CHECK (dp_near (dp_mean_power (CI16, ci16, 2), 1.0, 1e-9));

  int32_t ci32[4] = { 2147483647, 0, 0, -2147483647 };
  DP_CHECK (dp_near (dp_mean_power (CI32, ci32, 2), 1.0, 1e-9));

  /* Half scale is a quarter of the power, in the format that quantises. */
  int16_t half[2] = { 16384, 0 };
  DP_CHECK (dp_near (dp_mean_power (CI16, half, 1), 0.25, 1e-3));

  /* Both components count: I and Q at full scale is 2.0, not 1.0. */
  double _Complex both[1] = { 1 + 1 * I };
  DP_CHECK (dp_near (dp_mean_power (CF64, both, 1), 2.0, 1e-12));

  /* Nothing to measure, nothing to crash on. */
  DP_CHECK (dp_mean_power (CF64, NULL, 4) == 0.0);
  DP_CHECK (dp_mean_power (CF64, cf64, 0) == 0.0);
  DP_CHECK (dp_mean_power ((dp_sample_type_t)0, cf64, 4) == 0.0);
}

/* ------------------------------------------------------------------
 * dp_frame_parse: the five checks a receiver makes. v1 made one of them,
 * which is how a header could claim more samples than its message
 * carried and be believed.
 * ------------------------------------------------------------------ */
static size_t
build_frame (char *buf, size_t cap, size_t nsamples)
{
  dp_header_t h = { 0 };
  h.magic       = DP_STREAM_MAGIC;
  memcpy (h.data_rep, dp_host_rep (), 4);
  h.format        = (uint16_t)CF64;
  h.kind          = (uint16_t)DP_KIND_IQ;
  h.version       = DP_WIRE_VERSION;
  h.flags         = 0;
  h.num_samples   = nsamples;
  h.payload_bytes = (uint32_t)(nsamples * dp_sample_size (CF64));

  size_t total = sizeof h + h.payload_bytes;
  if (total > cap)
    return 0;
  memset (buf, 0, total);
  memcpy (buf, &h, sizeof h);
  return total;
}

static void
test_frame_parse (void)
{
  printf ("-- a receiver checks five things, not one\n");

  char        buf[512];
  size_t      len = build_frame (buf, sizeof buf, 4);
  dp_header_t h;
  dp_chunk_t  ch;
  int         chunked = 0;
  const void *body    = NULL;
  size_t      blen    = 0;

  DP_CHECK (len > 0);
  if (len == 0)
    return; /* the fixture itself is broken; the checks below are moot */
  DP_CHECK (dp_frame_parse (buf, len, &h, &ch, &chunked, &body, &blen)
            == DP_OK);
  DP_CHECK (chunked == 0);
  DP_CHECK (blen == 4 * sizeof (double _Complex));
  DP_CHECK ((const char *)body == buf + sizeof (dp_header_t));

  /* 1. the magic */
  char bad[512];
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->magic ^= 1u;
  DP_CHECK (dp_frame_parse (bad, len, &h, &ch, &chunked, &body, &blen)
            == DP_ERR_INVALID);

  /* 2. the version -- a v1 frame must not be read as a v2 one */
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->version = 1;
  DP_CHECK (dp_frame_parse (bad, len, &h, &ch, &chunked, &body, &blen)
            == DP_ERR_INVALID);

  /* 3. an unknown flag bit: it would move where the payload starts, so it
        is refused rather than ignored -- the property that lets a later
        additive block be introduced safely. */
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->flags |= 0x8000u;
  DP_CHECK (dp_frame_parse (bad, len, &h, &ch, &chunked, &body, &blen)
            == DP_ERR_INVALID);

  /* 4. the header's length claim against the transport's truth. THIS is
        the out-of-bounds read: a frame claiming a longer payload than it
        carries was believed, and both faces then read past the buffer. */
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->payload_bytes += 16;
  DP_CHECK (dp_frame_parse (bad, len, &h, &ch, &chunked, &body, &blen)
            == DP_ERR_INVALID);

  /* 5. num_samples against that same length */
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->num_samples += 1;
  DP_CHECK (dp_frame_parse (bad, len, &h, &ch, &chunked, &body, &blen)
            == DP_ERR_INVALID);

  /* An unknown format has no element size, so no length can be right. */
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->format = 0;
  DP_CHECK (dp_frame_parse (bad, len, &h, &ch, &chunked, &body, &blen)
            == DP_ERR_INVALID);

  /* Truncated before the header ends. */
  DP_CHECK (dp_frame_parse (buf, sizeof (dp_header_t) - 1, &h, &ch, &chunked,
                            &body, &blen)
            == DP_ERR_INVALID);

  /* Says CHUNKED but carries no chunk block. */
  memcpy (bad, buf, len);
  ((dp_header_t *)bad)->flags |= DP_FLAG_CHUNKED;
  DP_CHECK (dp_frame_parse (bad, sizeof (dp_header_t), &h, &ch, &chunked,
                            &body, &blen)
            == DP_ERR_INVALID);

  /* A well-formed chunked frame: the block is consumed and the payload
     starts after it. */
  dp_header_t ch_hdr = { 0 };
  memcpy (&ch_hdr, buf, sizeof ch_hdr);
  ch_hdr.flags |= DP_FLAG_CHUNKED;
  ch_hdr.num_samples   = 2;
  ch_hdr.payload_bytes = (uint32_t)(2 * sizeof (double _Complex));
  dp_chunk_t blk       = { 0 };
  blk.index            = 0;
  blk.count            = 2;
  blk.total_bytes      = 4 * sizeof (double _Complex);
  blk.offset           = 0;

  char   cbuf[512];
  size_t off = 0;
  memcpy (cbuf + off, &ch_hdr, sizeof ch_hdr);
  off += sizeof ch_hdr;
  memcpy (cbuf + off, &blk, sizeof blk);
  off += sizeof blk;
  memset (cbuf + off, 0, ch_hdr.payload_bytes);
  off += ch_hdr.payload_bytes;

  DP_CHECK (dp_frame_parse (cbuf, off, &h, &ch, &chunked, &body, &blen)
            == DP_OK);
  DP_CHECK (chunked == 1);
  DP_CHECK (ch.count == 2);
  DP_CHECK (ch.total_bytes == 4 * sizeof (double _Complex));
  DP_CHECK ((const char *)body
            == cbuf + sizeof (dp_header_t) + sizeof (dp_chunk_t));
}

/* ------------------------------------------------------------------
 * The guards a caller hits without a broker anywhere in sight.
 * ------------------------------------------------------------------ */
static void
test_argument_guards (void)
{
  printf ("-- bad arguments are refused before anything connects\n");

  /* An invalid format is refused at CONSTRUCTION, not at the first send:
     the socket declares what it will carry, so a retired or unknown code
     never reaches the wire. No broker is contacted to find this out. */
  DP_CHECK (dp_pub_create ("nats://127.0.0.1:4222/x", (dp_sample_type_t)0)
            == NULL);
  DP_CHECK (dp_pub_create ("nats://127.0.0.1:4222/x", (dp_sample_type_t)1)
            == NULL); /* v1's CF64 is not a v2 format */
  DP_CHECK (dp_pub_create (NULL, CF64) == NULL);
  DP_CHECK (dp_sub_create (NULL) == NULL);

  double _Complex x[2] = { 1, 2 };
  DP_CHECK (dp_pub_send_cf64 (NULL, x, 2, 0.0, 0.0) == DP_ERR_INVALID);
  DP_CHECK (dp_sub_recv (NULL, NULL, NULL) == DP_ERR_INVALID);

  /* The NULL-handle answers, which every accessor owes. */
  DP_CHECK (dp_pub_flush (NULL, 100) == DP_ERR_INVALID);
  DP_CHECK (dp_msg_mean_power (NULL) == 0.0);
  DP_CHECK (dp_msg_kind (NULL) == DP_KIND_IQ);
  DP_CHECK (dp_msg_num_samples (NULL) == 0);
  DP_CHECK (dp_msg_data (NULL) == NULL);
  dp_msg_free (NULL); /* must not crash */

  DP_CHECK (strcmp (dp_sample_type_str ((dp_sample_type_t)0), "UNKNOWN") == 0);
  DP_CHECK (strcmp (dp_strerror (DP_ERR_TOO_LARGE),
                    "Frame exceeds transport max_payload")
            == 0);
}

/* ------------------------------------------------------------------
 * The interrupt flag itself. Whether a blocked receive returns is a
 * broker test (test_stream_nats_core); that the flag is sticky, and
 * that a receive started while it is set refuses immediately, is not.
 * ------------------------------------------------------------------ */
static void
test_interrupt_flag (void)
{
  printf ("-- the interrupt flag is sticky and pre-checked\n");

  dp_stream_resume ();
  DP_CHECK (!dp_stream_interrupted ());

  dp_stream_interrupt ();
  DP_CHECK (dp_stream_interrupted ());

  /* Sticky: reading it does not clear it, so several parked loops all
     see one handler's signal rather than the first one eating it. */
  DP_CHECK (dp_stream_interrupted ());
  DP_CHECK (dp_stream_interrupted ());

  dp_stream_resume ();
  DP_CHECK (!dp_stream_interrupted ());

  /* The latency is the caller's, not a constant baked into the wait. */
  DP_CHECK (dp_stream_interrupt_latency_ms ()
            == DP_INTERRUPT_LATENCY_DEFAULT_MS);
  dp_stream_set_interrupt_latency_ms (7);
  DP_CHECK (dp_stream_interrupt_latency_ms () == 7);
  dp_stream_set_interrupt_latency_ms (0); /* 0 means "the default" */
  DP_CHECK (dp_stream_interrupt_latency_ms ()
            == DP_INTERRUPT_LATENCY_DEFAULT_MS);

  DP_CHECK (strcmp (dp_strerror (DP_ERR_INTERRUPTED),
                    "Interrupted by dp_stream_interrupt")
            == 0);
  /* An interrupt is a request to stop, not a failure, and a caller that
     lumps it in with DP_ERR_RECV would report a crash for a Ctrl+C. */
  DP_CHECK (DP_ERR_INTERRUPTED != DP_ERR_TIMEOUT);
  DP_CHECK (DP_ERR_INTERRUPTED != DP_ERR_RECV);
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
  test_mean_power ();
  test_frame_parse ();
  test_argument_guards ();
  test_interrupt_flag ();

  printf ("\n");
  DP_TEST_END ("test_stream_wire");
}
