- **The continuous DSSS receivers never drop a symbol.** A call whose
    symbols do not fit `out` holds the rest and delivers them first on the
    next call; a call takes its input only when they fit, and the new
    `consumed` property says whether it did. A call that completes a carried
    code period at several symbols a period lost all but one through the
    binding: 3,996 symbols one sample a call against 114,870 in one call on
    #2469's 127-chip, 250 ksym/s capture. Sized from `steps_max_out(x_len)`,
    as the binding is, nothing is held
    ([dsss_receiver_core.h](native/inc/doppler/dsss_receiver/dsss_receiver_core.h)).
