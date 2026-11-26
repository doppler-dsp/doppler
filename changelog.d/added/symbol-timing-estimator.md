- **`SymbolTimingEstimator`: symbol timing from one block, with no loop.**
    A feedforward estimate (Oerder and Meyr) of where the eye center sits,
    the symbol rate's error between two blocks and a quality figure, for
    any real number of samples per symbol above 2 and with a carrier
    offset present. Acquisition then takes the block's length whatever
    the starting timing. See
    [the guide](docs/guide/tracking-loops.md#symboltimingestimator-timing-from-one-block-with-no-loop),
    which has the numbers to size a block by.
