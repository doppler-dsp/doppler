#!/usr/bin/env python3
"""wfm_field_demo.py -- bits, written as text: the Field grammar from Python.

Every place wfmgen takes a run of bits -- ``--data``, ``--sync``,
``--acq-code``, ``--data-code``, a scene's ``"spec"`` -- reads it with ONE C
parser, and ``field_bits()`` is that parser's Python door. This is the twin
of ``native/examples/wfm_field_demo.c``.

What it shows, in order:

1. A Field is text in, bits out, one ``uint8`` per bit.
2. Two spellings of one Field give the same bits: the radix and the
   defaults are spelling, not meaning.
3. A repetition is the same bits again, never a fresh draw -- which is what
   a receiver integrating over a repeated preamble relies on.
4. A Field outside the grammar RAISES, and nothing is built.

Every claim is an explicit check that exits non-zero on failure.

Run: python src/doppler/examples/wfm_field_demo.py
"""

import sys

import numpy as np

from doppler.wfm import field_bits


def main() -> int:
    # 1. Text in, bits out.
    b = field_bits("0xA5")
    print("0xA5 ->", b.tolist())  # [1, 0, 1, 0, 0, 1, 0, 1], MSB first
    if b.tolist() != [1, 0, 1, 0, 0, 1, 0, 1]:
        return 1

    # 2. Spelling is not meaning: hex numbers, default SEED/POLY/galois.
    same = np.array_equal(
        field_bits("pn:0x1f:5:0:0:galois"), field_bits("pn:31:5")
    )
    print("pn:0x1f:5:0:0:galois is pn:31:5:", same)
    if not same:
        return 1

    # 3. *REPS repeats the SAME period.
    one, four = field_bits("pn:31:5"), field_bits("pn:31:5*4")
    repeated = np.array_equal(four, np.tile(one, 4))
    print(f"pn:31:5*4 is {len(four)} bits, one period four times:", repeated)
    if not repeated:
        return 1

    # 4. Refused, never repaired.
    for bad in ("01a1", "pn:31:5:32", "pn:4000000000:5"):
        try:
            field_bits(bad)
        except (RuntimeError, ValueError):
            print(f"{bad!r} is refused")
        else:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
