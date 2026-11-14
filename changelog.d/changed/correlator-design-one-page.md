- **The correlator's design is one page,
    [`docs/design/correlator.md`](docs/design/correlator.md),** describing the
    object as it is: its seven flavors, the one constructor, which flavors
    serve each member, the state blob, the map (padded inverse,
    delay-Doppler, batches), detection, and a new section on cost and
    parallelism with its runs in `correlator-measurements.md` (#3047). The
    step-3 and step-4 plans and the two `corr2d-*` pages it absorbs are
    archived; code comments and `test_correlator.py`'s table reader point at
    the new sections (#3051).
