- **`HalfbandDecimator` is jm-generated** (doppler#1446). It was held on
    just-makeit#2004 (a constructor array could not declare `rank`); with
    `rank = 1` on `h` the render keeps the 1-D guard, so a 2-D `h` still raises
    `ValueError` (the message now reads `h must be a 1-D array`). The binding is
    re-rendered by jm, leaves the `-Wall -Wextra` exempt list, and returns a
    fresh output array per call instead of one it owns. The generated C
    symbol test gains the state triplet rows.
