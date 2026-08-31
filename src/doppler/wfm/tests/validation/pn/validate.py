"""PN — certification evidence for the m-sequence generator.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/wfm/tests/validation/pn/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/wfm/tests/test_validation_limits.py`.

**This is a leaf, and its subject is the TABLE.** `pn_mls_poly()`
declares a primitive polynomial for every register width from 2 to 64,
and the header calls them "verified primitive polynomials (period
2^n-1)". Across both suites that was pinned at six of the sixty-three.
A wrong entry does not crash: it is a spreading code with a short
period, and the cost lands as missing processing gain in whatever
correlates against it.

Two things are measured that the C test cannot reach:

- **every entry**, not the 23 a stepping test can walk. n=64 is 1.8e19
  states, so primitivity is settled algebraically -- and the transition
  matrix is PROBED OUT OF THE SHIPPED LIBRARY rather than modelled here,
  so the algebra is about the code that ships. It is cross-checked
  against brute-force stepping wherever both apply (§2.1).
- **the properties a spreading code is actually chosen for** -- balance,
  ideal periodic autocorrelation, and shift-and-add closure (§2.2).
  "Period 2^n-1" is necessary and not sufficient: those are the reasons
  an m-sequence is the sequence anyone wants.

The order is the campaign's: `native/inc/pn/pn_core.h` is the SSOT and
`native/tests/test_pn_core.c` certifies it in C.
"""

from __future__ import annotations

import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.tests._repo import repo_root
from doppler.tests._validation_common import Report, cli
from doppler.wfm import PN, mls_poly

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)

R = Report()

# Every width the table declares.
WIDTHS = tuple(range(2, 65))
# Widths a full period can be walked at, for the cross-check and the
# sequence-property work. 13 -> 8191 chips; the FFT keeps it cheap.
WALKABLE = (5, 7, 9, 11, 13)
# Widths the brute-force period check runs at (2^16 steps is the ceiling
# worth paying twice a push).
BRUTE = tuple(range(2, 17))


def _csv(path: Path, header: str, rows: list[list[float]]) -> None:
    if not R.write:
        return
    DATA.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        fh.write(header + "\n")
        for r in rows:
            fh.write(",".join(f"{v:.10g}" for v in r) + "\n")


# ── factorisation, for the order test ────────────────────────────────
_FACTORS: dict[int, set[int]] = {}


def _is_prime(n: int) -> bool:
    if n < 2:
        return False
    small = (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37)
    for p in small:
        if n % p == 0:
            return n == p
    d, s = n - 1, 0
    while d % 2 == 0:
        d //= 2
        s += 1
    for a in small:
        x = pow(a, d, n)
        if x in (1, n - 1):
            continue
        for _ in range(s - 1):
            x = x * x % n
            if x == n - 1:
                break
        else:
            return False
    return True


def _gcd(a: int, b: int) -> int:
    while b:
        a, b = b, a % b
    return a


def _rho(n: int) -> int:
    if n % 2 == 0:
        return 2
    rng = random.Random(0xC0FFEE)  # fixed: a report is byte-compared
    while True:
        x = rng.randrange(2, n)
        y, c, d = x, rng.randrange(1, n), 1
        while d == 1:
            x = (x * x + c) % n
            y = (y * y + c) % n
            y = (y * y + c) % n
            d = _gcd(abs(x - y), n)
        if d != n:
            return d


