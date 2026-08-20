"""coding_demo.py — name your own code, in both directions.

`doppler.coding` holds the code FAMILIES a standard configures, rather than
any standard's picks: `ReedSolomon` is both directions of an RS code over
`GF(2**J)`, and `ConvEncoder` / `Viterbi` are the two directions of a
rate-1/n convolutional code. All three take the code as arguments, so this
file uses one that is **nobody's standard** — RS(255,239) over the textbook
GF(256), inside a K=7 rate-1/2 inner code with no output inverted.

That is the point. Until doppler#900 the library could decode any
convolutional code and encode exactly one, and the only Reed-Solomon
reachable from Python was CCSDS's, fixed, inside a frame descriptor that
carries an interleaving depth rather than a code.

Two things are demonstrated, and the second is the one worth the plot.

**1. The chain runs.** Outer code, inner code, a real AWGN channel, sync
acquisition, then back up: Viterbi, then Reed-Solomon. The payload comes out
bit-exact, and the assert says so.

**2. Past its radius, a decoder can be SILENTLY WRONG.** `rs_core.h` says it
plainly — "beyond `E` a bounded-distance decoder can land inside another
codeword's sphere and miscorrect, a property of the code, not of this
implementation" — and nothing in the tree showed it. A refusal is a fine
outcome: the receiver knows. A miscorrection is not: the decoder returns a
positive count, the word passes `codeword_ok`, and the payload is wrong.

**That study runs on a deliberately WEAK code**, RS(15,11) over GF(16), and
the reason is the measurement rather than the story. The chance a random
error pattern lands in some other codeword's sphere is about
`sum(C(n,i)(q-1)**i for i <= E) / q**(n-k)` — **2e-05** for the RS(255,239)
above and **0.36** for RS(15,11). Sweeping the strong code would need a
quarter of a million frames to see a handful of events, and an example that
reported zero would be reporting its own sample size. The scaling IS the
engineering answer: parity is what buys the silence.

Run:
    python src/doppler/examples/coding_demo.py
"""

from __future__ import annotations

import math

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# --8<-- [start:chain]
import numpy as np

from doppler.coding import ConvEncoder, ReedSolomon, Viterbi
from doppler.detection import SyncFinder

# A code that is nobody's standard: RS(255,239) over the textbook GF(256),
# and the K=7 rate-1/2 inner code with NOTHING inverted (CCSDS complements
# its second output; this does not).
OUTER = {"nroots": 16}
INNER_POLY = [0o171, 0o133]
K_INNER = 7
DEPTH = 35

# A Viterbi decoder answers LATE: it holds `depth - 1` decisions back while
# the traceback catches up, so a frame encoded with nothing after it comes
# out short by exactly that much and its tail is never emitted. Flushing the
# encoder with `depth` known bits is what buys the tail back -- the classic
# thing to forget, and it presents as a frame that is the wrong length rather
# than as anything that looks like a coding problem.
TAIL = np.zeros(DEPTH, np.uint8)

# A 32-bit marker of our own, for the same reason CCSDS has one: a receiver
# has to find where the frame starts, and the marker is what tells it -- and
# in which polarity.
MARKER = np.unpackbits(np.frombuffer(b"\xd3\x91\x2a\x7c", np.uint8))


def encode_frame(payload: np.ndarray) -> np.ndarray:
    """Payload octets -> channel symbols: outer code, marker, inner code."""
    rs = ReedSolomon(**OUTER)
    codeword = rs.encode(payload)  # k info + 2E parity
    bits = np.unpackbits(codeword)

    # The marker is prepended AFTER the outer code and covered by the inner
    # one -- the coverage asymmetry every framing with a sync word has.
    framed = np.concatenate([MARKER, bits, TAIL]).astype(np.uint8)
    return ConvEncoder(INNER_POLY, k=K_INNER).encode(framed)


def decode_frame(llr: np.ndarray, n_frame_bits: int) -> np.ndarray | None:
    """Channel LLRs -> payload octets, or None if the frame is unusable."""
    bits = Viterbi(INNER_POLY, k=K_INNER, depth=DEPTH).decode(llr)

    # Acquire: where does the frame start, and which polarity is it in?
    hit = SyncFinder(MARKER).find(bits, max_errors=4)
    if not hit.found:
        return None
    body = bits[hit.offset + MARKER.size : hit.offset + n_frame_bits]
    if hit.inverted:
        body = (body ^ 1).astype(np.uint8)

    rs = ReedSolomon(**OUTER)
    if body.size != rs.n * 8:
        return None
    word = np.packbits(body)
    if rs.decode(word) < 0:
        return None  # too far from any codeword
    return word[: rs.k]


