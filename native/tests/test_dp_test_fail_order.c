/*
 * test_dp_test_fail_order.c -- a FAIL lands in order, at the start of its
 * own line, in a `> log 2>&1` capture (#2050).
 *
 * dp_test.h writes failures to stderr, which is unbuffered, and verbose
 * PASS lines to stdout, which is fully buffered once redirected to a file.
 * Without a flush between them a FAIL landed ahead of, or inside, the PASS
 * lines before it, and a harness counting `^FAIL` in such a log undercounted.
 *
 * This runs a known PASS/FAIL sequence with stdout and stderr both pointed
 * at one file, stdout fully buffered (the redirected condition), then reads
 * the file back: every line must open with PASS or FAIL, in the order the
 * checks ran. Proven by sabotage: dropping the flush in DP_TEST_ERR_ puts
 * both FAILs first and turns this red.
 */
#define _POSIX_C_SOURCE 200809L
#define DP_TEST_VERBOSE
#include "dp_test.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#else
#include <unistd.h>
#endif

#define LOG_PATH "test_dp_test_fail_order.log"

int
main (void)
{
  /* Fully buffered, as stdout is when a shell redirects it to a file. */
  setvbuf (stdout, NULL, _IOFBF, BUFSIZ);

  FILE *log = fopen (LOG_PATH, "w+");
  if (!log)
    {
      fprintf (stderr, "cannot open %s\n", LOG_PATH);
      return 1;
    }
  fflush (stdout);
  fflush (stderr);
  const int out = dup (fileno (stdout));
  const int err = dup (fileno (stderr));
  dup2 (fileno (log), fileno (stdout));
  dup2 (fileno (log), fileno (stderr));

  /* The probe: PASS, FAIL, PASS, FAIL, PASS. */
  DP_CHECK (1);
  DP_CHECK (0);
  DP_CHECK (2 > 1);
  DP_CHECK_MSG (0, "second");
  DP_CHECK (3 > 2);

  fflush (stdout);
  fflush (stderr);
  dup2 (out, fileno (stdout));
  dup2 (err, fileno (stderr));
  /* The probe's two failures were the subject, not this test's. */
  dp_test_fails_  = 0;
  dp_test_checks_ = 0;

  static const char *const want[] = { "  PASS  1", "FAIL ", "  PASS  2 > 1",
                                      "FAIL: second", "  PASS  3 > 2" };
  const size_t             n_want = sizeof want / sizeof *want;
  char                     line[512];
  size_t                   k = 0;
  rewind (log);
  while (fgets (line, sizeof line, log))
    {
      DP_CHECK (k < n_want);
      if (k < n_want)
        DP_CHECK (strncmp (line, want[k], strlen (want[k])) == 0);
      k++;
    }
  DP_CHECK (k == n_want);
  fclose (log);
  DP_CHECK (remove (LOG_PATH) == 0);

  DP_TEST_END ("test_dp_test_fail_order");
}
