- **A design page for the polynomial-phase estimator**
    (`docs/design/ppe.md`). `ppe` had been certified with phase 1 of the
    lifecycle spine skipped — its reasoning existed only as header prose —
    which the validation-report gate catches by requiring section 1 to *link*
    a design page rather than restate it. The page is the argument: why the
    search is two-dimensional and coherent, why the transform is zero-padded
    4x, why the *caller* strips the modulation, and the measured envelope down
    to −10 dB input SNR.
