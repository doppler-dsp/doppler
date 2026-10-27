- **Reader-facing text and comments say "the specification", not a bare
    "SPEC".** The async receiver's validation report and its gallery titles
    and links named a term no reader could find (#2626, #2629); the report and
    its plots are regenerated. The comment sweep behind #2590 left 113 more
    mentions in 28 files, and removed-machinery references in
    `rx_dynamics.c`, `carrier_nda_core.c` and `mpsk_ber_common.h`. Those are
    comment and printout text only: the preprocessed C and the Python ASTs
    are unchanged, apart from three printouts and one docstring (#2621).
