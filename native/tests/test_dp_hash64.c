/**
 * @file test_dp_hash64.c
 * @brief Pins dp_hash64.h to FNV-1a 64 and to its incremental contract.
 *
 * The expected values are FNV-1a 64's published test vectors (the FNV
 * reference test suite, as reproduced in draft-eastlake-fnv): the empty
 * input returns the offset basis, and "a" and "foobar" are two of the
 * listed strings. They are the reference this primitive answers to, so a
 * failing vector means the hash is wrong, never the vector.
 *
 * The second half pins what the data source relies on
 * (docs/design/payload-data-source.md §4.8): a file hashed chunk by chunk,
 * split anywhere, gives the hash of the whole, so the file never has to be
 * read twice.
 */
#include "doppler/dp_hash64.h"
#include "dp_test.h"

#include <stdio.h>
#include <string.h>

static void
expect (const char *s, uint64_t want)
{
  const uint64_t got = dp_hash64 (DP_HASH64_INIT, s, strlen (s));
  if (got != want)
    {
      fprintf (stderr, "FAIL dp_hash64(\"%s\") = 0x%016llx, want 0x%016llx\n",
               s, (unsigned long long)got, (unsigned long long)want);
      DP_RECORD_FAIL ();
      return;
    }
  DP_CHECK (got == want);
}

int
main (void)
{
  /* The published vectors. */
  expect ("", UINT64_C (0xcbf29ce484222325)); /* the offset basis */
  expect ("a", UINT64_C (0xaf63dc4c8601ec8c));
  expect ("foobar", UINT64_C (0x85944171f73967e8));

  /* Incremental: every split of the input gives the hash of the whole. */
  {
    static const char s[]  = "foobar";
    const size_t      n    = sizeof s - 1u;
    const uint64_t    want = dp_hash64 (DP_HASH64_INIT, s, n);
    for (size_t k = 0; k <= n; k++)
      {
        const uint64_t h
            = dp_hash64 (dp_hash64 (DP_HASH64_INIT, s, k), s + k, n - k);
        DP_CHECK_MSG (h == want, "a split input hashes as the whole");
      }
  }

  /* The same over a longer input in uneven chunks, the shape a reader
     produces: 1 MiB in reads of 4093 bytes against one call. */
  {
    enum
    {
      N = 1u << 20
    };
    static unsigned char buf[N];
    for (size_t i = 0; i < N; i++)
      buf[i] = (unsigned char)(i * 131u + (i >> 9));
    const uint64_t whole = dp_hash64 (DP_HASH64_INIT, buf, N);
    uint64_t       h     = DP_HASH64_INIT;
    for (size_t at = 0; at < N; at += 4093u)
      h = dp_hash64 (h, buf + at, N - at < 4093u ? N - at : 4093u);
    DP_CHECK_MSG (h == whole, "chunked reads hash as one read");
  }

  /* An empty read changes nothing, and needs no buffer: the first read of
     an empty file, or a read that returned 0 bytes. */
  DP_CHECK_MSG (dp_hash64 (UINT64_C (0x1234), NULL, 0) == UINT64_C (0x1234),
                "an empty read leaves the hash as it was");

  /* The order of bytes matters, so a reordered file is a different file. */
  DP_CHECK_MSG (dp_hash64 (DP_HASH64_INIT, "ab", 2)
                    != dp_hash64 (DP_HASH64_INIT, "ba", 2),
                "a reordering changes the hash");

  DP_TEST_END ("dp_hash64");
}
