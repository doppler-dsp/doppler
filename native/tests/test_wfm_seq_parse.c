/*
 * test_wfm_seq_parse.c — the one text spelling of a frame field.
 *
 * The grammar exists so that a field is "a selection or a user-defined
 * pattern" on every face, and so that adding a kind reaches every field at
 * once. What is worth pinning here is therefore not the arithmetic — the
 * generated bits are wfm_frame.c's, tested there — but the four things a
 * parser gets silently wrong:
 *
 *   - the BARE form still parses, because every committed scene file and
 *     every wfmgen invocation in the goldens uses it. This grammar is a
 *     superset or it is a migration;
 *   - a malformed spec is REFUSED. It replaced two parsers that disagreed:
 *     wfm_json's string_to_bits() skipped a stray character and wfmgen's
 *     parse_bit_string() refused it, so `--sync 1O11` (letter O) silently
 *     became a 3-bit sync word on one face and an error on the other;
 *   - an unknown KEY is refused, not ignored, or `reg_bits=7` on a PN field
 *     leaves reg at 0 and the error names the register instead of the typo;
 *   - an absent field is not an error, because that is how a frame says it
 *     carries no preamble.
 */
#define _GNU_SOURCE
#include "dp_test.h"
#include "wfm/wfm_frame.h"
#include "wfm/wfm_seq_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The two helpers return int, not the parsed value: DP_REQUIRE_MSG expands to
   `return 1`, so it is "only valid where `return 1` is" (dp_test.h) and main
   does the asserting. */

/* 0 when `spec` parsed. Fills *s and *owned either way. */
static int
parses (const char *spec, wfm_seq_t *s, uint8_t **owned)
{
  const char *err = NULL;
  return wfm_seq_parse (spec, s, owned, &err);
}

/* Non-zero when `spec` is refused WITH a message and allocates nothing — a
   silent -1 would be as hard to act on as the truncation this replaced. */
static int
refuses (const char *spec)
{
  wfm_seq_t   s     = { 0 };
  uint8_t    *owned = NULL;
  const char *err   = NULL;
  int         rc    = wfm_seq_parse (spec, &s, &owned, &err);
  return rc == -1 && err != NULL && *err != '\0' && owned == NULL;
}

/* Refused, AND the message contains `want`.
 *
 * This distinction is not pedantry, it is the whole value of the unknown-key
 * check, and it was measured: with kv_only() disabled, `pn:len=127,reg_bits=7`
 * is STILL refused — `reg` is simply never found, defaults to 0, and the range
 * check rejects it. So `refuses()` alone passes with the typo detection gone,
 * and the caller is told their register width is out of range when what they
 * actually did was misspell a key. Only the message separates the two. */
static int
refuses_saying (const char *spec, const char *want)
{
  wfm_seq_t   s     = { 0 };
  uint8_t    *owned = NULL;
  const char *err   = NULL;
  int         rc    = wfm_seq_parse (spec, &s, &owned, &err);
  return rc == -1 && err != NULL && strstr (err, want) != NULL;
}

