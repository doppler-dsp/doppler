- **just-makeit pin 0.62.0 → 0.62.1**, for a fix this repo drove and could not
    work around: **just-makeit#1018**. A gh-1012 signature override's `.pyi`
    documented the PARENT's C symbol while the runtime face documented the
    override's, so `MpskReceiverR.steps` — whose whole point is that it takes
    `float32` where its parent takes `complex64` — was stubbed with a doctest
    that constructs `MpskReceiver` and hands it a complex array.

    `scripts/check_doc_face_parity.py` refused it, correctly, and **no manifest
    or header configuration made both faces right**: removing the override's
    `@code` only swapped the runtime face onto jm's synthesized example while
    the stub kept the parent's authored one. That is what made it an upstream
    blocker rather than a local carve-out, and why the collapse waited on a jm
    release instead of on an exemption.

    The fix keys a member's doc block on the symbol it BINDS
    (`C.method_c_symbol`) rather than on `<component>_<member>`, and judges the
    scaffold sentinel against the member NAME — jm writes its skeleton
    `@brief` from the Python name while the parser recognises a scaffold by the
    name derived from the C symbol, the same string for every method until
    `fn` made them differ.

    Adopting it reconciled two `.pyi` files and eight sacred fragments, with no
    `_core` and no signature drift. `Doc face parity` reports 213 methods compared and 0 divergent.
