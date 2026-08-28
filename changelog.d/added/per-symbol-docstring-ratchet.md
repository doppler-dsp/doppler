- **The docstring ratchet now fails per SYMBOL, not just per module.** A
    symbol recorded as documented may never stop being documented, however
    much improved elsewhere; the per-module incomplete count stays, as what
    catches *new* undocumented surface. Adopting just-makeit 0.70.0,
    `doppler.track` read 1 → 1 while `BpskReceiver` gained a description and
    `MpskReceiverR` lost one — a count cannot represent a pair that cancels.
    A face the build never exposed is a third state (`?`), so an unbuilt
    module reads unscored rather than regressed. See
    `scripts/check_docstring_coverage.py`'s `face_flags`.
