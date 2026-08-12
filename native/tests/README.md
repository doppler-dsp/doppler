# doppler's C test harness

Every `test_*.c` here is a plain C program: it asserts, it counts, it exits
non-zero if anything failed. CTest runs them. There is no framework, and the
shared parts live in the `dp_*_test.h` family rather than in each file.

## The family

| header            | owns                                                          |
| ----------------- | ------------------------------------------------------------- |
| `dp_test.h`       | **assertions, the counters, the epilogue** — everything below |
| `dp_state_test.h` | the serialize → restore → reject-a-clobbered-blob round trip  |
| `dp_tx_test.h`    | stimulus: one shaped symbol stream, one place                 |
| `dp_sym_test.h`   | truth-free symbol-quality verdicts for receiver tests         |
| `dp_ber_test.h`   | error-rate measurement: settling, alignment, sampling, the CI |
| `dp_mf_test.h`    | matched-filter fixtures (RRC-BPSK on a carrier, EVM)          |

`dp_test.h` is the one every other member depends on. Include it first;
include the others as the test needs them.

## Assertions

Two failure semantics, because both are legitimate and the codebase used
both. The names are the ones Catch2 and doctest use for the same split:

| macro                       | on failure                                    |
| --------------------------- | --------------------------------------------- |
| `DP_CHECK(cond)`            | report, count it, **carry on**                |
| `DP_REQUIRE(cond)`          | report, count it, **`return 1` immediately**  |
| `DP_CHECK_MSG(cond, msg)`   | as `DP_CHECK`, with your wording              |
| `DP_REQUIRE_MSG(cond, msg)` | as `DP_REQUIRE`, with your wording            |
| `DP_CHECK_NEAR(a, b, tol)`  | as `DP_CHECK`, printing both values and tol   |
| `DP_RECORD_FAIL()`          | count a failure you already reported yourself |

Reach for `DP_CHECK` by default — you learn about all ten broken cases in one
run instead of the first. Reach for `DP_REQUIRE` when the checks below would
be unsafe: a `create()` that returned NULL, a buffer that failed to allocate.

Prefer the condition forms over the `_MSG` ones. `#cond` is stringified by the
preprocessor and cannot go stale; a hand-written sentence can, and did.

## Ending a test

```c
int
main (void)
{
  thing_t *t = thing_create ();
  DP_REQUIRE (t != NULL);
  DP_CHECK (thing_size (t) == 3);
  DP_CHECK (dp_nearf (thing_gain (t), 0.5f, 1e-6f));
  thing_destroy (t);
  DP_TEST_END ("test_thing_core");
}
```

`DP_TEST_END` **must be the last statement of `main`**, and that is not a
style rule. The shape it replaced was a hand-written
`if (_fails) { ...; return 1; } printf ("... PASSED"); return 0;`, and in 20
files that block had drifted to sit *before* later assertions — so 75 checks
printed `FAIL` and the test still exited 0. Because `DP_TEST_END` reports and
returns, nothing can be appended after it.

It also fails a test that asserted **nothing**. A body that is `#if 0`-ed out,
or a loop that never ran, otherwise exits 0 and reads as passing forever.

## Comparisons and tolerances

```c
dp_nearf  (a, b, tol)   /* float           */
dp_near   (a, b, tol)   /* double          */
dp_cnearf (a, b, tol)   /* float complex   */
dp_cnear  (a, b, tol)   /* double complex  */
```

Four names rather than one because doppler is C99 — no `_Generic`, the same
reason the C library spells `fabsf` and `fabs` separately.

**Tolerance is always an argument.** It is a property of the measurement, not
of the comparison: a CF32 round-trip and a double-precision spectral estimate
do not share an epsilon, and the suite's per-file `TOL` constants deliberately
range from `1e-3f` to `1e-12`. Keep yours at the call site or in your file;
never push one into `dp_test.h`.

## Where a new helper goes

- **Used by one test** → keep it `static` in that test. Not everything shared
    by two files wants to be shared by all of them.
- **A measurement or verdict several suites need** → the matching family
    header above, or a new `dp_<topic>_test.h` if it is genuinely a new topic.
- **An assertion or a counter** → `dp_test.h`, and only after asking whether
    an existing macro composes into it.

What does *not* go here: a per-object test that happens to share a name with
another one. `test_state_roundtrip` is defined in six files with six different
bodies — each drives its own object's split-stream resume. Same property,
different test. Consolidating those would delete coverage, not duplication.

## Gate

`make lint` runs `tests-ssot`, which fails if a test re-defines an assertion
`dp_test.h` already provides. It exists because this file previously held 90
copies of `CHECK` in six incompatible variants, and a note in a README is not
a control.

## The one exception

`examples/downstream-jm/native/tests/` keeps its own `CHECK`. That is a
separate downstream project demonstrating what consuming doppler looks like
from outside, so it cannot reach into this directory — and should not.
