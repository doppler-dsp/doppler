/* jm_test.h — header-only assertion harness for the C tests.
 *
 * Written once per project (next to the tests, like jm_bench.h next to the
 * benchmarks) and included by every test_<component>_core.c.  Create-only:
 * just-makeit never rewrites it, so a project is free to extend it.
 *
 * Usage:
 *
 *     #define JM_TEST_NAME       "test_gain_core"   // stamped by just-makeit
 *     #define JM_SCAFFOLD_CHECKS 4                  //   "        "     "
 *     #include "jm_test.h"
 *
 *     int main(void)
 *     {
 *         CHECK(x == 1);         // record a failure, keep going
 *         REQUIRE(obj != NULL);  // record and stop; nothing after it is
 *                                // meaningful
 *         JM_TEST_EPILOGUE();    // must be the LAST statement
 *     }
 *
 * gh-934: this used to be stamped into every scaffolded test file instead.
 * One downstream reached 90 copies of CHECK in 6 mutually incompatible
 * variants -- two arities, two failure semantics, one with the condition
 * inverted -- and in 20 of those files the failure gate had drifted ABOVE
 * later assertions, leaving 75 assertions unable to affect the exit code.
 * That hid a real heap buffer overflow.  A copy per file is a copy that
 * diverges; there is one here now.
 */
#ifndef JM_TEST_H
#define JM_TEST_H

#include <complex.h>
#include <math.h>
#include <stdio.h>

/* How many assertions just-makeit generated into the including file.  The
 * scaffold stamps its own count; a hand-written test that never defines it
 * gets 0, which makes the "scaffold coverage only" note below fire only for a
 * file that asserted nothing at all.  Defaulted rather than required so this
 * header is usable in a test an author wrote from scratch. */
#ifndef JM_SCAFFOLD_CHECKS
#  define JM_SCAFFOLD_CHECKS 0
#endif

/* What this test calls itself in its own output.  Defaulted to __FILE__ so a
 * hand-written test needs to define nothing; the scaffold stamps the CTest
 * target name so the log line and the target agree.  Defined once per file
 * rather than passed to the epilogue, so the two places that report a name
 * cannot be given different ones. */
#ifndef JM_TEST_NAME
#  define JM_TEST_NAME __FILE__
#endif

/* File-scope, not locals in main().  The previous per-file copy read a
 * `_fails` that only existed inside main(), so CHECK could not be used from a
 * helper function -- which is a large part of why downstream copies grew
 * incompatible failure semantics instead of being reused. */
static int jm_checks = 0;
static int jm_fails = 0;

/* Record a failure and continue.  The common case: one run reports every
 * broken assertion rather than only the first. */
#define CHECK(cond)                                                          \
    do {                                                                     \
        jm_checks++;                                                         \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
            jm_fails++;                                                      \
        }                                                                    \
    } while (0)

/* Record a failure and stop the test.  For a precondition whose failure makes
 * everything after it meaningless -- a NULL object, a failed allocation --
 * where continuing would segfault rather than report. */
#define REQUIRE(cond)                                                        \
    do {                                                                     \
        jm_checks++;                                                         \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d  %s (fatal)\n", __FILE__, __LINE__,  \
                    #cond);                                                  \
            jm_fails++;                                                      \
            JM_TEST_EPILOGUE();                                              \
        }                                                                    \
    } while (0)

/* Floating-point helpers -- inline functions, not macros, so arguments are
 * evaluated exactly once.  Safe to call with stateful step() results.
 *
 * `static inline`, not plain `static`: a test that does not compare floats
 * never calls these, and a plain `static` in a header makes that a
 * -Wunused-function warning, so a project building with -Werror could not
 * compile a jm scaffold at all (the gh-840 lesson, from jm_bench.h).
 */
static inline int jm_almost_eq(float a, float b, float tol)
    { return fabsf(a - b) <= tol; }
static inline int jm_almost_eq_c(float complex a, float complex b, float tol)
    { return jm_almost_eq(crealf(a), crealf(b), tol)
          && jm_almost_eq(cimagf(a), cimagf(b), tol); }
#define ALMOST_EQ(a, b, tol)   jm_almost_eq((float)(a), (float)(b), tol)
#define ALMOST_EQ_C(a, b, tol) jm_almost_eq_c((float complex)(a),            \
                                              (float complex)(b), tol)

/* Report and return.  MUST be the last statement in main().
 *
 * Gate and report are one macro on purpose, and that is the whole point of
 * the split-out header.  When they were separate statements in a per-file
 * copy, `if (_fails) return 1;` could drift above later assertions -- and it
 * did, in 20 downstream files, where the checks below it still ran, still
 * printed FAIL, and still could not change the exit code.  A gate that cannot
 * be written apart from the report cannot be left behind by one.
 *
 * Placing this anywhere but last makes every statement after it unreachable,
 * which is a loud failure (the count in PASSED drops and the note below
 * fires) rather than the silent one it replaces.
 *
 * The note is phrased against the SCAFFOLD count rather than "every check
 * above was generated by just-makeit", which is what it used to say. With
 * JM_SCAFFOLD_CHECKS defaulting to 0, that sentence reached hand-written
 * tests too and told their authors just-makeit had written their file.
 */
#define JM_TEST_EPILOGUE()                                                   \
    do {                                                                     \
        if (jm_fails) {                                                      \
            fprintf(stderr, "%s FAILED (%d of %d)\n", JM_TEST_NAME,          \
                    jm_fails, jm_checks);                                    \
            return 1;                                                        \
        }                                                                    \
        printf("%s PASSED (%d checks)\n", JM_TEST_NAME, jm_checks);         \
        /* gh-806: the count is the point.  A renamed component orphans its  \
         * real suite and a fresh scaffold takes over the CTest target --    \
         * and because the scaffold passes, "100%% tests passed" is still    \
         * what CI prints.  The one thing that distinguishes the two in a    \
         * log is how much was actually asserted, so say it. */              \
        if (jm_checks <= JM_SCAFFOLD_CHECKS)                                 \
            printf("  NOTE: no assertions beyond the %d just-makeit"          \
                   " generated -- this target\n        does not yet test"     \
                   " anything an author wrote.\n", JM_SCAFFOLD_CHECKS);      \
        return 0;                                                            \
    } while (0)

#endif /* JM_TEST_H */
