- **A forged blob is refused, not written.** The state envelope checks a
    blob's size, magic, version and endianness, never a payload field, so a
    blob whose index was past its ring (boxcar's `pos`, a dual-write ring's
    `head`) passed the envelope and the next call wrote past the buffer.
    Those objects now decode into a temporary, check it against the
    predicate the object defines, and commit only when it holds. A refused
    blob returns `DP_ERR_INVALID` and leaves that object unchanged (#2142).
    The objects are boxcar, delay, wfm_synth, hbdecim, hbdecim_q15,
    hbdecim_r2c, lockdet, farrow, symsync and adc.

    "Unchanged" covers the leaf object. A composition that restores a child
    before it checks the parent can still leave the child changed:
    RateConverter discards hbdecim's refusal and returns `DP_OK` (#2138),
    and costas's refusal fires mid-restore in dsss_receiver and the async
    path (#2104).

    The check accepts the zero state. A calloc'd embedding's `tsamps`,
    `avgs` and `rng` are 0 and that is a fresh object, not a forgery; the
    one exception is symsync `avgs = 0` and adc `rng = 0`, which init never
    produces and which are refused.

    Behaviour changes:

    - `dp_farrow_create` and `dp_symsync_create` return NULL for an order
        outside 0..2. `create(3)` used to build a cubic interpolator.
    - `dp_wfm_synth_set_nsps` and `dp_wfm_synth_set_sym_pos` return
        `DP_ERR_INVALID` instead of writing an out-of-range value, and the
        Python `set_nsps` and `set_sym_pos` raise `ValueError`. A shrinking
        `nsps` needs `sym_pos` set to 0 first.
