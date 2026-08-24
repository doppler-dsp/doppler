- **The stimulus gate's `evm` marker was defeated by a one-word prefix.** It
    matched `def evm(`, `def _evm(`, `def evm_db(` and `def _evm_db(` — so
    `self_evm_db()`, sitting in `async_dsss_receiver_spec_demo.py` as a
    line-for-line twin of `ber.ber_evm_db` (self-referenced, rotation from the
    data, no truth and no lag — and returning the identical −10.1 dB), was
    invisible. The gate reported **`evm 0`** while that function was in the
    tree, which is the exact failure `check_stimulus_sources.py`'s own header
    is written about: a check that reads as coverage and looks at nothing.

    The pattern now allows any prefix (`\w*evm(_db)?`). Sabotage-proven —
    re-adding a `self_evm_db()` fires it, and removing it goes green.

    Widening it surfaced four more names, and **none of them turned out to be
    a private copy**, which is worth stating because it is the reason the
    ratchet grew by four without any debt being added:

    - `panel_evm()` plots values measured elsewhere and `clamp_evm_db()` floors
        one that has already been measured — neither computes an EVM.
    - `_best_evm_db()` / `_sweep_evm_db()` are **truth-referenced and minimised
        over strobe alignment**, which is precisely what `ber_evm_db` is built to
        refuse ("cannot be fooled by an alignment search"). Open loop the
        cascade's strobe phase is arbitrary, so minimising is what isolates the
        matched filter from a timing loop that does not exist yet. That is the
        opposite measurement, not a copy of this one.

    All four are allowlisted under a new **SAFE** heading with those reasons,
    keeping the "KNOWN VIOLATIONS" section — the part that may only shrink —
    unchanged at one entry.
