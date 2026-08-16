/**
 * @file test_dp_test_end.c
 * @brief `DP_TEST_END`'s three exit paths, at process level.
 *
 * `DP_TEST_END` cannot be tested from inside `test_dp_test.c`, because it
 * RETURNS: whichever path it takes ends the caller. So each path gets a
 * process, selected by argv, and CTest asserts the exit status —
 * `WILL_FAIL TRUE` for the two that must fail.
 *
 * The path that matters most is `nothing`. `DP_TEST_END`'s own docstring says
 * a test "whose body is `#if 0`-ed out, or whose only loop never ran,
 * otherwise exits 0 and reads as a passing test forever", and that floor is
 * the single guard standing between this suite and the "empty result set is
 * not a pass" trap the glibc and tarball gates were both caught by. Nothing
 * had ever run a zero-assertion program to check the floor fires.
 *
 * Usage (CTest drives all three):
 *   test_dp_test_end nothing   -> must exit non-zero, having asserted nothing
 *   test_dp_test_end fail      -> must exit non-zero, one failed check
 *   test_dp_test_end pass      -> must exit zero, one passing check
 */
#include "dp_test.h"

#include <stdio.h>
#include <string.h>

int
main (int argc, char **argv)
{
  const char *mode = (argc > 1) ? argv[1] : "";

  if (strcmp (mode, "nothing") == 0)
    {
      /* Assert NOTHING, deliberately. The floor must turn this into a
         failure; without it this program is indistinguishable from a test
         that ran fifty checks and passed. */
      DP_TEST_END ("test_dp_test_end[nothing]");
    }
  if (strcmp (mode, "fail") == 0)
    {
      /* One real failure. Its FAIL line on stderr is expected output for this
         process, not a defect — CTest inverts the verdict. */
      DP_CHECK (0);
      DP_TEST_END ("test_dp_test_end[fail]");
    }
  if (strcmp (mode, "pass") == 0)
    {
      DP_CHECK (1);
      DP_TEST_END ("test_dp_test_end[pass]");
    }

  /* An unknown mode must not look like any of the three. A silent exit 0 here
     would make a typo in the CMake registration read as a passing gate. */
  fprintf (stderr, "test_dp_test_end: unknown mode '%s'\n", mode);
  return 2;
}
