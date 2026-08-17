- **`dp_test.h` has a self-test — the assertion foundation 97 C test files
    include, and nothing tested it.** That is the worst place in the tree for
    an untested thing, because the failure mode is not a red suite but a
    **green** one: a `DP_CHECK` that stops recording failures turns all 97
    files into programs that run to completion and report success, and `ctest`
    says 100%. The header replaced 90 hand-rolled `CHECK` macros in six
    variants — one with its condition inverted, twenty whose failure gate had
    drifted so that 75 checks printed FAIL and their tests still exited 0 —
    and nothing had been watching the replacement.

    `test_dp_test.c` resolves the circularity by observing `dp_test.h`'s own
    counters rather than its exit status, so it can assert that a check FAILS
    without failing itself, and it captures stderr so a deliberate failure
    never puts a fake `FAIL` line into a passing test's log. Capturing rather
    than discarding also pins the DIAGNOSTIC — file, line, stringified
    condition, and both values plus the tolerance for `DP_CHECK_NEAR`.

    `DP_TEST_END` returns, so its three exit paths cannot be tested
    in-process; `test_dp_test_end.c` gives each one a process and CTest
    asserts the status (`WILL_FAIL` for two). The path that matters is
    **`nothing`**: the zero-assertion floor is the only guard between this
    suite and a test whose body never ran reading as a pass forever, and
    nothing had ever run a zero-assertion program to confirm the floor fires.

    Writing it found a documentation defect on the first run. `dp_cnearf` /
    `dp_cnear` are component-wise and their comment called that "the stricter
    of the two" — it is the **looser**: the component test accepts a square of
    side `2*tol`, the magnitude test a disc of radius `tol` that sits strictly
    inside it, so a diagonal error of `(0.4, 0.4)` passes at `tol = 0.5`
    despite a magnitude of 0.566. That is the shape of every carrier-phase
    error the suite measures, so the semantics are now asserted rather than
    described.
