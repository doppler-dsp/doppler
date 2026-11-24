- **`AsyncDsssReceiver` hands over a fine carrier estimate**: the tone of
    its refine `Dll`'s squared partial dumps (`PolynomialPhaseEstimator`,
    `ho_src` 3) when it clears its check, else `CarrierAcquisition`'s
    estimate or the seed. At 415.1-B-1 its median error is 0.10 / 0.06 Hz
    at the floor (2.2 / 2.8 GHz), worst 1.68 Hz over 720 draws. The check,
    set for 10⁻³ on noise, runs 1.5 to 2 times over it: the interpolated
    peak searches more cells than its bound counts (#3293). State blob
    version 23
    ([async-dsss-receiver §4.11](docs/design/async-dsss-receiver.md)).
