- **`DelayDoppler` and the Corr2D hypothesis bank run on the library's one
    delay-Doppler map.** The map computes the delay-Doppler surface in the
    order with fewer transforms: rows first, with each row inverted as it is
    accumulated, or slow time first, shared across a fraction's carriers.
    The maps are unchanged to float rounding. The bank without rate
    hypotheses runs about 20% faster on the dev box, and every other row
    ties (#2831, #2752).
