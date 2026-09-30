/**
 * @file wfm_field_explore.c
 * @brief The Field parser's malformed and round-trip corpus (phase 7).
 *
 * A parser has no statistical envelope, so its exploration is a CORPUS, not
 * a sweep: text generated to be valid, text mutated to be probably invalid,
 * and hand-picked edges, each driven through the one reader and the one
 * writer of the grammar (`dp_wfm_field_parse`, `dp_wfm_field_format`) and
 * the one door to bits (`dp_wfm_field_bits`). The record is
 * docs/design/frame-description-measurements.md §F.6; this is the harness
 * that produced it, committed so the run can be repeated rather than
 * recalled.
 *
 * ## The properties, per accepted text
 *
 * 1. parse -> format -> parse gives back the SAME field;
 * 2. the text is canonical: formatting the re-parsed field gives the same
 *    text, and a NULL buffer sizes it exactly;
 * 3. the original and the canonical text render the SAME bits, every one
 *    0 or 1 (for a field of at most 2^16 bits, to keep the run short) --
 *    except `data:LEN`, whose bits are its data source's, so BOTH texts
 *    must be refused by the door to bits instead.
 *
 * A generated-valid text that is REFUSED is a finding too. A mutated text
 * may be accepted or refused; it must never crash, and if accepted it must
 * hold all three properties. Under `make test-asan`/`test-ubsan` the
 * sanitizers are the fourth property: no report.
 *
 * ## The corpus is reproducible
 *
 * One xorshift64 generator, seeded with SEED below, picks one of five shapes
 * -- a 1-70 bit binary literal, a `0x`/`0X` literal of 1-20 digits in both
 * cases, `pn` (LEN 1-3000, REG 2-31, five argument shapes including one
 * malformed empty-seed form), `gold` over the CCSDS preferred pair (LEN
 * 1-2000), and `dotted` (LEN 1-500) -- and appends `*1`-`*9` to a third of
 * them. A mutation deletes, inserts, replaces or truncates at one position,
 * drawing from ALPHABET.
 *
 * Usage:
 *   validate_wfm_field_explore           full corpus: 200000 + 200000 + edges
 *   validate_wfm_field_explore --check   the same, 2000 + 2000 + edges
 *
 * Every finding is a failed DP_CHECK_MSG, and the run also CHECKS that it
 * checked something: text was accepted, every accepted text round-tripped,
 * every mutation ran. A generator that produced nothing the parser accepts
 * would otherwise find nothing and pass. DP_TEST_END gives the status.
 */
#include "doppler/clib_common.h"
#include "doppler/wfm/wfm_frame.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The recorded seed (§F.6). Changing it changes the corpus, and the record
   quotes counts from this one. */
#define SEED 0x9E3779B97F4A7C15ull

#define FULL_N 200000
#define CHECK_N 2000

/* The largest field whose bits are rendered and compared. */
#define BITS_CAP (1u << 16)

static const char ALPHABET[] = "01:x*X_- +pngoldtd9aF\t";

static uint64_t rng = SEED;

/* The shared xorshift64 (13/7/17): the recorded corpus is its stream. */
static uint64_t
rnd (void)
{
  return dp_xs64 (&rng);
}

typedef struct
{
  unsigned long round_trips; /* accepted texts that held every property */
  unsigned long accepted;
  unsigned long mutations;
  unsigned long findings;
} tally_t;

static int
same_field (const wfm_field_t *a, const wfm_field_t *b)
{
  const wfm_seq_t *x = &a->seq, *y = &b->seq;
  if (x->kind != y->kind || x->len != y->len || a->reps != b->reps)
    return 0;
  if (x->kind == WFM_SEQ_LITERAL)
    return memcmp (x->bits, y->bits, x->len) == 0;
  return x->poly == y->poly && x->seed == y->seed && x->reg_bits == y->reg_bits
         && x->lfsr == y->lfsr && x->taps_a == y->taps_a
         && x->seed_a == y->seed_a && x->taps_b == y->taps_b
         && x->seed_b == y->seed_b;
}

/* A finding is a failed check: printed with the text that caused it, and
   counted by dp_test.h, so the run's status is the harness's, not ours. */
static void
finding (tally_t *t, const char *what, const char *spec, const char *more)
{
  printf ("FINDING %s: '%s'%s%s\n", what, spec, more ? " -- " : "",
          more ? more : "");
  t->findings++;
  DP_CHECK_MSG (0, what);
}

