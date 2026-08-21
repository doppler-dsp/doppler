- **The "no private RNG" gate now scans `native/validation/`, where four
    harnesses had one.** `check_tests_ssot.py` enforces a rule its own
    docstring stated absolutely — an inline xorshift, a hand-written
    Box-Muller or either uniform mapping may exist only in `dp_rng_test.h` —
    and printed `no private RNG` in its summary. It scanned `native/tests/`
    only. `check_stimulus_sources.py` had scanned both directories all
    along, so the two SSOT gates disagreed about their own scope and the
    narrower one was the one whose rule was stated as absolute.

    `native/validation/` is where this costs the most, for the same reason
    it is exempt from the gate's *other* rules: a validation harness
    **reports** a number instead of asserting a bound, so there is no
    assertion to have margin and nothing to fail. A generator that is
    quietly wrong becomes a published figure that is quietly wrong — which
    is exactly what the consolidation found in `native/tests`, where a
    half-finished Box-Muller delivered mean +0.056 and variance 1.115 while
    claiming N(0, 1) and no test noticed.

    Scope is now **per rule**: the two randomness rules read both
    directories, while re-defining a `dp_*.h` macro, naming your own
    assertion, and the assertion ratchet stay `native/tests`-only. Those are
    about who owns the *assertions*, and a validation harness does not
    assert.

    `symsync_lock.c`, `ber_despreader.c`, `dll_jitter.c` and `rx_dynamics.c`
    were **migrated rather than excused**, so the rule stays absolute rather
    than becoming a ratchet. All four are bit-exact: three had `dp_xs32` /
    `dp_cgauss` / `dp_bit` spelled out by hand, and `ber_despreader.c`'s
    (13, 7, 17) triple on a `uint64_t` *is* `dp_xs64`. Proven by compiling
    each harness before and after with identical flags and diffing the full
    sweep, not just the `--check` subset.

    Widening the scan found the second half of the same defect.
    `rx_dynamics.c` drew both Gaussian components from **one state inside
    one expression** — indeterminately sequenced (C11 6.5.2.2p10), so it
    drew a different noise stream under `make test` (gcc) than under
    `make coverage` (clang), in a harness that publishes a lock/rate table.
    The rule against that could not see it either: its wrapper fold starts
    from the `dp_*` names, and this file's generator chain was private all
    the way down. **A private generator does not just risk being wrong; it
    hides the other rules from the code that uses it.** Rewritten into named
    locals in gcc's order, so the published numbers are unchanged.

- **The gate now recognises a linear-congruential generator, which it had
    never looked for.** The scan knew xorshift and Box-Muller only, so it
    printed `no private RNG` over four live LCG streams — three of them in
    `native/tests`, the directory whose count the docstring called zero.
    Found on the new idiom's first run.

    It uses the same backreference discipline as the xorshift idioms, and it
    is load-bearing for the same reason: the *same* lvalue on both sides is
    what makes it a stream. `x = k * 1103515245u + 12345u`, the one-shot
    index hash in `dp_mf_test.h`, multiplies a different value and carries
    no state — a deterministic bit pattern, not a random source — and stays
    silent, as does `n = n * 2 + 1`. Both were checked as controls.

    Unlike the xorshift copies, migrating these **moves the stream** and
    therefore the numbers measured against it, so the four are held in
    `scripts/.private-rng-ratchet` with a reason each. The list may only
    shrink, and an entry matching nothing fails the gate rather than being
    quietly ignored.

    The summary line now names the directories it scanned and the number it
    is holding, so it cannot read as an unqualified zero again. Every rule
    above was proven by sabotage — putting each shape back and watching
    `make tests-ssot` name the file.
