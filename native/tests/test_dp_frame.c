/*
 * test_dp_frame.c — the named starter frame set.
 *
 * `dp_frame_test.h` is a TABLE, not an algorithm: five descriptions of the
 * common frame that `dp_wfm_frame_assemble()` materialises. So what is worth
 * testing is not the materialisation — `test_wfm_frame.c` owns that — but the
 * CLAIMS the set makes about itself, because those are what a reader will rely
 * on without re-deriving:
 *
 *   - the bit counts the header's table states, which is what makes a record
 *     length reproducible from a name;
 *   - that RX_FRAME_NONE and RX_FRAME_CONT carry the SAME payload, which is
 *     the whole basis for reading their difference as the frame's effect;
 *   - that RX_FRAME_CONT and RX_FRAME_GOLD have the same geometry and
 *     DIFFERENT bits, which is the whole basis for reading their difference as
 *     the sequence family's effect;
 *   - that no frame's sync word also appears as its own payload, which would
 *     be an ambiguity invented by the test rather than found in the waveform.
 *
 * Each of those is a sentence in the header today. A sentence is not a gate.
 */
#include "dp_frame_test.h"
#include "dp_test.h"

#include <stdio.h>
#include <string.h>

#define CAP 4096

static uint8_t buf[CAP];
static uint8_t alt[CAP];

/* The set's documented totals, in the order of dp_frame_name_t. Repeating the
   header's table here is deliberate: the table is the contract, and a change
   to a length that nobody meant now has to be made twice. */
static const size_t want_bits[DP_FRAME_COUNT] = { 304, 285, 959, 959, 512 };

/* The frame's bits, through the one assembler. */
static size_t
bits (const wfm_frame_desc_t *d, uint8_t *out)
{
  return dp_wfm_frame_assemble (d, NULL, out, CAP);
}

