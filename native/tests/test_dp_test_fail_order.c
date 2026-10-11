/*
 * test_dp_test_fail_order.c -- every diagnostic lands in order in a
 * `> log 2>&1` capture (#2050).
 *
 * dp_test.h writes failures to stderr, which is unbuffered, and verbose
 * PASS lines to stdout, which is fully buffered once redirected to a file.
 * Without a flush between them a FAIL landed ahead of, or inside, the PASS
 * lines before it, and a harness counting `^FAIL` in such a log undercounted.
 *
 * This runs a known sequence with stdout and stderr both pointed at one file,
 * stdout fully buffered (the redirected condition), then reads the file back:
 * every line must come out in the order the checks ran. The sequence reaches
 * every diagnostic site in dp_test.h -- DP_CHECK, DP_CHECK_MSG, DP_CHECK_NEAR,
 * DP_TEST_END's FAILED and ASSERTED NOTHING lines, DP_TEST_EMIT_END -- because
 * a site the probe never reaches can be reverted to a raw fprintf and stay
 * green. Proven by sabotage: reverting any one site to a raw stderr write
 * puts its line out of order and turns this red.
 *
 * "In order" is the guarantee. It is not "at column 0": a DP_REQUIRE inside an
 * open table row writes its FAIL after the row's flushed cells.
 */
#define _POSIX_C_SOURCE 200809L
#define DP_TEST_VERBOSE
#include "dp_test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define close _close
#define fileno _fileno
#else
#include <unistd.h>
#endif

#define LOG_PATH "test_dp_test_fail_order.log"

/* The end macros, each in a function of its own, so the probe can call them
 * with failures on the books. Each returns 1, as the macro does. */
static int
end_probe (void)
{
  DP_TEST_END ("probe");
}

static int
empty_probe (void)
{
  DP_TEST_END ("empty");
}

static int
emit_end_probe (void)
{
  DP_TEST_EMIT_END ("emit");
}

int
main (void)
{
  /* Fully buffered, as stdout is when a shell redirects it to a file. */
  setvbuf (stdout, NULL, _IOFBF, BUFSIZ);

  FILE *log = fopen (LOG_PATH, "w+");
  if (!log)
    {
      DP_TEST_ERR ("cannot open %s\n", LOG_PATH);
      return 1;
    }
  fflush (stdout);
  fflush (stderr);
  const int out = dup (fileno (stdout));
  const int err = dup (fileno (stderr));
  dup2 (fileno (log), fileno (stdout));
  dup2 (fileno (log), fileno (stderr));

  /* The probe: PASS, FAIL, PASS, FAIL, PASS, a near-check FAIL, PASS. */
  DP_CHECK (1);
  DP_CHECK (0);
  DP_CHECK (2 > 1);
  DP_CHECK_MSG (0, "second");
  DP_CHECK (3 > 2);
  DP_CHECK_NEAR (1.0, 2.0, 0.5);
  DP_CHECK (4 > 3);
  /* The end macros report the three failures on the books. Each raw site
     must follow a PASS still buffered on stdout, or it has nothing to land
     ahead of: the PASS lines below are what a reverted site reorders. */
  (void)end_probe ();
  DP_CHECK (6 > 5);
  (void)emit_end_probe ();
  /* DP_TEST_END's other branch: a test that checked nothing. */
  DP_CHECK (7 > 6);
  dp_test_checks_ = 0;
  dp_test_fails_  = 0;
  (void)empty_probe ();

  fflush (stdout);
  fflush (stderr);
  dup2 (out, fileno (stdout));
  dup2 (err, fileno (stderr));
  close (out);
  close (err);
  /* The probe's failures were the subject, not this test's. */
  dp_test_fails_  = 0;
  dp_test_checks_ = 0;

  static const char *const want[] = {
    "  PASS  1",     "FAIL ",
    "  PASS  2 > 1", "FAIL: second",
    "  PASS  3 > 2", "FAIL ",
    "  PASS  4 > 3", "probe FAILED (3)",
    "  PASS  6 > 5", "emit FAILED (3)",
    "  PASS  7 > 6", "empty ASSERTED NOTHING",
  };
  const size_t n_want = sizeof want / sizeof *want;
  char         line[512];
  size_t       k = 0;
  rewind (log);
  while (fgets (line, sizeof line, log))
    {
      DP_CHECK (k < n_want);
      if (k < n_want && strncmp (line, want[k], strlen (want[k])) != 0)
        {
          /* Say what was read, so a mismatch names its line. */
          DP_TEST_ERR ("  line %zu: want \"%s\", got \"%s\"", k, want[k],
                       line);
          DP_RECORD_FAIL ();
        }
      k++;
    }
  DP_CHECK (k == n_want);
  fclose (log);
  DP_CHECK (remove (LOG_PATH) == 0);

  DP_TEST_END ("test_dp_test_fail_order");
}
