- **The DSSS receivers' state blobs stop carrying bytes that are not
    state.** `DsssReceiver` and `AsyncDsssReceiver` wrote their whole
    carrier-carry buffer, though only its first `car_carry_len` samples are
    state. A Costas loop built on the stack carried its 4-byte struct hole,
    and hbdecim kept a consumed sample in `pending`. Two identical
    receivers could therefore give different blobs. Each is now written as
    zeros, through one new `dp_w_zeros` that every zero-pad uses, and the
    receivers and `CarrierAcquisition` refuse a blob claiming a full carry,
    which `steps` never leaves.
