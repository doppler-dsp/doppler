- **C: `Corr` and `Corr2D` are one object, `correlator`** (#2752 step 3.2;
    design #2863). Every `dp_corr_*` and `dp_corr2d_*` symbol is now
    `dp_correlator_*` (header `doppler/correlator/correlator_core.h`), and
    the constructors are `create_1d`, `create_2d` and `create_delay_doppler`.
    The state magics `CORR`, `CR2D` and `DDMP` give way to one, `CORL`, so an
    older blob is refused; `DET1`/`DET2` go to version 3 and `CAQR` to 2.
    Python is unchanged.
