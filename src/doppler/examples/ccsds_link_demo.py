"""ccsds_link_demo.py — a CCSDS CADU, end to end, as a DESCRIPTION.

A frame is a list of **fields** that appear on the wire and a list of
**stages** that transform them, each stage carrying the span it covers. This
demo builds one, damages it, and scores it — using nothing CCSDS-specific
except the numbers CCSDS picked.

The point is the coverage asymmetry. The four stages do **not** all cover the
same bits:

    stage                  covers the marker?   131.0-B-3
    Reed-Solomon (outer)   no                   9.5.1, 9.2.1.5
    pseudo-randomiser      no                   10.3.2, 10.3.4 note 1
    convolutional (inner)  YES                  3.2.1, 9.2.1.4

An assembler that gets any one of those backwards still encodes, still
decodes against a receiver of its own construction, and syncs to nothing.
That is why a frame is a description of spans rather than a chain of
transforms — a chain is right at three stage boundaries and wrong at the
fourth.

The second half is what a coded link buys on the RECEIVE side. A CRC reports
one bit: right or wrong. An outer code reports how much repair it took, so a
margin being spent is visible long before it is lost.

Run:
    python src/doppler/examples/ccsds_link_demo.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# --8<-- [start:cadu]
import numpy as np

from doppler.wfm import FrameDesc

# The five numbers CCSDS 131.0-B-3 section 4.3 picks, and the marker 9.4.1
# does. Everything else in this file is general.
K, N, E = 223, 255, 16  # RS(255,223), 16 correctable symbols
DEPTH = 5  # interleaving depth (4.3.5.1 allows 1,2,3,4,5,8)
ASM = 0x1ACFFC1D  # the attached sync marker (figure 9-1)

# Stage kinds — indices of wfm_stage_kind_t.
CRC16, RS, RANDOMISE, CONV = 0, 1, 2, 3

EMPTY = np.zeros(0, np.uint8)


def transfer_frame(depth: int = DEPTH) -> np.ndarray:
    """`223 * depth` octets of Transfer Frame, as unpacked bits."""
    octets = np.array(
        [(i * 37 + 11) & 0xFF for i in range(K * depth)], np.uint8
    )
    return np.unpackbits(octets).astype(np.uint8)


def describe_cadu(payload: np.ndarray, depth: int, *, inner: bool):
    """A CADU as three fields and three covers.

    `inner=False` describes the unit a frame checker sees: the convolutional
    code is streaming and emits its decisions `depth` bits late, so it is
    undone before frame synchronisation and a frame checker never sees
    channel symbols.
    """
    marker = np.array([(ASM >> (31 - i)) & 1 for i in range(32)], np.uint8)

    d = FrameDesc(EMPTY, EMPTY, EMPTY)  # start from nothing
    d.add_field(marker)  # 0: the ASM
    d.add_field(payload)  # 1: the Transfer Frame
    d.add_field(  # 2: the check symbols the outer code derives
        EMPTY, derived_by=1, derived_bits=32 * depth * 8
    )

    # THE COVERS. Fields 1..2 are the data group; field 0 is the marker, and
    # only the inner code reaches over it.
    d.add_stage(RS, first_field=1, n_fields=2, depth=depth)
    d.add_stage(RANDOMISE, first_field=1, n_fields=2)
    if inner:
        d.add_stage(CONV, first_field=0, n_fields=3, emit_num=2, emit_den=1)
    d.build()
    return d


# --8<-- [end:cadu]


def main() -> None:
    payload = transfer_frame()

    # ── 1. the shape, and the asymmetry that defines it ──────────────────
    coded = describe_cadu(payload, DEPTH, inner=True)
    cadu = describe_cadu(payload, DEPTH, inner=False)

    block_bits = N * DEPTH * 8
    assert cadu.nbits == 32 + block_bits, "CADU = marker + codeblock"
    assert coded.nbits == 2 * cadu.nbits, "the inner code doubles it"

    print("CADU layout")
    print(f"  marker        {cadu.field_bits(0):>7} bits")
    print(f"  transfer frame{cadu.field_bits(1):>7} bits")
    print(f"  R-S check     {cadu.field_bits(2):>7} bits")
    print(f"  channel syms  {coded.nbits:>7}\n")

    print("what each stage covers (bit offset + span)")
    names = ("outer (R-S)", "randomiser", "inner (conv)")
    for i, name in enumerate(names):
        first, n = coded.stage_first(i), coded.stage_bits(i)
        reaches = "marker included" if first == 0 else "starts behind marker"
        print(f"  {name:<13} {first:>6} + {n:<7}  {reaches}")

    # The claim, as an assertion rather than a comment.
    assert coded.stage_first(0) == 32, "9.5.1: outer code excludes the ASM"
    assert coded.stage_first(1) == 32, "10.3.4: the ASM was not randomized"
    assert coded.stage_first(2) == 0, "9.2.1.4: the inner code includes it"
    assert coded.stage_bits(2) == cadu.nbits, "...over the whole CADU"

    # The marker does NOT survive verbatim once the inner code runs — the
    # direct falsification of the other stage order.
    marker = np.asarray(cadu.bits(1))[:32]
    assert not np.array_equal(np.asarray(coded.bits(1))[:32], marker)

    # ── 2. what the outer code buys, measured ────────────────────────────
    clean = np.asarray(cadu.bits(1)).copy()
    blk = cadu.stage_first(0)

    r = cadu.check(clean)
    print(f"\nclean frame        : {r.ok}/{r.units} ok, {r.symbols} repaired")
    assert r.passed == 1 and r.symbols == 0, "a clean frame repairs nothing"

    # A contiguous burst is what interleaving exists for: at depth I it lands
    # as ceil(B/I) errors in each codeword, so depth trades no rate at all for
    # an I-fold longer correctable burst.
    bursts, repaired, survived = [], [], []
    for b in range(0, DEPTH * E + DEPTH + 1, DEPTH):
        rx = clean.copy()
        for s in range(b):
            rx[blk + s * 8] ^= 1  # one symbol each
        r = cadu.check(rx)
        bursts.append(b)
        repaired.append(r.symbols)
        survived.append(r.passed)

    edge = max(b for b, ok in zip(bursts, survived) if ok)
    print(f"longest burst held : {edge} symbols  (depth {DEPTH} x E {E})")
    assert edge == DEPTH * E, (
        f"interleaving depth {DEPTH} must carry a {DEPTH * E}-symbol burst, "
        f"held {edge}"
    )

    # One symbol past the radius, in ONE column: that codeword is refused and
    # the count says which. A CRC would say only "wrong".
    rx = clean.copy()
    for c in range(E + 1):
        rx[blk + (c * DEPTH + 2) * 8] ^= 1
    r = cadu.check(rx)
    print(
        f"E+1 in one codeword: {r.ok}/{r.units} ok "
        f"-> the frame is refused, and it names the cost"
    )
    assert r.passed == 0, "E+1 in one codeword must fail the frame"
    assert r.ok == r.units - 1, "and exactly one unit must be bad"

    # ── 3. the plot ──────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(6.4, 4.0))
    colour = ["#2b8a3e" if ok else "#c92a2a" for ok in survived]
    ax.bar(bursts, repaired, width=DEPTH * 0.7, color=colour)
    ax.axvline(DEPTH * E, color="k", ls=":", lw=1)
    ax.annotate(
        f"depth x E = {DEPTH * E}",
        (DEPTH * E, max(repaired) * 0.75),
        textcoords="offset points",
        xytext=(-96, 0),
    )
    ax.set_xlabel("contiguous burst (symbols)")
    ax.set_ylabel("symbol errors repaired")
    ax.set_title(
        f"RS(255,223) E={E}, interleaved {DEPTH} deep — "
        "green survives, red is refused"
    )
    ax.grid(True, axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig("ccsds_link_demo.png", dpi=110)
    plt.close(fig)
    print("\nwrote ccsds_link_demo.png")


if __name__ == "__main__":
    main()
