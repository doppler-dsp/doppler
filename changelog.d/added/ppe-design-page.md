- **A design page for the polynomial-phase estimator**
    (`docs/design/ppe.md`). `ppe` was certified without one: phase 1 of the
    lifecycle spine had been skipped, which the validation-report gate
    catches by requiring section 1 to *link* a design page rather than
    restate it. Four of the five reports it flagged had a page and simply
    did not cite it; `ppe` genuinely had none, and its reasoning existed
    only as header prose.

    The page is the argument, not the usage: why the search is
    two-dimensional and coherent (a pass ramps the frequency, and fitting a
    constant to a ramp smears the energy — noise is not what breaks it), why
    the transform is zero-padded 4× (parabolic interpolation is a better
    assumption on a finely sampled main lobe, which is a memory trade and
    was mis-documented as next-pow2), why the caller strips the modulation
    rather than the estimator (only the caller knows whether it squared the
    stream, and the M-th-power trick scales both estimates by M), and the
    measured envelope down to −10 dB input SNR. It degrades rather than
    breaks, which is the signature of a coherent search — the incoherent
    failure mode is picking a different peak, and there is no graceful
    version of that.