/* Hold the three properties for one text. `must_parse`: a refusal is a
   finding (generated-valid text) rather than an allowed outcome. */
static void
round_trip (tally_t *t, const char *spec, int must_parse)
{
  wfm_field_t f, g;
  uint8_t    *o1 = NULL, *o2 = NULL;
  const char *why = NULL;

  if (dp_wfm_field_parse (spec, &f, &o1, &why) != DP_OK)
    {
      if (must_parse)
        finding (t, "valid text refused", spec, why);
      return;
    }
  t->accepted++;

  char         t1[4096], t2[4096];
  const size_t l1 = dp_wfm_field_format (&f, t1, sizeof t1);
  if (l1 == 0 || l1 >= sizeof t1)
    {
      finding (t, "no canonical text", spec, NULL);
      free (o1);
      return;
    }
  if (dp_wfm_field_format (&f, NULL, 0) != l1)
    finding (t, "a NULL buffer sizes it differently", spec, t1);
  if (dp_wfm_field_parse (t1, &g, &o2, &why) != DP_OK)
    {
      finding (t, "its canonical text is refused", spec, why);
      free (o1);
      return;
    }
  if (!same_field (&f, &g))
    finding (t, "parse(format(f)) != f", spec, t1);
  const size_t l2 = dp_wfm_field_format (&g, t2, sizeof t2);
  if (l2 != l1 || strcmp (t1, t2) != 0)
    finding (t, "the text is not canonical", spec, t1);

  const size_t n = f.seq.len * f.reps;
  if (f.seq.kind == WFM_SEQ_DATA)
    {
      if (dp_wfm_field_bits (spec, NULL, 0, NULL) != 0
          || dp_wfm_field_bits (t1, NULL, 0, NULL) != 0)
        finding (t, "a data field rendered bits of its own", spec, t1);
    }
  else if (n <= BITS_CAP)
    {
      uint8_t     *b1 = dp_xmalloc (n), *b2 = dp_xmalloc (n);
      const size_t r1 = dp_wfm_field_bits (spec, b1, n, NULL);
      const size_t r2 = dp_wfm_field_bits (t1, b2, n, NULL);
      if (r1 != n || r2 != n || memcmp (b1, b2, n) != 0)
        finding (t, "the two texts render different bits", spec, t1);
      for (size_t i = 0; i < r1; i++)
        if (b1[i] > 1u)
          {
            finding (t, "a rendered bit is not 0 or 1", spec, NULL);
            break;
          }
      free (b1);
      free (b2);
    }
  t->round_trips++;
  free (o1);
  free (o2);
}

/* One generated text; the pn shape 4 is the one deliberately malformed. */
static void
gen (char *s, size_t cap)
{
  static const char hx[] = "0123456789abcdefABCDEF";
  size_t            n    = 0;
  switch (rnd () % 5u)
    {
    case 0:
      {
        const size_t len = 1u + rnd () % 70u;
        for (size_t i = 0; i < len; i++)
          s[n++] = (char)('0' + (rnd () & 1u));
        s[n] = '\0';
        break;
      }
    case 1:
      {
        const size_t len = 1u + rnd () % 20u;
        n = (size_t)snprintf (s, cap, "0%c", (rnd () & 1u) ? 'x' : 'X');
        for (size_t i = 0; i < len; i++)
          s[n++] = hx[rnd () % 22u];
        s[n] = '\0';
        break;
      }
    case 2:
      {
        const unsigned           reg = 2u + (unsigned)(rnd () % 30u);
        const unsigned long long len = 1u + rnd () % 3000u;
        /* A seed is a register's worth of bits (doppler#1624): each drawn
           value is reduced into the register, so the DRAWS are the recorded
           corpus's and only an out-of-range value's text changes. */
        const unsigned long long in = (1ull << reg) - 1u;
        switch (rnd () % 5u)
          {
          case 0:
            snprintf (s, cap, "pn:%llu:%u", len, reg);
            break;
          case 1:
            snprintf (s, cap, "pn:%llu:%u:%llu", len, reg,
                      (unsigned long long)(rnd () % 1000u) & in);
            break;
          case 2:
            snprintf (s, cap, "pn:%llu:%u:0x%llx:0", len, reg,
                      (unsigned long long)(1u + rnd () % 100u) & in);
            break;
          case 3:
            snprintf (s, cap, "pn:%llu:%u:%llu:0:fibonacci", len, reg,
                      (unsigned long long)(rnd () % 50u) & in);
            break;
          default:
            snprintf (s, cap, "pn:0x%llx:%u::galois", len, reg);
            break;
          }
        break;
      }
    case 3:
      snprintf (s, cap, "gold:%llu:10:934:%llu:567:%llu",
                (unsigned long long)(1u + rnd () % 2000u),
                (unsigned long long)(1u + rnd () % 1023u),
                (unsigned long long)(1u + rnd () % 1023u));
      break;
    default:
      snprintf (s, cap, "dotted:%llu",
                (unsigned long long)(1u + rnd () % 500u));
      break;
    }
  if (rnd () % 3u == 0)
    {
      const size_t m = strlen (s);
      snprintf (s + m, cap - m, "*%llu",
                (unsigned long long)(1u + rnd () % 9u));
    }
}

