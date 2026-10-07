- **`HalfbandDecimatorQ15` is jm-generated** (doppler#1446). It waited on
    just-makeit#1996 (`elements_per_sample` was ignored on a `variable_output`
    method), fixed in jm 0.99.0. With `pass_capacity` and
    `elements_per_sample = 2` on `x`, jm does the samples-to-elements
    conversion the hand fragment did (`/ 2` on the input and capacity, `* 2` on
    the output), so a re-render cannot drift from it. The binding leaves the
    `-Wall -Wextra` exempt list and returns a fresh array per call.
