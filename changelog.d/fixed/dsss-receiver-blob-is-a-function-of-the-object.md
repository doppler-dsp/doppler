- **The DSSS receivers' state blobs are a function of the object.**
    `DsssReceiver` and `AsyncDsssReceiver` wrote their whole carrier-carry
    buffer into `get_state`, though only its first `car_carry_len` samples
    are state, so two identical receivers gave different blobs from whatever
    the buffer held before (the #2076 defect, in two more places). They now
    write zeros past the carry, through a new `dp_w_zeros` that the framer's
    pad uses too, and `CarrierAcquisition.set_state` refuses a carry of a
    full `n` samples, which `steps` never leaves.
