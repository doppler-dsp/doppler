- **A forged blob is refused, not written.** The state envelope checks a
    blob's size, magic, version and endianness, never a payload field, so a
    blob whose index was past its ring (boxcar's `pos`, a dual-write ring's
    `head`) passed the envelope and the next call wrote past the buffer.
    Those objects now decode into a temporary, check it against the
    predicate the object defines, and commit only when it holds. A blob that
    fails returns `DP_ERR_INVALID` and leaves the object unchanged (#2142).

    The check accepts the zero state: a calloc'd embedding's `tsamps`,
    `avgs` and `rng` are 0 and that is a fresh object, not a forgery.