# --8<-- [end:chain]


def _channel(sym: np.ndarray, ebn0_db: float, rng) -> np.ndarray:
    """BPSK over AWGN, returned as LLRs. Rate 1/2, so Es/N0 = Eb/N0 - 3 dB."""
    esn0 = 10 ** ((ebn0_db - 10 * np.log10(2.0)) / 10.0)
    sigma = np.sqrt(1.0 / (2.0 * esn0))
    tx = np.where(sym, -1.0, 1.0)
    rx = tx + rng.normal(0.0, sigma, tx.size)
    return (2.0 * rx / sigma**2).astype(np.float32)


# The weak code the miscorrection study runs on. Nothing sends this; it is
# chosen so the effect is visible in seconds instead of hours.
WEAK = {
    "nroots": 4,
    "symbol_bits": 4,
    "field_poly": 0b0011,
}  # RS(15,11), GF(16)


def sphere_fraction(rs: ReedSolomon) -> float:
    """Rough chance a random word lands in SOME codeword's decoding sphere.

    `q**k` codewords, each claiming `sum(C(n,i)(q-1)**i for i <= E)` words,
    out of `q**n` — so the codewords cancel and only the PARITY count is
    left. That is the whole engineering content: miscorrection is bought
    down by `n - k`, and by nothing else.
    """
    q = 1 << rs.symbol_bits
    vol = sum(math.comb(rs.n, i) * (q - 1) ** i for i in range(rs.e + 1))
    return vol / q ** (rs.n - rs.k)


def _outcomes(
    rs: ReedSolomon, n_err: int, trials: int, rng
) -> tuple[int, int, int]:
    """corrected / refused / MISCORRECTED, at exactly `n_err` symbol errors.

    A miscorrection is the case that needs naming: `decode` returns a
    non-negative count and the result passes `codeword_ok`, because it IS a
    codeword -- just not the one that was sent.
    """
    good = refused = wrong = 0
    hi = 1 << rs.symbol_bits

    for _ in range(trials):
        info = rng.integers(0, hi, rs.k).astype(np.uint8)
        clean = rs.encode(info)
        word = clean.copy()
        pos = rng.choice(rs.n, size=n_err, replace=False)
        word[pos] ^= rng.integers(1, hi, n_err).astype(np.uint8)

        if rs.decode(word) < 0:
            refused += 1
        elif np.array_equal(word, clean):
            good += 1
        else:
            wrong += 1
    return good, refused, wrong


