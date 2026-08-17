- **`carrier_acq`'s state blob carried seven undefined bytes.**
    `carrier_acq_extra_t` puts a `uint8_t ready` in front of a `double`, and
    `get_state` writes the struct whole — so the seven bytes of padding the
    compiler inserts went into the blob holding whatever the stack last left
    there. A designated initializer zeroes the members it does not mention;
    padding keeps *unspecified* values (C11 6.7.9p10), and compilers differ on
    whether they zero it anyway.

    Found by this branch's fidelity check the moment it existed, and found the
    way this class always is: **green on macOS and on gcc 15, red on both Linux
    runners**, because whether two blobs of the same state compare equal
    depended on the compiler rather than on the object. Both DSSS receivers
    already declare an explicit `_pad[7]` for this reason; `carrier_acq` did
    not.

    The member is now explicit, which is a **format-preserving** fix — those
    seven bytes are the same seven the compiler was already inserting, at the
    same offsets, so no blob changes size or layout. They are merely defined.