int
main (void)
{
  /* ── the bare form: a superset, not a migration ────────────────────────*/
  {
    wfm_seq_t s     = { 0 };
    uint8_t  *owned = NULL;
    DP_REQUIRE_MSG (parses ("1111100110101", &s, &owned) == 0, "bare");
    DP_REQUIRE_MSG (s.kind == WFM_SEQ_LITERAL, "bare is literal");
    DP_REQUIRE_MSG (s.len == 13, "Barker-13");
    DP_REQUIRE_MSG (s.bits && s.bits[0] == 1 && s.bits[5] == 0,
                    "the bits are the string");
    DP_REQUIRE_MSG (owned == s.bits, "the literal is owned by the caller");
    free (owned);
  }

  /* ── absent is not an error ────────────────────────────────────────────*/
  {
    wfm_seq_t   s     = { 0 };
    uint8_t    *owned = NULL;
    const char *err   = NULL;
    DP_REQUIRE_MSG (wfm_seq_parse (NULL, &s, &owned, &err) == 0, "NULL");
    DP_REQUIRE_MSG (s.len == 0 && owned == NULL, "absent");
    DP_REQUIRE_MSG (wfm_seq_parse ("", &s, &owned, &err) == 0, "empty");
    DP_REQUIRE_MSG (s.len == 0 && owned == NULL, "also absent");
  }

  /* ── hex, both spellings, MSB-first ────────────────────────────────────*/
  {
    wfm_seq_t s     = { 0 };
    uint8_t  *owned = NULL;
    DP_REQUIRE_MSG (parses ("hex:A5", &s, &owned) == 0, "hex:A5");
    DP_REQUIRE_MSG (s.len == 8, "two digits, eight bits");
    /* 0xA5 = 1010 0101 */
    const uint8_t want[8] = { 1, 0, 1, 0, 0, 1, 0, 1 };
    DP_REQUIRE_MSG (memcmp (s.bits, want, 8) == 0, "MSB-first per digit");
    free (owned);

    wfm_seq_t t      = { 0 };
    uint8_t  *owned2 = NULL;
    DP_REQUIRE_MSG (parses ("0xA5", &t, &owned2) == 0, "0xA5");
    DP_REQUIRE_MSG (t.len == 8 && memcmp (t.bits, want, 8) == 0,
                    "0x is the same literal, so an existing --bits-hex "
                    "value keeps working unprefixed");
    free (owned2);
  }

  /* ── the generated kinds carry parameters, not bits ────────────────────*/
  {
    wfm_seq_t s     = { 0 };
    uint8_t  *owned = NULL;
    DP_REQUIRE_MSG (parses ("pn:len=127,reg=7", &s, &owned) == 0, "pn");
    DP_REQUIRE_MSG (s.kind == WFM_SEQ_PN, "pn");
    DP_REQUIRE_MSG (s.len == 127 && s.reg_bits == 7, "len and reg");
    DP_REQUIRE_MSG (s.bits == NULL && owned == NULL,
                    "a generated field allocates nothing — that IS the point, "
                    "a million-symbol truth without a million-symbol array");
    DP_REQUIRE_MSG (s.poly == 0 && s.seed == 0,
                    "omitted means the descriptor's own default, not a "
                    "value invented here");

    wfm_seq_t t = { 0 };
    uint8_t *ot = NULL;
    DP_REQUIRE_MSG (
        parses ("pn:len=8,reg=5,poly=0x12,seed=3,lfsr=fibonacci", &t, &ot) == 0,
        "pn with every key");
    DP_REQUIRE_MSG (t.poly == 0x12, "hex value");
    DP_REQUIRE_MSG (t.seed == 3 && t.lfsr == 1, "seed and realization");

    wfm_seq_t g = { 0 };
    uint8_t *og = NULL;
    DP_REQUIRE_MSG (
        parses ("gold:len=127,reg=10,taps_a=5,seed_b=9", &g, &og) == 0, "gold");
    DP_REQUIRE_MSG (g.kind == WFM_SEQ_GOLD, "gold");
    DP_REQUIRE_MSG (g.taps_a == 5 && g.seed_b == 9, "both registers");

    wfm_seq_t d = { 0 };
    uint8_t *od = NULL;
    DP_REQUIRE_MSG (parses ("dotted:len=64", &d, &od) == 0, "dotted");
    DP_REQUIRE_MSG (d.kind == WFM_SEQ_DOTTED && d.len == 64,
                    "dotted — the kind every named test frame's preamble "
                    "uses and which no face could spell");
  }

  /* ── a spec round-trips through the descriptor it describes ────────────*/
  {
    wfm_seq_t sync = { 0 }, pay = { 0 };
    uint8_t  *o1 = NULL, *o2 = NULL;
    DP_REQUIRE_MSG (parses ("1111100110101", &sync, &o1) == 0, "sync");
    DP_REQUIRE_MSG (parses ("pn:len=64,reg=7", &pay, &o2) == 0, "payload");

    wfm_frame_t f = { 0 };
    f.sync        = sync;
    f.payload     = pay;
    f.crc         = 1;
    size_t nb     = wfm_frame_nbits (&f);
    DP_REQUIRE_MSG (nb == 13 + 64 + 16, "the parsed fields lay out");

    uint8_t *bits = malloc (nb);
    DP_REQUIRE_MSG (bits && wfm_frame_bits (&f, bits, nb) == nb,
                    "and materialise — a parsed generated field is buildable, "
                    "which is the only claim this grammar makes");
    DP_REQUIRE_MSG (wfm_frame_crc_ok (&f, bits) == 1, "its own CRC checks");
    free (bits);
    free (o1);
    free (o2);
  }

  /* ── refusals ──────────────────────────────────────────────────────────*/
  {
    /* The divergence this parser exists to end: one face skipped the stray
       character and shortened the word, the other refused it. */
    DP_REQUIRE_MSG (refuses ("1O11"), "1O11");      /* letter O */
    DP_REQUIRE_MSG (refuses ("10 11"), "10 11");     /* a space */
    DP_REQUIRE_MSG (refuses ("hex:zz"), "hex:zz");    /* not hex */
    DP_REQUIRE_MSG (refuses ("0xZZ"), "0xZZ");
    DP_REQUIRE_MSG (refuses ("file:/nonexistent/definitely/not/here.bin"), "file:/nonexistent/definitely/not/here.bin");

    DP_REQUIRE_MSG (refuses ("barker:13"), "barker:13");            /* an unknown kind */
    DP_REQUIRE_MSG (refuses ("pn:reg=7"), "pn:reg=7");             /* no len */
    DP_REQUIRE_MSG (refuses ("pn:len=127"), "pn:len=127");           /* no register width */
    DP_REQUIRE_MSG (refuses ("pn:len=127,reg=65"), "pn:len=127,reg=65");    /* wider than the struct allows */
    /* The typo cases assert the MESSAGE: see refuses_saying() for why
       asserting the refusal alone proves nothing here. */
    DP_REQUIRE_MSG (refuses_saying ("pn:len=127,reg_bits=7", "unknown key"),
                    "a misspelled key names the KEY, not the register");
    DP_REQUIRE_MSG (refuses ("pn:len=127,reg=7x"), "pn:len=127,reg=7x");    /* a trailing character */
    DP_REQUIRE_MSG (refuses ("pn:len=127,reg=7,lfsr=galoise"), "pn:len=127,reg=7,lfsr=galoise");
    DP_REQUIRE_MSG (refuses ("gold:len=127"), "gold:len=127");         /* no register width */
    DP_REQUIRE_MSG (refuses ("dotted:len=0"), "dotted:len=0");
    DP_REQUIRE_MSG (refuses_saying ("dotted:count=64", "unknown key"),
                    "and so does dotted's");
    DP_REQUIRE_MSG (refuses_saying ("gold:len=8,reg=5,tapsa=1", "unknown key"),
                    "and gold's");
  }

  printf ("test_wfm_seq_parse: OK (bare superset, absent, hex, generated "
          "kinds, round-trip, refusals)\n");
  return 0;
}
