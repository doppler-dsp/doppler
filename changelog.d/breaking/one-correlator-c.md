- **C: `Corr` and `Corr2D` are one object, `correlator`** (#2752 step 3.2;
    design #2863). Every `dp_corr_*` and `dp_corr2d_*` symbol is now
    `dp_correlator_*` (header `doppler/correlator/correlator_core.h`), with
    constructors `create_1d`, `create_2d` and `create_delay_doppler`. One
    state magic, `CORL`, replaces `CORR`, `CR2D` and `DDMP`, so an older blob
    is refused; `DET1`/`DET2` go to version 3 and `CAQR` to 2. Python's
    signatures are unchanged, but `Corr.execute`, `reset` and
    `execute_max_out` now carry the shared 2-D docstrings, and a wrong-length
    frame's `ValueError` says `(n: Corr's n, or ny*nx)`, not `(Corr.n)`.
