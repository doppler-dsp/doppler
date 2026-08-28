"""interleave_burst_demo.py — what an interleaver actually buys.

A round trip proves an interleaver is invertible. It says nothing about why
anyone would apply one, which is the only interesting question about a
transform that adds no redundancy and detects nothing.

So this demonstrates the property a link budget is written against: a **burst**
of errors that an outer code cannot correct becomes, after interleaving, errors
it can. And it demonstrates the bound, because an interleaver multiplies the
corrigible burst by the depth — it does not remove the limit, and a demo
showing only the good case would claim something stronger than the code does.

Three things are measured, each with an assertion. Exit 0 means demonstrated
AND checked.

**1. A burst inside the code's own power is fine either way.** The control:
without it, "interleaving helped" could be measuring a code that never failed.

**2. Past that, the interleaver is the difference.** A burst of `E+1` octets
breaks the bare code and is corrected once spread — and so is a burst of
`E * rows`, which is where the arithmetic says the gain runs out.

**3. The bound bites.** One octet past `E * rows`, everything fails.

The arrangement is one codeword per interleaver row. Reading by columns
transmits one symbol from each codeword in turn, so a burst of up to `rows`
consecutive symbols costs each codeword at most one. Interleaving a SINGLE
codeword would buy nothing at all — Reed-Solomon corrects any E symbol errors
wherever they fall — which is the first thing to understand about this
transform and the easiest to get wrong.

See docs/design/interleaving.md.
"""

import numpy as np

from doppler.coding import Deinterleaver, Interleaver, ReedSolomon

# --8<-- [start:setup]
N_CW = 5  # codewords interleaved — one per row
rs = ReedSolomon(nroots=32)  # RS(255,223), corrects E = 16 symbols
E = rs.e

# One codeword per row, so `cols` is the codeword length in octets.
tx = Interleaver(rows=N_CW, cols=rs.n, unit_bits=8)
rx = Deinterleaver(rows=N_CW, cols=rs.n, unit_bits=8)
# --8<-- [end:setup]


def _bits(octets):
    return np.unpackbits(octets.astype(np.uint8))


def _octets(bits):
    return np.packbits(bits.astype(np.uint8))


def frame_error(burst_octets, at, interleaved, seed=7):
    """Encode N_CW codewords, put a burst on the wire, try to recover.

    Returns True if any codeword failed — a frame is lost if any part is.
    """
    rng = np.random.default_rng(seed)
    info = rng.integers(0, 256, size=(N_CW, rs.k), dtype=np.uint8)

    # --8<-- [start:transmit]
    codewords = np.stack([rs.encode(row) for row in info])
    wire = _bits(codewords.reshape(-1))
    if interleaved:
        wire = np.asarray(tx.interleave(wire))
    # --8<-- [end:transmit]

    # The burst goes on the WIRE, which is what the interleaver was applied
    # to. Corrupting the codewords directly would measure nothing.
    n_bits = wire.size
    idx = (np.arange(burst_octets * 8) + at * 8) % n_bits
    wire = wire.copy()
    wire[idx] ^= 1

    # --8<-- [start:receive]
    if interleaved:
        wire = np.asarray(rx.deinterleave(wire))
    received = _octets(wire).reshape(N_CW, rs.n)
    # --8<-- [end:receive]

    # decode() corrects IN PLACE and returns the symbols it repaired, or a
    # negative count when the codeword is beyond the radius. The comparison
    # against the transmitted information is the verdict rather than that
    # return, because a decoder can "succeed" while miscorrecting — and a
    # frame lost that way is lost just the same.
    for c in range(N_CW):
        word = received[c].copy()
        if rs.decode(word) < 0:
            return True
        if not np.array_equal(word[: rs.k], info[c]):
            return True
    return False


def fer(burst_octets, interleaved, trials=12):
    total = rs.n * N_CW
    bad = sum(
        frame_error(burst_octets, (t * 37) % total, interleaved, seed=t + 1)
        for t in range(trials)
    )
    return bad / trials


def main():
    bound = E * N_CW
    print(f"{N_CW} x RS({rs.n},{rs.k}), E={E} each, one codeword per row")
    print(
        f"interleaver: rows={tx.rows} cols={tx.cols} unit_bits={tx.unit_bits}"
    )
    print(f"  burst_len={tx.burst_len}  separation={tx.separation}")
    print(
        f"  corrigible burst: E={E} octets bare, E*rows={bound} interleaved\n"
    )
    print("  burst   FER(bare)   FER(interleaved)")

    rows = {}
    for b in (E, E + 1, bound, bound + 1):
        rows[b] = (fer(b, False), fer(b, True))
        print(f"  {b:5d}   {rows[b][0]:9.3f}   {rows[b][1]:16.3f}")

    # 1. the control — within E, both correct, so a difference above E is
    #    the interleaver and not a code that was already failing
    assert rows[E] == (0.0, 0.0), rows[E]

    # 2. past E the bare code breaks and the interleaver rescues it, at E+1
    #    and all the way to the bound. Not 1.0 at E+1: a burst straddling a
    #    codeword boundary splits into two runs of at most E, and both
    #    correct — physical, not noise.
    assert rows[E + 1][0] > 0.5, rows[E + 1]
    assert rows[E + 1][1] == 0.0, rows[E + 1]
    assert rows[bound][0] == 1.0, rows[bound]
    assert rows[bound][1] == 0.0, rows[bound]

    # 3. and it stops exactly where the arithmetic says
    assert rows[bound + 1][1] == 1.0, rows[bound + 1]

    # 4. and the geometry is a link parameter, not a local choice. Nothing
    #    on the wire carries it and nothing checks it: a receiver holding
    #    different numbers returns an array of exactly the right length,
    #    raises nothing, and hands the decoder a different permutation.
    #    That is F3 of the validation report beside this file, and it is
    #    why the object refuses to infer `cols` from the input length.
    rng = np.random.default_rng(7)
    info = rng.integers(0, 256, size=(N_CW, rs.k), dtype=np.uint8)
    clean = _bits(np.stack([rs.encode(row) for row in info]).reshape(-1))
    sent = np.asarray(tx.interleave(clean))

    wrong_end = Deinterleaver(rows=rs.n, cols=N_CW, unit_bits=8)
    got = np.asarray(wrong_end.deinterleave(sent))
    wrong = float(np.mean(got != clean))
    print("\na receiver with rows and cols exchanged:")
    print(f"     no error raised, {got.size} bits back (the right count),")
    print(f"     {wrong * 100:.0f}% of them wrong")
    assert got.size == clean.size, (got.size, clean.size)
    assert wrong > 0.4, wrong

    print(f"\nOK — the corrigible burst went from {E} octets to {bound},")
    print(f"     and {bound + 1} still fails: a bound, not immunity.")
    print("     Both ends must be configured with the same three numbers.")


if __name__ == "__main__":
    main()