def main() -> None:
    rng = np.random.default_rng(20260820)
    rs = ReedSolomon(**OUTER)

    print(f"outer  RS({rs.n},{rs.k}) over GF(2^{rs.symbol_bits}), E = {rs.e}")
    print(f"inner  K={K_INNER} rate-1/2, poly {[oct(p) for p in INNER_POLY]}")
    print(f"marker {MARKER.size} bits\n")

    # ── 1. the chain, end to end ────────────────────────────────────────
    payload = rng.integers(0, 256, rs.k).astype(np.uint8)
    sym = encode_frame(payload)
    n_frame_bits = MARKER.size + rs.n * 8
    assert sym.size == 2 * (n_frame_bits + TAIL.size), (
        "rate 1/2 over marker + codeword + the decoder's flush"
    )

    # Lead-in the receiver has to search through before the frame arrives.
    lead = rng.integers(0, 2, 64).astype(np.uint8)
    stream = np.concatenate([lead, sym]).astype(np.uint8)

    for ebn0 in (3.0, 4.0, 5.0):
        llr = _channel(stream, ebn0, rng)
        got = decode_frame(llr, n_frame_bits)
        ok = got is not None and np.array_equal(got, payload)
        print(f"  Eb/N0 = {ebn0:>4.1f} dB   payload recovered: {ok}")

    llr = _channel(stream, 5.0, rng)
    got = decode_frame(llr, n_frame_bits)
    assert got is not None, "the frame must be acquired at 5 dB"
    assert np.array_equal(got, payload), "and recovered bit-exact"

    # ── 2. what happens past the radius ─────────────────────────────────
    weak = ReedSolomon(**WEAK)
    print("\n  miscorrection is bought down by PARITY, and only by that:")
    print(
        f"    RS({rs.n},{rs.k})  n-k = {rs.n - rs.k:>3}  "
        f"sphere fraction ~ {sphere_fraction(rs):.2e}"
    )
    print(
        f"    RS({weak.n},{weak.k})    n-k = {weak.n - weak.k:>3}  "
        f"sphere fraction ~ {sphere_fraction(weak):.2e}"
    )
    print("  so the sweep below runs on the WEAK code, where the effect is")
    print("  measurable in seconds rather than in a quarter-million frames.")

    TRIALS = 2000
    errs = list(range(max(0, weak.e - 2), weak.e + 5))
    good, refused, wrong = [], [], []
    for m in errs:
        g, r, w = _outcomes(weak, m, TRIALS, rng)
        good.append(g / TRIALS)
        refused.append(r / TRIALS)
        wrong.append(w / TRIALS)

    print(f"\n  symbol errors -> outcome, {TRIALS} frames each (E = {weak.e})")
    print("     errs   corrected    refused   MISCORRECTED")
    for m, g, r, w in zip(errs, good, refused, wrong):
        mark = "  <- E" if m == weak.e else ""
        print(f"    {m:>5}   {g:>9.3f}  {r:>9.3f}   {w:>12.3f}{mark}")

    # The claims, as asserts rather than as a picture.
    at_e = errs.index(weak.e)
    assert good[at_e] == 1.0, "at E every error pattern must be corrected"
    assert wrong[at_e] == 0.0, "and none may be miscorrected"
    assert good[at_e + 1] == 0.0, "past E the true codeword is not recovered"
    assert wrong[at_e + 1] > 0.1, (
        "past E a bounded-distance decoder MUST sometimes miscorrect on a "
        "code this weak — a sweep that never does is measuring its own "
        "sample size, which is what the strong code above would have done"
    )
    # Well past E the error pattern is effectively a random word, so the
    # miscorrection rate must approach the sphere-packing fraction computed
    # from the code's parameters alone. Measurement against theory, with
    # nothing shared between them but the code.
    tail = sum(wrong[-3:]) / 3.0
    assert abs(tail - sphere_fraction(weak)) < 0.05, (
        f"far past E the miscorrection rate should approach the sphere "
        f"fraction {sphere_fraction(weak):.3f}, measured {tail:.3f}"
    )
    for m, g in zip(errs, good):
        assert (g == 1.0) == (m <= weak.e), (
            "every pattern within E is corrected and none beyond it is — "
            "the code's distance, not a tuning choice"
        )

    # ── 3. the plot ─────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(7.4, 4.2))
    # BARS, not a stackplot: the outcome is a function of an INTEGER error
    # count, and a filled area drawn between 2 and 3 invites reading a
    # gradient off a quantity that has no values in between.
    # MISCORRECTED goes at the BOTTOM so its top edge is its own height,
    # which is what the theory line predicts. Stacked on top it would be a
    # band floating at 0.63-1.0 with a reference line drawn at 0.36 -- a
    # height to be compared against a level, which no reader can do.
    g, r, w = np.array(good), np.array(refused), np.array(wrong)
    ax.bar(
        errs,
        w,
        width=0.82,
        color="#c92a2a",
        label="MISCORRECTED — the receiver does not know",
    )
    ax.bar(
        errs,
        r,
        width=0.82,
        bottom=w,
        color="#f59f00",
        label="refused — the receiver knows",
    )
    ax.bar(
        errs, g, width=0.82, bottom=w + r, color="#2b8a3e", label="corrected"
    )

    ax.axhline(sphere_fraction(weak), color="k", ls="--", lw=1.4)
    ax.annotate(
        f"sphere fraction {sphere_fraction(weak):.2f}"
        f" = $\\Sigma_{{i\\leq E}}\\binom{{n}}{{i}}(q-1)^i / q^{{n-k}}$",
        (errs[-1] + 0.45, sphere_fraction(weak) + 0.015),
        ha="right",
        va="bottom",
        fontsize=9,
    )
    ax.axvline(weak.e + 0.5, color="k", ls=":", lw=1)
    ax.annotate(f"E = {weak.e}", (weak.e + 0.42, 1.02), ha="right", fontsize=9)

    ax.set_xticks(errs)
    ax.set_xlim(errs[0] - 0.6, errs[-1] + 0.6)
    ax.set_ylim(0, 1.12)
    ax.set_xlabel("symbol errors in the codeword")
    ax.set_ylabel("fraction of frames")
    ax.set_title(
        f"RS({weak.n},{weak.k}) past its radius — a refusal is safe, "
        "a miscorrection is not"
    )
    ax.legend(
        loc="lower left",
        bbox_to_anchor=(0.0, -0.02),
        framealpha=0.95,
        fontsize=9,
    )
    fig.tight_layout()
    fig.savefig("coding_demo.png", dpi=110)
    plt.close(fig)
    print("\nwrote coding_demo.png")


if __name__ == "__main__":
    main()
