- **C: `Corr` and `Corr2D` are one object, `correlator`** (#2752 step 3.2;
    its design, #2863). Every `dp_corr_*` and
    `dp_corr2d_*` symbol is now `dp_correlator_*`, with no shims, and the
    header is `doppler/correlator/correlator_core.h`. The constructors are
    the flavors:

    - `dp_corr_create` is `dp_correlator_create_1d`, which is the 2-D
        correlator at one row, bit for bit;
    - `dp_corr2d_create` is `dp_correlator_create_2d`;
    - `dp_corr2d_create_delay_doppler` is
        `dp_correlator_create_delay_doppler`.

    `dp_correlator_set_ref` returns `int` for every flavor (the 1-D one
    returned `void`). The three state magics `CORR`, `CR2D` and `DDMP` are
    retired for one, `CORL`, whose first field is the flavor, a reject key; a
    blob saved by an earlier release is refused, never misread. The blobs
    that nest one bump: `CorrDetector` (`DET1`) and `CorrDetector2D` (`DET2`)
    to version 3, and `CarrierAcquisition` (`CAQR`) to version 2.

    Python is unchanged: `Corr`, `Corr2D` and `DelayDoppler` keep their
    names, signatures and members, as views of the one object.
