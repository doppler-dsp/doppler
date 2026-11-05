# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# pn_core and cvt_core: the bench's short rows (#2752 step 3.1b) correlate an
# m-sequence from the shipped generator, mapped by the shipped NRZ mapper.
# Linked onto the bench alone: a `depends_on` entry would put them on
# corr_core's own link line.
target_link_libraries(bench_corr_core PRIVATE pn_core cvt_core)
