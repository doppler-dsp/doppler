- **A frame can be described from Python, CCSDS included.**
    `doppler.wfm.FrameDesc` is `Frame`'s deferred flavor: the same
    constructor arguments, but it stops before materialising, so the four
    fields `wfm_frame_t` names are a starting point a caller extends with
    `add_field` / `add_stage` before `build()`. Empty arrays for all three
    begin from nothing.

    That is what makes the CCSDS coding reachable from Python at all.
    `ccsds_tm` has no binding and is not getting one, so a caller meets the
    outer code, the randomiser and the inner code by **describing** a CADU —
    three fields and three covers — rather than through a CCSDS entry point
    bolted onto this object. The covers are 131.0-B-3's coverage table: the
    inner code reaches over the marker and neither of the other two does.

    `Frame` is unchanged and is now visibly one configuration of the general
    description: `n_fields()`, `n_stages()`, `field_off()`, `field_bits()`,
    `stage_first()` and `stage_bits()` read it too, and agree with the named
    `layout()` field for field.

    A view rather than a second type, by the rule the `ddc` module already
    follows — a difference in CONSTRUCTOR is a flavor; a difference in METHOD
    SIGNATURE is a separate type. Every method is shared verbatim.

    Two carve-outs, both filed. `add_field`/`add_stage` take `kind` as an int
    rather than one of the enum's names, because a method parameter cannot yet
    be a string enum
    ([just-makeit#1021](https://github.com/just-buildit/just-makeit/issues/1021)
    — the `enum` key is accepted on a method parameter and silently ignored,
    which is the half worth fixing). And `layout()`'s named view reports
    nothing for a description, on purpose: it would go stale the moment a
    fifth field is appended, and a stale offset is worse than an absent one.
