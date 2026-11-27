- **A CIC starts from silence.** Its first outputs after create or reset
    read near negative full scale, for any input, silence included: the
    offset that makes its samples non-negative was carried through the
    filter. It is now taken off before the filter and put back after it,
    still with no signed register. Outputs past the first three are bit for
    bit what they were; a NaN sample is taken as over the bound and raises
    `clipped`. The state blob is version 4.
