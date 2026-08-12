# doppler's C test harness

Every `test_*.c` here is a plain C program: it asserts, it counts, it exits
non-zero if anything failed. CTest runs them. There is no framework, and the
shared parts live in the `dp_*_test.h` family rather than in each file.

## The family

| header            | owns                                                          |
| ----------------- | ------------------------------------------------------------- |
| `dp_test.h`       | **assertions, the counters, the epilogue** — everything below |
| `dp_rng_test.h`   | **randomness**: the generator, the uniforms, the Gaussians    |
| `dp_state_test.h` | the serialize → restore → reject-a-clobbered-blob round trip  |
| `dp_tx_test.h`    | stimulus: one shaped symbol stream, one place                 |
| `dp_dsss_test.h`  | stimulus: the code-spread BPSK capture, fixed or ramped       |
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

## Randomness

Every test's data bits and every test's noise come from `dp_rng_test.h`.

```c
uint32_t st = 12345u;              /* the seed is yours, and explicit */
int   b = dp_bit (&st);            /* +-1                             */
double u = dp_uni (&st);           /* (0, 1]                          */
double g = dp_gauss (&st);         /* N(0, 1)                         */
float complex z = dp_cgauss (&st); /* E|z|^2 = 1, 0.5 per component   */
```

`dp_xs32` is the generator itself; `dp_xs64` / `dp_uni64` / `dp_gauss64` are
the 64-bit width, which exists so `test_dp_ber.c` can measure into the tail of
the error-rate curve with a full 53-bit mantissa.

**`dp_cgauss` is not `dp_gauss() + I * dp_gauss()`.** It draws two words and
uses both Box-Muller branches, landing at `E|z|^2 = 1` — so a caller scaling
by `sigma` gets noise power `sigma^2`. The sum form draws four, discards half
of them, and lands at unit variance *per component*, which is twice the power.
Both are defensible readings of "complex Gaussian", which is precisely why the
one this suite uses is a function rather than a convention.

Twenty copies of the xorshift step and five of `gauss` accumulated before this
header existed, and one of the five was a half-finished edit that delivered
mean +0.056 and variance 1.115 while claiming N(0, 1). Nothing failed —
`test_costas_core.c` ran its only AWGN test 0.47 dB hot on biased,
heavy-tailed noise for as long as the copy existed. A private generator cannot
be wrong in a way anything notices, so `make lint` rejects a new one: an
inline xorshift, a hand-written Box-Muller, or either uniform mapping fails
`tests-ssot` outside `dp_rng_test.h`.

`test_dp_rng.c` pins it. The integer streams are compared bit-for-bit against
recorded vectors; the Gaussians are checked to a tolerance and then measured
(mean, variance, two-sigma tail, and `E|z|^2`). The split is not fussiness —
`log`, `cos` and `sin` are libm, libm is not correctly rounded by any
standard, and the arm64 and macOS runners return their own last ulp. Pinning a
Gaussian's bits would be a flaky test wearing a strict one's clothes.

## Where a new helper goes

- **Used by one test** → keep it `static` in that test. Not everything shared
    by two files wants to be shared by all of them.
- **A measurement or verdict several suites need** → the matching family
    header above, or a new `dp_<topic>_test.h` if it is genuinely a new topic.
- **An assertion or a counter** → `dp_test.h`, and only after asking whether
    an existing macro composes into it.
- **A distribution or a generator** → `dp_rng_test.h`, always, even for one
    caller. Nothing else in this directory may hold one, and the gate enforces
    that rather than trusting this bullet.

What does *not* go here: a per-object test that happens to share a name with
another one. `test_state_roundtrip` is defined in six files with six different
bodies — each drives its own object's split-stream resume. Same property,
different test. Consolidating those would delete coverage, not duplication.

`make_signal` is the same trap one level up. It is defined in eight files, and
only two of those were one function written twice (the DSSS pair, now
`dp_dsss_test.h`). The other six build genuinely different signals that happen
to share a name: a tone times NRZ data, an M-PSK stream with a frequency ramp,
a carrier-free spread signal at code rate, an RC-shaped stream at a timing
offset. Count implementations, not names.

## Gates

`make lint` runs `tests-ssot`, which enforces three things.

**One definition.** A test may not re-define anything the family already
provides, nor roll its own `CHECK`/`REQUIRE`/`EXPECT`/`ASSERT`. The forbidden
set is derived from every `dp_*.h` here on every run, so a name added to any
member — or a whole new member — is covered without touching the checker. This
exists because 90 copies of `CHECK` in six incompatible variants accumulated
under a convention that was already written down — a note in a README is not a
control.

**No private randomness.** An inline xorshift, a hand-written Box-Muller, or
either of the two uniform mappings fails the gate anywhere but
`dp_rng_test.h`. `check_stimulus_sources.py` deliberately declines this check
at repo scale, where hand-rolled noise hits 72 files and a ratchet that large
is noise; here the count is zero, so it is a rule instead of a ratchet.

**No silent loss of coverage.** No `native/tests/*.c` may end up with fewer
assertions than `$(ASSERT_BASE)` (default `origin/main`) has. A migration or a
badly resolved rebase can drop assertions while everything stays green: the
file compiles, the survivors pass, `ctest` reports 100%. Read the count, not
the percentage. This consolidation itself dropped **43 assertions across three
files** — cut before `feat(telemetry)` reached `main` — and only one of the
three was visible to review; the other two were found by counting.

Deliberate removals go in `native/tests/.assertion-ratchet-ignore` with a
reason. Deleting a whole file needs no entry: a deletion is visible in the
diff, which is exactly what the silent case is not.

## The one exception

`examples/downstream-jm/native/tests/` keeps its own `CHECK`. That is a
separate downstream project demonstrating what consuming doppler looks like
from outside, so it cannot reach into this directory — and should not.
