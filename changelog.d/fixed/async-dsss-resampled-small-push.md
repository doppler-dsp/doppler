- **`AsyncDsssReceiver` decoded differently depending on how its input was
    split into calls** (#2974): BER up to 0.13 at 1-5 samples a call
    (0.109, 0.044, 0.109, 0.129, 0.025) against 0 fed whole, at 2.5
    samples a chip. The refine decides past the live chain's start by its
    chain's lag, and the start was taken only from the deciding call. The
    receiver now keeps a ring of its last refine samples, sized at create
    from the new `dp_RateConverter_get_delay()`, and waits for a start not
    yet reached. Output fed whole is unchanged.