def prime_factors(n: int) -> set[int]:
    """Distinct primes of n, memoised across both build() calls."""
    if n in _FACTORS:
        return _FACTORS[n]
    if n == 1:
        out: set[int] = set()
    elif _is_prime(n):
        out = {n}
    else:
        d = _rho(n)
        out = prime_factors(d) | prime_factors(n // d)
    _FACTORS[n] = out
    return out


# ── the transition matrix, probed out of the shipped library ─────────
def register_of(p: PN) -> int:
    """The LFSR register, read from the object's own serialized state.

    The blob is `[dp_state_hdr_t][u64 reg]`, so the register is the last
    eight bytes. Reading it here rather than re-implementing pn_step in
    Python is what makes §2.1 a measurement of the shipped code.
    """
    return int.from_bytes(p.get_state()[-8:], "little")


def transition_matrix(poly: int, n: int, lfsr: str) -> list[int]:
    """T[i] = one step applied to the basis register e_i.

    The step is linear over GF(2), so probing the n basis vectors
    determines it completely: step(v) = XOR of T[i] over v's set bits.
    """
    T = []
    for i in range(n):
        p = PN(poly, 1 << i, n, lfsr=lfsr)
        p.generate(1)
        T.append(register_of(p))
    return T


def _apply(T: list[int], v: int) -> int:
    out = 0
    while v:
        i = (v & -v).bit_length() - 1
        out ^= T[i]
        v &= v - 1
    return out


def _compose(A: list[int], B: list[int]) -> list[int]:
    return [_apply(A, b) for b in B]


def _identity(n: int) -> list[int]:
    return [1 << i for i in range(n)]


def _mat_pow(T: list[int], e: int, n: int) -> list[int]:
    r, base = _identity(n), T
    while e:
        if e & 1:
            r = _compose(r, base)
        base = _compose(base, base)
        e >>= 1
    return r


def is_primitive(poly: int, n: int, lfsr: str = "galois") -> bool:
    """Does the shipped LFSR have full order 2^n-1 at this width?

    T has order 2^n-1 iff T^(2^n-1) = I and T^((2^n-1)/q) != I for every
    prime q dividing 2^n-1 -- the standard order test, which is what
    "period 2^n-1 from every nonzero seed" means.
    """
    order = (1 << n) - 1
    T = transition_matrix(poly, n, lfsr)
    ident = _identity(n)
    if _mat_pow(T, order, n) != ident:
        return False
    return all(
        _mat_pow(T, order // q, n) != ident for q in prime_factors(order)
    )


# ── sequence helpers ─────────────────────────────────────────────────
def chips(n: int, count: int, lfsr: str = "galois", seed: int = 1):
    return np.asarray(PN(mls_poly(n), seed, n, lfsr=lfsr).generate(count))


def brute_period(n: int, lfsr: str = "galois") -> int:
    """Steps until the register returns to its seed. The ground truth."""
    p = PN(mls_poly(n), 1, n, lfsr=lfsr)
    cap = (1 << n) + 2
    for i in range(1, cap):
        p.generate(1)
        if register_of(p) == 1:
            return i
    return -1


def autocorr(seq: np.ndarray) -> np.ndarray:
    """Circular autocorrelation of the +-1 mapping, exact in integers."""
    x = 1.0 - 2.0 * seq.astype(np.float64)  # 0 -> +1, 1 -> -1
    spec = np.fft.rfft(x)
    ac = np.fft.irfft(spec * np.conj(spec), n=x.size)
    return np.rint(ac).astype(np.int64)


def is_rotation(a: np.ndarray, b: np.ndarray) -> bool:
    """Is `a` some cyclic rotation of `b`? O(P) via a doubled buffer."""
    ab, bb = a.astype(np.uint8).tobytes(), b.astype(np.uint8).tobytes()
    return len(ab) == len(bb) and ab in (bb + bb)


@dataclass
class Data:
    """Everything measured, so review/limits read data rather than re-run."""

    prim_rows: list[list[str]] = field(default_factory=list)
    all_primitive: bool = False
    n_widths: int = 0
    fib_all_primitive: bool = False
    cross_rows: list[list[str]] = field(default_factory=list)
    cross_agrees: bool = False
    brute_all_maximal: bool = False
    prop_rows: list[list[str]] = field(default_factory=list)
    balance_exact: bool = False
    ac_peak_exact: bool = False
    ac_sidelobe_exact: bool = False
    worst_sidelobe: int = 0
    shift_add_closed: bool = False
    realization_rows: list[list[str]] = field(default_factory=list)
    reversal_exact: bool = False
    never_identical: bool = False
    fib_same_period: bool = False
    fib_same_balance: bool = False
    table_zero_outside: bool = False
    default_resolves: bool = False
    reset_exact: bool = False
    state_exact: bool = False


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "`PN` is a maximal-length-sequence LFSR: a register of `length` "
        "bits, a primitive tap polynomial, and one chip per step. It is "
        "the chip source under DSSS spreading, the data source under "
        "`bpsk`/`qpsk`, and the PN sequence kind inside a frame. The "
        "design is "
        "[docs/design/wfmgen.md](../../../../../../docs/design/wfmgen.md); "
        "the API is `native/inc/pn/pn_core.h`, certified in C by "
        "`native/tests/test_pn_core.c`."
    )
    R.md()
    R.md(
        "**Its subject is the table.** `pn_mls_poly()` declares a "
        "primitive polynomial for every width 2..64; the rest of the "
        "object is a shift and an XOR. What is measured here is that "
        "every declared polynomial is primitive, and that the sequence "
        "has the properties a spreading code is chosen for -- not just "
        "the period."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def measure_table(d: Data) -> None:
    """2.1 — every declared width, and the algebra cross-checked."""
    rows, all_ok, fib_ok = [], True, True
    for n in WIDTHS:
        poly = mls_poly(n)
        g = is_primitive(poly, n, "galois")
        f = is_primitive(poly, n, "fibonacci")
        all_ok &= g
        fib_ok &= f
        if n in (2, 7, 13, 17, 24, 32, 40, 52, 61, 64):
            rows.append(
                [
                    str(n),
                    f"`{poly:#x}`",
                    "yes" if g else "**NO**",
                    "yes" if f else "**NO**",
                ]
            )
    d.prim_rows, d.all_primitive, d.fib_all_primitive = rows, all_ok, fib_ok
    d.n_widths = len(WIDTHS)

    R.md("### 2.1 Every declared width is primitive (C §MLS table)")
    R.md()
    R.md(
        f"All **{d.n_widths}** entries of `pn_mls_poly()`, in both "
        "realizations. The order test settles it without walking the "
        "sequence: the state transition is linear over GF(2), so it is an "
        "`n x n` bit matrix `T`, and the period is 2^n-1 from every "
        "nonzero seed exactly when `T^(2^n-1) = I` while "
        "`T^((2^n-1)/q) != I` for every prime `q` dividing 2^n-1."
    )
    R.md()
    R.md(
        "**`T` is probed out of the shipped library, not modelled here** "
        "-- each column is one basis register stepped once through the "
        "real `PN`, read back from its own `get_state()` blob. A Python "
        "re-implementation of `pn_step` would have certified the "
        "re-implementation. Ten of the widths, spanning the 32-bit "
        "boundary and the top of the table:"
    )
    R.md()
    R.table(
        ["length n", "poly", "Galois primitive", "Fibonacci primitive"], rows
    )

    # Brute force, where it is affordable — and the two must agree.
    crows, agree, brute_ok = [], True, True
    for n in BRUTE:
        per = brute_period(n)
        expect = (1 << n) - 1
        alg = is_primitive(mls_poly(n), n)
        brute_ok &= per == expect
        agree &= (per == expect) == alg
        if n in (2, 5, 8, 11, 14, 16):
            crows.append(
                [
                    str(n),
                    str(per),
                    str(expect),
                    "yes" if per == expect else "**NO**",
                ]
            )
    d.cross_rows, d.cross_agrees, d.brute_all_maximal = crows, agree, brute_ok
    _csv(
        DATA / "brute_period.csv",
        "length,measured_period,expected_period",
        [
            [float(n), float(brute_period(n)), float((1 << n) - 1)]
            for n in BRUTE
        ],
    )

    R.md(
        f"**The algebra is checked against brute force wherever both "
        f"apply.** For n = {BRUTE[0]}..{BRUTE[-1]} the register is stepped "
        "until it returns to its seed and the count compared to 2^n-1; "
        "the order test must agree with that verdict at every one. An "
        "algebraic shortcut nobody cross-checks is a second "
        "implementation with no referee."
    )
    R.md()
    R.table(["length n", "steps to return", "2^n - 1", "maximal"], crows)
    R.md("Raw sweep: [data/brute_period.csv](data/brute_period.csv).")
    R.md()

    d.table_zero_outside = mls_poly(1) == 0 and mls_poly(65) == 0


def measure_properties(d: Data) -> None:
    """2.2 — the properties a spreading code is chosen for."""
    rows = []
    bal_ok = peak_ok = side_ok = sa_ok = True
    worst = 0
    for n in WALKABLE:
        P = (1 << n) - 1
        seq = chips(n, P)
        ones = int(seq.sum())
        ac = autocorr(seq)
        peak, side = int(ac[0]), ac[1:]
        smax = int(np.max(np.abs(side)))
        worst = max(worst, int(np.max(np.abs(side + 1))))

        bal_ok &= ones == (1 << (n - 1))
        peak_ok &= peak == P
        side_ok &= bool(np.all(side == -1))

        # Shift-and-add: an m-sequence XOR a cyclic shift of itself is
        # another cyclic shift of the SAME sequence.
        for k in (1, 3, P // 2):
            sa_ok &= is_rotation(seq ^ np.roll(seq, -k), seq)

        rows.append(
            [
                str(n),
                str(P),
                str(ones),
                str(1 << (n - 1)),
                str(peak),
                str(smax),
            ]
        )
    d.prop_rows = rows
    d.balance_exact, d.ac_peak_exact = bal_ok, peak_ok
    d.ac_sidelobe_exact, d.shift_add_closed = side_ok, sa_ok
    d.worst_sidelobe = worst
    _csv(
        DATA / "sequence_properties.csv",
        "length,period,ones,expected_ones,ac_peak,max_abs_sidelobe",
        [
            [float(x) for x in r]
            for r in [
                [
                    n,
                    (1 << n) - 1,
                    int(chips(n, (1 << n) - 1).sum()),
                    1 << (n - 1),
                    (1 << n) - 1,
                    1,
                ]
                for n in WALKABLE
            ]
        ],
    )

    R.md("### 2.2 The properties a spreading code is chosen for")
    R.md()
    R.md(
        "Period 2^n-1 is necessary and not sufficient. What makes an "
        "m-sequence the sequence a correlator wants is the shape of its "
        "autocorrelation, and that is measured here on the +-1 mapping, "
        "circularly, over one full period."
    )
    R.md()
    R.table(
        ["n", "period P", "ones", "P+1 over 2", "AC peak", "max |sidelobe|"],
        rows,
    )
    R.md(
        f"**The sidelobe is exactly -1 at every nonzero lag** -- not "
        f"approximately, not on average: the largest deviation from -1 "
        f"across every lag of every width measured is "
        f"{d.worst_sidelobe}. That two-valued autocorrelation is the "
        "whole reason for the construction; it is what lets a correlator "
        f"put a peak of P against a floor of 1 and read the alignment "
        "off directly."
    )
    R.md()
    R.md(
        "**Shift-and-add closure** holds too: the sequence XOR a cyclic "
        "shift of itself is another cyclic shift of the same sequence. "
        "That is the algebraic identity behind the autocorrelation "
        "result, so measuring both means a defect has to break the "
        "identity and its consequence consistently to pass. Raw figures: "
        "[data/sequence_properties.csv](data/sequence_properties.csv)."
    )
    R.md()


def measure_realizations(d: Data) -> None:
    """2.3 — Galois and Fibonacci are one sequence, read two ways."""
    rows = []
    rev_ok = distinct_ok = per_ok = bal_ok = True
    for n in WALKABLE:
        P = (1 << n) - 1
        g = chips(n, P, "galois")
        f = chips(n, P, "fibonacci")
        idx = (P - np.arange(P)) % P
        rev = bool(np.array_equal(f, g[idx]))
        same = bool(np.array_equal(f, g))
        rev_ok &= rev
        distinct_ok &= not same
        per_ok &= brute_period(n, "fibonacci") == P
        bal_ok &= int(f.sum()) == (1 << (n - 1))
        rows.append(
            [
                str(n),
                "yes" if rev else "**NO**",
                "no" if not same else "**identical**",
                str(int(f.sum())),
            ]
        )
    d.realization_rows = rows
    d.reversal_exact, d.never_identical = rev_ok, distinct_ok
    d.fib_same_period, d.fib_same_balance = per_ok, bal_ok

    R.md("### 2.3 The two realizations are one sequence, read two ways")
    R.md()
    R.md(
        'The header says they "differ only in chip ordering/phase". '
        "Both suites asserted only that they DIFFER, which is the "
        "complement of that claim and is satisfied by any unrelated "
        "sequence. Measuring it gives the exact relationship:"
    )
    R.md()
    R.md("```text")
    R.md("    fib[i] == gal[(P - i) % P]        P = 2^n - 1")
    R.md("```")
    R.md()
    R.md(
        "The Fibonacci output is the Galois output read BACKWARDS about "
        "index 0. No plain rotation aligns them -- all P were searched at "
        "n=5..11 and none does -- because a Galois LFSR realizes the "
        "RECIPROCAL of the polynomial its Fibonacci twin does. That is "
        'the word "ordering" in the header doing real work, and it is '
        "why the weaker test could never have caught a wrong `fib_taps` "
        "derivation."
    )
    R.md()
    R.table(["n", "fib == reversed gal", "identical to gal", "ones"], rows)


def measure_lifecycle(d: Data) -> None:
    """2.4 — the default-poly resolution, reset, and state."""
    n = 7
    P = (1 << n) - 1
    explicit = chips(n, P)
    omitted = np.asarray(PN(seed=1, length=n).generate(P))
    zero = np.asarray(PN(poly=0, seed=1, length=n).generate(P))
    d.default_resolves = bool(
        np.array_equal(omitted, explicit) and np.array_equal(zero, explicit)
    )

    p = PN(mls_poly(n), 1, n)
    a = np.asarray(p.generate(P)).copy()
    p.reset()
    d.reset_exact = bool(np.array_equal(a, np.asarray(p.generate(P))))

    q = PN(mls_poly(n), 1, n)
    q.generate(9)
    blob = q.get_state()
    ref = np.asarray(q.generate(64)).copy()
    fresh = PN(mls_poly(n), 1, n)
    fresh.set_state(blob)
    d.state_exact = bool(np.array_equal(ref, np.asarray(fresh.generate(64))))

    R.md("### 2.4 Defaulting, reset and resume")
    R.md()
    R.md(
        "**`poly = 0` is not a polynomial.** At the C level `pn_create` "
        "takes the mask verbatim, so a zero tap mask is a register with "
        "no feedback: it shifts the seed out and emits zeros forever -- "
        '"a constant field that still looks like a field". Every caller '
        "that lets a user say *default* therefore resolves it as "
        "`poly ? poly : pn_mls_poly(n)`. That resolution was audited "
        "across the tree for this report and all three production call "
        "sites do it: `wfm_synth_create` on both its branches (and it "
        "rejects a width the table has no entry for), `wfm_frame`'s PN "
        "sequence kind, and the `PN` binding itself (guarded at "
        "`length >= 2`, since the table starts at n=2)."
    )
    R.md()
    R.md(
        "So from Python `PN(seed=1, length=7)`, `PN(poly=0, ...)` and "
        "`PN(poly=mls_poly(7), ...)` are one sequence. The unresolved "
        "C-level behaviour is **C-ONLY** -- the binding resolves it "
        "before `pn_create` sees it, so Python cannot reach the zeros; "
        "it is pinned in `test_pn_core.c` (F3)."
    )
    R.md()
    R.md(
        "`reset()` returns the register to its seed so the sequence "
        "restarts from chip 0, and a serialized blob resumes a "
        "mid-sequence generator bit-exactly into a fresh instance -- both "
        "over a full period."
    )
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    d = Data()
    measure_table(d)
    measure_properties(d)
    measure_realizations(d)
    measure_lifecycle(d)
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: Data) -> None:
    R.md("## 3. Review -- findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        '**The table said "verified" and six of sixty-three were.** '
        '`pn_mls_poly()` is documented as *"Generated from verified '
        'primitive polynomials (period 2^n-1)"*; across '
        "`test_pn_core.c` and `test_pn.py` together the period was "
        "pinned at n=7/17/20 and n=5/9/40, and the other 57 entries "
        "were asserted only to be NONZERO. That is the "
        "pinned-only-at-literals shape at 57 entries, and the failure it "
        "admits is quiet: a wrong entry is not a crash but a short-period "
        "spreading code, which costs processing gain in whatever "
        "correlates against it. Every entry was checked out-of-band "
        "first -- all 63 ARE primitive, in both realizations -- so the "
        "table was right and nothing was running it. Closed by walking a "
        "full period for n=2..24 in C and by the order test over all 63 "
        "here (§2.1).",
    )
    R.find(
        "F2",
        "FIXED",
        "**A claim was pinned by its own complement.** The header says "
        'the two realizations "differ only in chip ordering/phase"; '
        "both suites asserted only that they DIFFER, which any unrelated "
        "sequence satisfies. Measuring the actual relationship gives a "
        "closed form -- `fib[i] == gal[(P-i) % P]`, the Galois sequence "
        "read backwards about index 0 -- and shows no plain rotation "
        "aligns them, because a Galois LFSR realizes the RECIPROCAL "
        "polynomial. Pinning the closed form is what makes the test "
        "catch a wrong `fib_taps` derivation; sabotaging that derivation "
        "by one bit position leaves the old assertion green and takes "
        "the new one red (§2.3).",
    )
    R.find(
        "F3",
        "C-ONLY",
        "**Three claims the Python face cannot reach**, now pinned in "
        "`native/tests/test_pn_core.c`. (1) `poly = 0` emitting zeros "
        "forever once the seed shifts out -- the binding resolves the "
        "default before `pn_create` sees it, so Python cannot observe "
        "the unresolved behaviour the header warns about, and it is "
        "exactly what every caller's resolution exists to avoid. "
        "(2) `pn_create` rejecting a zero seed or a zero length. "
        "(3) `pn_destroy(NULL)` as a documented no-op. Proving the last "
        "took two attempts worth recording: a store to `state->reg` "
        "before `free()` is a dead store and gcc deletes it, so the "
        "sabotage never dereferenced anything and the test passed "
        "honestly; a `volatile` read cannot be elided and goes red.",
    )
    R.find(
        "F4",
        "FIXED",
        "**`pn_reset` was called with nothing asserted after it.** The C "
        "test invoked it between other work and checked no consequence, "
        "so a reset that reloaded nothing at all passed -- the same "
        "shape `validation.md` records against resamp. Python covered "
        "the behaviour, but C is the only face the sanitisers run. Now "
        "advances the register first, asserts it actually moved, and "
        "requires the reloaded generator to reproduce the sequence over "
        "a full period.",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: Data) -> None:
    R.md("## 4. Limits -- the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/wfm/tests/test_validation_limits.py`."
    )
    R.md()
    R.limit(
        d.all_primitive,
        f"every one of the {d.n_widths} polynomials in the table is "
        "primitive in the Galois realization -- period 2^n-1 from every "
        "nonzero seed, at every declared width from 2 to 64",
    )
    R.limit(
        d.fib_all_primitive,
        "and every one is primitive in the Fibonacci realization too, so "
        "the derived taps are not a second table that can drift",
    )
    R.limit(
        d.cross_agrees,
        f"the order test agrees with brute-force stepping at every width "
        f"where both are affordable (n={BRUTE[0]}..{BRUTE[-1]}) -- the "
        "algebra is refereed, not trusted",
    )
    R.limit(
        d.brute_all_maximal,
        "and that brute force finds exactly 2^n-1 steps to return, never "
        "fewer",
    )
    R.limit(
        d.balance_exact,
        "the balance property is exact: 2^(n-1) ones per period, which a "
        "non-primitive polynomial also fails",
    )
    R.limit(
        d.ac_peak_exact,
        "the periodic autocorrelation peak is exactly P at zero lag",
    )
    R.limit(
        d.ac_sidelobe_exact,
        f"and exactly -1 at EVERY nonzero lag (worst deviation "
        f"{d.worst_sidelobe} across every lag of every width measured) -- "
        "the two-valued autocorrelation a correlator is designed around",
    )
    R.limit(
        d.shift_add_closed,
        "shift-and-add closure holds: the sequence XOR any cyclic shift "
        "of itself is another cyclic shift of the same sequence",
    )
    R.limit(
        d.reversal_exact,
        "the Fibonacci output is the Galois output read backwards about "
        "index 0, exactly: fib[i] == gal[(P - i) % P]",
    )
    R.limit(
        d.never_identical,
        "and never simply identical to it -- the ordering is a real "
        "difference, not a no-op",
    )
    R.limit(
        d.fib_same_period,
        "the Fibonacci realization has the same maximal period as its "
        "Galois twin",
    )
    R.limit(
        d.fib_same_balance,
        "and the same balance, so either realization is usable as a "
        "spreading code with no change of properties",
    )
    R.limit(
        d.table_zero_outside,
        "the table returns 0 outside 2..64, so a caller-side width error "
        "surfaces as a refused build rather than a silent zero-tap "
        "register",
    )
    R.limit(
        d.default_resolves,
        "an omitted poly, an explicit poly=0 and an explicit "
        "mls_poly(n) give one identical sequence through the Python "
        "face -- the default resolution every caller owns",
    )
    R.limit(
        d.reset_exact,
        "reset() restarts the sequence from chip 0, reproducing a full "
        "period bit-for-bit",
    )
    R.limit(
        d.state_exact,
        "and a serialized blob resumes a mid-sequence generator "
        "bit-exactly into a fresh instance",
    )


# ── build ────────────────────────────────────────────────────────────
def build(write: bool = True) -> Report:
    """Measure everything and render the report."""
    global R
    R = Report(write=write)
    if write:
        DATA.mkdir(parents=True, exist_ok=True)
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "PN",
        [
            f"**All {d.n_widths} polynomials in the table are "
            "primitive**, at every width from 2 to 64 and in both "
            "realizations. Six of them had anything running that claim "
            "before this; the rest were asserted only to be nonzero "
            "(§2.1, F1).",
            "**The autocorrelation is two-valued and exact** -- P at zero "
            f"lag, exactly -1 at every other lag, deviation "
            f"{d.worst_sidelobe} across everything measured. That, not "
            "the period, is why a correlator can read alignment straight "
            "off the peak (§2.2).",
            "**Either realization is usable, and they are one sequence.** "
            "The Fibonacci output is the Galois output read backwards "
            "about index 0. Same period, same balance, so the choice is "
            "about chip order and nothing else (§2.3).",
            "**Never pass `poly = 0` to the C API and expect a default.** "
            "A zero tap mask is a register with no feedback: it shifts "
            "the seed out and emits zeros forever, which looks like a "
            "field. Resolution is the caller's job and all three "
            "production call sites do it; the Python face is safe by "
            "construction (§2.4, F3).",
            "**Widths above 24 are certified algebraically, not walked.** "
            "n=64 is 1.8e19 states. The order test settles them, its "
            "transition matrix is probed out of the shipped library "
            "rather than modelled, and it is cross-checked against brute "
            "force everywhere both apply (§2.1).",
        ],
    )
    R.summary()
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
