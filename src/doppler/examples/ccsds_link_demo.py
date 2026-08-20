"""ccsds_link_demo.py — a CCSDS CADU, end to end, as a DESCRIPTION.

A frame is a list of **fields** that appear on the wire and a list of
**stages** that transform them, each stage carrying the span it covers. This
demo builds one, damages it, and scores it — using nothing CCSDS-specific
except the numbers CCSDS picked.

The point is the coverage asymmetry. The four stages do **not** all cover the
same bits:

    stage                  covers the marker?   131.0-B-3 section
    Reed-Solomon (outer)   no                   9.5.1, 9.2.1.5
    pseudo-randomiser      no                   10.3.2, 10.3.4 note 1
    convolutional (inner)  YES                  3.2.1, 9.2.1.4

Those sections are numbered as B-3 has them, the issue this was written
against. 131.0-B-6 is current and moved the numbers without changing what
they say (gh-865).

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

from doppler.wfm import FrameDesc, ccsds_asm_bits

# The four numbers CCSDS 131.0-B-3 section 4.3 picks. The marker 9.4.1 picks
# comes from `ccsds_asm_bits()` rather than from a constant expanded here:
# an MSB-first transcription written out twice is one that can disagree with
# itself. Everything else in this file is general.
K, N, E = 223, 255, 16  # RS(255,223), 16 correctable symbols
DEPTH = 5  # interleaving depth (4.3.5.1 allows 1,2,3,4,5,8)

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
    marker = ccsds_asm_bits()  # 0x1ACFFC1D, figure 9-1

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
    #
    # Swept one symbol at a time THROUGH the boundary, because the interesting
    # behaviour is not the cliff but the shoulder: 80 symbols is exactly E in
    # each of the five codewords, and every symbol after that pushes one more
    # codeword past its radius. A coarse sweep in steps of `depth` jumps from
    # "all five fine" to "all five refused" and hides the graded failure that
    # makes the counts worth reporting at all.
    bursts, repaired, good, survived = [], [], [], []
    for b in range(0, DEPTH * E + 2 * DEPTH + 1):
        rx = clean.copy()
        for s in range(b):
            rx[blk + s * 8] ^= 1  # one symbol each
        r = cadu.check(rx)
        bursts.append(b)
        repaired.append(r.symbols)
        good.append(r.ok)
        survived.append(bool(r.passed))

    edge = max(b for b, ok in zip(bursts, survived) if ok)
    print(f"longest burst held : {edge} symbols  (depth {DEPTH} x E {E})")
    assert edge == DEPTH * E, (
        f"interleaving depth {DEPTH} must carry a {DEPTH * E}-symbol burst, "
        f"held {edge}"
    )
    assert not survived[edge + 1], "one symbol more must fail"

    # The shoulder, as a claim rather than a picture: each extra symbol past
    # the boundary costs exactly one more codeword, because a contiguous burst
    # visits the codewords in rotation.
    print("past the boundary, one symbol at a time:")
    for k in range(0, DEPTH + 1):
        b = edge + k
        verdict = "ok " if survived[b] else "REFUSED"
        print(
            f"  {b:>3} symbols  {good[b]}/{DEPTH + 1} units good  "
            f"{repaired[b]:>3} repaired  {verdict}"
        )
    for k in range(1, DEPTH + 1):
        assert good[edge + k] == (DEPTH - k) + 1, (
            f"{k} symbols past the boundary must leave {DEPTH - k} codewords "
            f"good (plus the randomiser), got {good[edge + k]}"
        )

    # ── 3. the plot ──────────────────────────────────────────────────────
    fig, (ax0, ax1) = plt.subplots(
        2,
        1,
        figsize=(6.8, 5.4),
        sharex=True,
        gridspec_kw={"height_ratios": [2, 1]},
    )
    colour = ["#2b8a3e" if ok else "#c92a2a" for ok in survived]
    ax0.bar(bursts, repaired, width=1.0, color=colour)
    ax0.axvline(DEPTH * E, color="k", ls=":", lw=1)
    ax0.annotate(
        f"depth x E = {DEPTH * E}",
        (DEPTH * E, max(repaired) * 0.55),
        textcoords="offset points",
        xytext=(-104, 0),
    )
    ax0.set_ylabel("symbol errors repaired")
    ax0.set_title(
        f"RS(255,223) E={E}, interleaved {DEPTH} deep — "
        "a burst costs, then it kills"
    )
    ax0.grid(True, axis="y", alpha=0.3)

    # The second panel is why the first one alone would mislead: past the
    # boundary the repair count falls to ZERO, because a refused codeword is
    # not repaired at all. Height 0 there means "gave up", not "nothing to do".
    ax1.step(bursts, good, where="mid", color="#1c7ed6", lw=1.6)
    ax1.axvline(DEPTH * E, color="k", ls=":", lw=1)
    ax1.axhline(DEPTH + 1, color="#2b8a3e", ls="--", lw=1)
    ax1.set_ylim(0, DEPTH + 1.6)
    ax1.set_xlabel("contiguous burst (symbols)")
    ax1.set_ylabel("units good")
    ax1.grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig("ccsds_link_demo.png", dpi=110)
    plt.close(fig)
    print("\nwrote ccsds_link_demo.png")


if __name__ == "__main__":
    main()
