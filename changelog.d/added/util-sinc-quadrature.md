- **`doppler.util` gains `sinc`, `mean_sinc`, `complement_power` and the
    quadrature rules `simpson_weights`, `midpoint_nodes`, `gauss_hermite`.**
    One definition each, in C and Python; `acq`, `carrier_acq`,
    `design_lowpass` and `wfm`'s raised-cosine pulse now call them instead
    of private copies.
