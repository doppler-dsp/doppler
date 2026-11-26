- **`SymbolTimingEstimator`: symbol timing from one block, with no loop.**
    A feedforward estimate (Oerder and Meyr) of where the eye center sits,
    the symbol rate's error between two blocks and a quality figure, for
    any real number of samples per symbol above 2 and with a carrier
    offset present. Acquisition then takes the block's length whatever
    the starting timing. See
    [the design page](docs/design/timing-estimator.md).