/* One single-character edit of @p s into @p m. */
static void
mutate (const char *s, char *m)
{
  const size_t len = strlen (s);
  const size_t p   = rnd () % (len + 1u);
  /* The draw order is the corpus: position, then edit, then the character
     only for an edit that needs one -- as the recorded run drew them. */
  switch (rnd () % 4u)
    {
    case 0: /* delete */
      if (len)
        {
          memcpy (m, s, p);
          strcpy (m + p, s + (p < len ? p + 1u : len));
          return;
        }
      break;
    case 1: /* insert */
      memcpy (m, s, p);
      m[p] = ALPHABET[rnd () % (sizeof ALPHABET - 1u)];
      strcpy (m + p + 1u, s + p);
      return;
    case 2: /* replace */
      if (len)
        {
          strcpy (m, s);
          m[p < len ? p : len - 1u]
              = ALPHABET[rnd () % (sizeof ALPHABET - 1u)];
          return;
        }
      break;
    default:
      break;
    }
  memcpy (m, s, p); /* truncate */
  m[p] = '\0';
}

/* Edges a generator would not reach: the limits of every number, the
   operators doubled or emptied, case, sign and radix. */
static const char *const EDGES[] = {
  "pn:18446744073709551615:5",
  "pn:18446744073709551616:5",
  "pn:1:64",
  "pn:1:65",
  "pn:5:0",
  "*2",
  "0101*0",
  "0101*",
  "0101**2",
  "0101*18446744073709551615",
  "0x1*18446744073709551615",
  "dotted:0",
  "gold:5:10:0:0:0:0",
  "PN:5:3",
  "pn:5:3:0x",
  "pn:5:3:1:2:3",
  "pn:5:3:1:0:galois",
  "pn:5:3:1:0:fib",
  "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
  "pn:05:3",
  "pn:0x05:3",
  "pn:+5:3",
  "pn:5:3:-1",
  "dotted:5*1",
  "1*1",
  "pn:12:1",
  "pn:31:5:32",
  "pn:261120:18",
  "pn:261121:18",
  "0x1*65281",
  "data:1",
  "data:0x10*3",
  "data:261120",
  "data:261121",
  "data:0",
  "data:8:1",
  "DATA:8",
};

int
main (int argc, char **argv)
{
  const int check = argc > 1 && strcmp (argv[1], "--check") == 0;
  const int n     = check ? CHECK_N : FULL_N;
  tally_t   t     = { 0 };
  char      s[512], m[600];

  for (int i = 0; i < n; i++)
    {
      gen (s, sizeof s);
      round_trip (&t, s, strstr (s, "::") == NULL);
    }
  for (int i = 0; i < n; i++)
    {
      gen (s, sizeof s);
      mutate (s, m);
      t.mutations++;
      round_trip (&t, m, 0);
    }
  for (size_t i = 0; i < sizeof EDGES / sizeof *EDGES; i++)
    round_trip (&t, EDGES[i], 0);

  printf ("seed 0x%llx, %d generated, %lu mutations, %zu edges\n",
          (unsigned long long)SEED, n, t.mutations,
          sizeof EDGES / sizeof *EDGES);
  printf ("accepted %lu, round trips %lu, findings %lu\n", t.accepted,
          t.round_trips, t.findings);

  /* Not vacuous: the corpus reached the parser, and past it. */
  DP_CHECK_MSG (t.accepted > (unsigned long)n / 2u,
                "most generated text is accepted -- the corpus reached the "
                "grammar");
  DP_CHECK_MSG (t.round_trips == t.accepted,
                "every accepted text held all three properties");
  DP_CHECK_MSG (t.mutations == (unsigned long)n, "every mutation ran");
  DP_TEST_END ("wfm_field_explore");
}
