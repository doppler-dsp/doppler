- **The retired hand-off flavour still named a live mode.** #1295 deleted
    `HandoffAsyncDsssReceiver`, but the mode it named survives as the cell
    receiver, and 120 sites still called it "hand-off mode" — 15 in the
    receiver header, which the validation process treats as the spec, and
    from there into the Python docstrings on both faces. Two validators named
    the deleted constructor in a file header while their bodies called the
    live one; the pool header's `@brief` contradicted itself eleven lines
    later; a header cited `async_dsss_pool_create_cell()`, a symbol that does
    not exist; and the docstring-coverage baseline still listed 35 methods of
    the deleted class. [#1295](https://github.com/doppler-dsp/doppler/issues/1295).
