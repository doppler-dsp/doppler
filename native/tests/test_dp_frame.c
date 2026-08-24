/*
 * test_dp_frame.c — the named starter frame set.
 *
 * `dp_frame_test.h` is a TABLE, not an algorithm: five `wfm_frame_t` values
 * that `wfm_frame_bits()` materialises. So what is worth testing is not the
 * materialisation — `test_wfm_frame.c` owns that — but the CLAIMS the set
 * makes about itself, because those are what a reader will rely on without
 * re-deriving:
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

int
main (void)
{
  /* ── the table: every name's geometry is what the header says ─────────── */
  for (int i = 0; i < DP_FRAME_COUNT; i++)
    {
      dp_frame_name_t    nm = (dp_frame_name_t)i;
      wfm_frame_t        f  = dp_frame_named (nm);
      wfm_frame_layout_t l;
      size_t             n;

      DP_REQUIRE_MSG (wfm_frame_layout (&f, &l) == 0, "layout");
      DP_REQUIRE_MSG (l.total_bits == want_bits[i], dp_frame_label (nm));
      DP_REQUIRE_MSG (wfm_frame_nbits (&f) == want_bits[i], "nbits agrees");

      n = wfm_frame_bits (&f, buf, CAP);
      DP_REQUIRE_MSG (n == want_bits[i], "the frame materialises in full");
      for (size_t k = 0; k < n; k++)
        DP_REQUIRE_MSG (buf[k] <= 1, "every output bit is 0 or 1");

      /* Same descriptor, same bits — the property that lets a receiver
         regenerate a record's truth from the name alone. */
      DP_REQUIRE_MSG (wfm_frame_bits (&f, alt, CAP) == n, "rebuild");
      DP_REQUIRE_MSG (memcmp (buf, alt, n) == 0, "a named frame is fixed");

      /* Every preamble in the set is the SAME dotted unit, so a comparison
         between two names is not also a comparison between two acquisition
         targets. */
      for (size_t k = 0; k < l.preamble_bits; k++)
        DP_REQUIRE_MSG (buf[l.preamble_off + k] == (k % 2 == 0),
                        "the preamble is 1010... at every name that has one");

      /* A frame with a CRC checks when nothing has touched it, and stops
         checking the moment one payload bit moves. That is the truth-free
         detector the frame set exists to make available. */
      if (l.crc_bits)
        {
          DP_REQUIRE_MSG (wfm_frame_crc_ok (&f, buf) == 1, "crc checks");
          buf[l.payload_off] ^= 1u;
          DP_REQUIRE_MSG (wfm_frame_crc_ok (&f, buf) == 0, "one bit fails it");
          buf[l.payload_off] ^= 1u;
        }
      else
        DP_REQUIRE_MSG (wfm_frame_crc_ok (&f, buf) == -1,
                        "an unprotected frame reports -1, never 0");

      /* A payload nobody looks at is the easiest thing in the set to get
         silently wrong -- a generated field whose descriptor does not resolve
         still writes bits, they are just all the same one. Any real sequence
         is roughly balanced; a drained register reads near zero. */
      if (l.payload_bits >= 64)
        {
          size_t ones = 0;
          for (size_t k = 0; k < l.payload_bits; k++)
            ones += buf[l.payload_off + k];
          DP_REQUIRE_MSG (ones > l.payload_bits / 4
                              && ones < 3 * l.payload_bits / 4,
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
    wfm_frame_t        f = dp_frame_named (RX_FRAME_BURST);
    wfm_frame_layout_t l;
    wfm_frame_layout (&f, &l);
    DP_REQUIRE_MSG (wfm_frame_bits (&f, buf, CAP) == l.total_bits, "build");
    DP_REQUIRE_MSG (l.sync_bits == 13, "the burst sync is 13 symbols");
    DP_REQUIRE_MSG (memcmp (buf + l.sync_off, dp_frame_barker13, 13) == 0,
                    "the sync word appears verbatim at its stated offset");
  }

  /* ── NONE vs CONT: the payload is held equal, so the frame is the
   *    variable ─────────────────────────────────────────────────────────────
   */
  {
    wfm_frame_t        none = dp_frame_named (RX_FRAME_NONE);
    wfm_frame_t        cont = dp_frame_named (RX_FRAME_CONT);
    wfm_frame_layout_t ln, lc;

    wfm_frame_layout (&none, &ln);
    wfm_frame_layout (&cont, &lc);
    DP_REQUIRE_MSG (ln.payload_bits == lc.payload_bits, "same payload length");

    wfm_frame_bits (&none, buf, CAP);
    wfm_frame_bits (&cont, alt, CAP);
    DP_REQUIRE_MSG (
        memcmp (buf + ln.payload_off, alt + lc.payload_off, ln.payload_bits)
            == 0,
        "the baseline carries CONT's payload BIT FOR BIT -- "
        "without that, their difference is not the frame");

    /* And the baseline really is unframed: nothing but payload. */
    DP_REQUIRE_MSG (ln.preamble_bits == 0 && ln.sync_bits == 0
                        && ln.crc_bits == 0,
                    "RX_FRAME_NONE is an unframed PRBS");
  }

  /* ── CONT vs GOLD: one variable moves, and it really moves ────────────── */
  {
    wfm_frame_t        cont = dp_frame_named (RX_FRAME_CONT);
    wfm_frame_t        gold = dp_frame_named (RX_FRAME_GOLD);
    wfm_frame_layout_t lc, lg;
    size_t             diff = 0;

    wfm_frame_layout (&cont, &lc);
    wfm_frame_layout (&gold, &lg);
    DP_REQUIRE_MSG (memcmp (&lc, &lg, sizeof lc) == 0,
                    "identical geometry: same offsets, same widths, same "
                    "total -- only the sequence family differs");
    DP_REQUIRE_MSG (cont.sync.kind == WFM_SEQ_PN
                        && gold.sync.kind == WFM_SEQ_GOLD,
                    "and the family is what differs");

    wfm_frame_bits (&cont, buf, CAP);
    wfm_frame_bits (&gold, alt, CAP);
    for (size_t i = 0; i < lc.sync_bits; i++)
      diff += (buf[lc.sync_off + i] != alt[lg.sync_off + i]);
    /* Two independent 127-bit sequences differ in ~half their positions. A
       handful would mean the two descriptors are producing nearly the same
       bits, and the comparison the pair exists for would be measuring
       nothing. */
    DP_REQUIRE_MSG (diff > lc.sync_bits / 4,
                    "the two sync words are genuinely different sequences");
  }

  /* ── a sync word must not also be its own payload ─────────────────────── */
  {
    dp_frame_name_t named[2] = { RX_FRAME_CONT, RX_FRAME_GOLD };
    for (int i = 0; i < 2; i++)
      {
        wfm_frame_t        f = dp_frame_named (named[i]);
        wfm_frame_layout_t l;
        int                found = 0;

        wfm_frame_layout (&f, &l);
        wfm_frame_bits (&f, buf, CAP);
        /* The marker a receiver hunts for is the sync word. If the same run
           of bits also occurs inside the payload, every detection has a
           competitor that the WAVEFORM does not have -- an ambiguity the test
           set invented, which would then be measured as the receiver's. */
        for (size_t off = 0; off + l.sync_bits <= l.payload_bits; off++)
          if (memcmp (buf + l.sync_off, buf + l.payload_off + off, l.sync_bits)
              == 0)
            found = 1;
        DP_REQUIRE_MSG (!found,
                        "the sync word does not recur inside the payload");
      }
  }

  /* ── outside the set builds nothing, rather than something plausible ──── */
  {
    wfm_frame_t f = dp_frame_named ((dp_frame_name_t)DP_FRAME_COUNT);
    DP_REQUIRE_MSG (wfm_frame_nbits (&f) == 0, "an unknown name is 0 bits");
    DP_REQUIRE_MSG (wfm_frame_bits (&f, buf, CAP) == 0, "and writes nothing");
    DP_REQUIRE_MSG (
        strcmp (dp_frame_label ((dp_frame_name_t)DP_FRAME_COUNT), "?") == 0,
        "and has no label to quote in a report");
  }

  printf (
      "test_dp_frame: OK (5 named frames, stated geometry, Barker-13, "
      "NONE==CONT payload, CONT/GOLD one variable, sync not in payload)\n");
  return 0;
}