int
main (void)
{
  /* ── the table: every name's geometry is what the header says ─────────── */
  for (int i = 0; i < DP_FRAME_COUNT; i++)
    {
      dp_frame_name_t         nm = (dp_frame_name_t)i;
      wfm_frame_desc_t        f  = dp_frame_named (nm);
      dp_frame_geo_t          l  = dp_frame_geo (&f);
      wfm_frame_desc_layout_t dl;
      size_t                  n;

      DP_REQUIRE_MSG (dp_wfm_frame_desc_layout (&f, &dl) == 0,
                      "every named description lays out");
      DP_REQUIRE_MSG (l.total == want_bits[i], dp_frame_label (nm));
      DP_REQUIRE_MSG (dp_frame_desc_nbits (&f) == want_bits[i],
                      "nbits agrees");

      n = bits (&f, buf);
      DP_REQUIRE_MSG (n == want_bits[i], "the frame materialises in full");
      for (size_t k = 0; k < n; k++)
        DP_REQUIRE_MSG (buf[k] <= 1, "every output bit is 0 or 1");

      /* Same descriptor, same bits — the property that lets a receiver
         regenerate a record's truth from the name alone. */
      DP_REQUIRE_MSG (bits (&f, alt) == n, "rebuild");
      DP_REQUIRE_MSG (memcmp (buf, alt, n) == 0, "a named frame is fixed");

      /* Every preamble in the set is the SAME dotted unit, so a comparison
         between two names is not also a comparison between two acquisition
         targets. */
      for (size_t k = 0; k < l.pre; k++)
        DP_REQUIRE_MSG (buf[l.pre_off + k] == (k % 2 == 0),
                        "the preamble is 1010... at every name that has one");

      /* A frame with a CRC checks when nothing has touched it, and stops
         checking the moment one payload bit moves. That is the truth-free
         detector the frame set exists to make available. */
      if (l.crc)
        {
          DP_REQUIRE_MSG (dp_wfm_frame_desc_crc_ok (&f, buf) == 1,
                          "crc checks");
          buf[l.pay_off] ^= 1u;
          DP_REQUIRE_MSG (dp_wfm_frame_desc_crc_ok (&f, buf) == 0,
                          "one bit fails it");
          buf[l.pay_off] ^= 1u;
        }
      else
        DP_REQUIRE_MSG (dp_wfm_frame_desc_crc_ok (&f, buf) == -1,
                        "an unprotected frame reports -1, never 0");

      /* A payload nobody looks at is the easiest thing in the set to get
         silently wrong -- a generated field whose descriptor does not resolve
         still writes bits, they are just all the same one. Any real sequence
         is roughly balanced; a drained register reads near zero. */
      if (l.pay >= 64)
        {
          size_t ones = 0;
          for (size_t k = 0; k < l.pay; k++)
            ones += buf[l.pay_off + k];
          DP_REQUIRE_MSG (ones > l.pay / 4 && ones < 3 * l.pay / 4,
                          "the payload is a real sequence, not a constant");
        }

      DP_REQUIRE_MSG (dp_frame_label (nm)[0] == 'R', "every name has a label");
    }

  /* ── Barker-13 is the harness's ONE copy of the literal ───────────────── */
  {
    /* The string every caller currently types: gen_wfmgen_flag_matrix.py,
       `wfmgen --sync`'s help, burst_demod's docstrings. Pinning it here is
       what makes RX_FRAME_BURST a replacement for retyping it rather than a
       sixth place it can go wrong. */
    static const char *s = "1111100110101";
    DP_REQUIRE_MSG (strlen (s) == sizeof dp_frame_barker13, "13 symbols");
    for (size_t i = 0; i < sizeof dp_frame_barker13; i++)
      DP_REQUIRE_MSG (dp_frame_barker13[i] == (uint8_t)(s[i] - '0'),
                      "Barker-13 matches the literal every caller types");

    /* And it lands verbatim where the layout says it does. */
    wfm_frame_desc_t f = dp_frame_named (RX_FRAME_BURST);
    dp_frame_geo_t   l = dp_frame_geo (&f);
    DP_REQUIRE_MSG (bits (&f, buf) == l.total, "build");
    DP_REQUIRE_MSG (l.sync == 13, "the burst sync is 13 symbols");
    DP_REQUIRE_MSG (memcmp (buf + l.sync_off, dp_frame_barker13, 13) == 0,
                    "the sync word appears verbatim at its stated offset");
  }

  /* ── NONE vs CONT: the payload is held equal, so the frame is the
   *    variable ─────────────────────────────────────────────────────────────
   */
  {
    wfm_frame_desc_t none = dp_frame_named (RX_FRAME_NONE);
    wfm_frame_desc_t cont = dp_frame_named (RX_FRAME_CONT);
    dp_frame_geo_t   ln = dp_frame_geo (&none), lc = dp_frame_geo (&cont);

    DP_REQUIRE_MSG (ln.pay == lc.pay, "same payload length");

    bits (&none, buf);
    bits (&cont, alt);
    DP_REQUIRE_MSG (memcmp (buf + ln.pay_off, alt + lc.pay_off, ln.pay) == 0,
                    "the baseline carries CONT's payload BIT FOR BIT -- "
                    "without that, their difference is not the frame");

    /* And the baseline really is unframed: nothing but payload. */
    DP_REQUIRE_MSG (ln.pre == 0 && ln.sync == 0 && ln.crc == 0,
                    "RX_FRAME_NONE is an unframed PRBS");
  }

  /* ── CONT vs GOLD: one variable moves, and it really moves ────────────── */
  {
    wfm_frame_desc_t cont = dp_frame_named (RX_FRAME_CONT);
    wfm_frame_desc_t gold = dp_frame_named (RX_FRAME_GOLD);
    dp_frame_geo_t   lc = dp_frame_geo (&cont), lg = dp_frame_geo (&gold);
    size_t           diff = 0;
    const int        cs   = dp_wfm_frame_field_index (&cont, "sync");
    const int        gs   = dp_wfm_frame_field_index (&gold, "sync");

    DP_REQUIRE_MSG (memcmp (&lc, &lg, sizeof lc) == 0,
                    "identical geometry: same offsets, same widths, same "
                    "total -- only the sequence family differs");
    DP_REQUIRE_MSG (cs >= 0 && gs >= 0 && cont.field[cs].seq.kind == WFM_SEQ_PN
                        && gold.field[gs].seq.kind == WFM_SEQ_GOLD,
                    "and the family is what differs");

    bits (&cont, buf);
    bits (&gold, alt);
    for (size_t i = 0; i < lc.sync; i++)
      diff += (buf[lc.sync_off + i] != alt[lg.sync_off + i]);
    /* Two independent 127-bit sequences differ in ~half their positions. A
       handful would mean the two descriptors are producing nearly the same
       bits, and the comparison the pair exists for would be measuring
       nothing. */
    DP_REQUIRE_MSG (diff > lc.sync / 4,
                    "the two sync words are genuinely different sequences");
  }

  /* ── a sync word must not also be its own payload ─────────────────────── */
  {
    dp_frame_name_t named[2] = { RX_FRAME_CONT, RX_FRAME_GOLD };
    for (int i = 0; i < 2; i++)
      {
        wfm_frame_desc_t f     = dp_frame_named (named[i]);
        dp_frame_geo_t   l     = dp_frame_geo (&f);
        int              found = 0;

        bits (&f, buf);
        /* The marker a receiver hunts for is the sync word. If the same run
           of bits also occurs inside the payload, every detection has a
           competitor that the WAVEFORM does not have -- an ambiguity the test
           set invented, which would then be measured as the receiver's. */
        for (size_t off = 0; off + l.sync <= l.pay; off++)
          if (memcmp (buf + l.sync_off, buf + l.pay_off + off, l.sync) == 0)
            found = 1;
        DP_REQUIRE_MSG (!found,
                        "the sync word does not recur inside the payload");
      }
  }

  /* ── outside the set builds nothing, rather than something plausible ──── */
  {
    wfm_frame_desc_t f = dp_frame_named ((dp_frame_name_t)DP_FRAME_COUNT);
    DP_REQUIRE_MSG (dp_frame_desc_nbits (&f) == 0,
                    "an unknown name is 0 bits");
    DP_REQUIRE_MSG (bits (&f, buf) == 0, "and writes nothing");
    DP_REQUIRE_MSG (
        strcmp (dp_frame_label ((dp_frame_name_t)DP_FRAME_COUNT), "?") == 0,
        "and has no label to quote in a report");
  }

  printf (
      "test_dp_frame: OK (5 named frames, stated geometry, Barker-13, "
      "NONE==CONT payload, CONT/GOLD one variable, sync not in payload)\n");
  return 0;
}
