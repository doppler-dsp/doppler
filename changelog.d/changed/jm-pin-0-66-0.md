- **just-makeit pin 0.63.3 → 0.66.0, and six header/manifest constructor
    mismatches are resolved.** 0.64.x added a **CTOR** check comparing the
    `create()` declaration jm injects into a sacred header against the one the
    manifest describes. It found six that `drift-check` at 0.63.3 exits 0 on —
    none of them a live defect, all of them a declaration jm could not have
    regenerated.

    Four were ours to fix. `hbdecim_q15_create(size_t num_taps, const float *h)`
    takes its length as a named scalar *before* the pointer, which is gh-900's
    `derived` key — a manifest line, not a C change, and the unfixed twin of
    the `HalfbandDecimator_create()` argument-order fix in 0.43.0. `corr_create`,
    `fir_create` and `detector_create` named their length `n`/`num_taps` where
    the manifest derives `ref_len`/`taps_len`; parameter names are
    documentation, not ABI, so the headers moved.

    Two needed jm, and both shipped in 0.65.0:

    - **`c_type` on an init-param** ([jm#1096](https://github.com/just-buildit/just-makeit/issues/1096)).
        `detector`/`detector2d` take `det_noise_mode_t noise_mode`; a
        `string_enum:` renders `int`. `c_type = "det_noise_mode_t"` moves only
        the injected declaration — the binding still parses the choice string
        and passes an index, which C converts at the call, so
        `Detector(noise_mode="median")` is unchanged. The alternative was
        weakening two public headers to `int` to satisfy a text comparison,
        which would not even have unblocked the bump: `detector2d` needed the
        second fix as well.
    - **`derived = ["ny", "nx"]`** ([jm#1097](https://github.com/just-buildit/just-makeit/issues/1097)),
        and a doppler under-declaration it exposed. `corr2d`/`detector2d` take
        a 2-D reference, and **the manifest declared it 1-D**
        (`float _Complex[]`), so jm derived a single `ref_len` against a C
        taking `(ref, ny, nx)` — which is why the two could not be reconciled
        at all. Both are `float _Complex[][]` now, the spelling that makes jm
        model the array as 2-D; jm already passed both extents and enforced
        `ndim == 2`, they simply could not be *named*, which is what gh-1097
        adds. Codegen delta: zero — the injected declaration now matches C the
        hand-owned bindings were already calling correctly.

    Also carried since 0.63.3: gh-1079's `out=` buffer for an all-scalar
    `variable_output` method (eight bindings, and `DelayCf64.push_ptr` off the
    `kwarg-parity` ratchet), and gh-1026's enum refusals naming their choices.

    0.66.0 additionally refuses an `init_param` default that is not a literal
    for its type, and found one: `detector`/`detector2d`'s `noise_hi` carried
    `default = "n-1"` / `"ny*nx-1"` beside a `default_raw` sentinel. Those
    expressions described the EFFECT — the C clamps anything at or beyond the
    window to the last bin — not a value, and jm renders a default into C, the
    `.pyi` and an app's flags, where an expression is valid in none. The
    non-literal is gone; the clamping behaviour now lives on the header's
    `@param`, which is where it should have been, since the `.pyi` had been
    publishing `default n-1` with no explanation of what selected it.
