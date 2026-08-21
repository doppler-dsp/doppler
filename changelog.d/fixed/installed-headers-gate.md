- **An installed header can no longer declare a C API the library does not
    define.** `native/inc/` is installed wholesale, so a header there is a
    **published C API** whether or not anything implements it — a downstream
    can read it, include it, and fail at link time, which is the worst
    possible first experience of a library.

    Two headers had done exactly that (`telemetry/tlm_recorder.h`, seven
    functions superseded by `dp_tlm_capture` and never built; an empty
    `stream/stream_core.h` scaffold) and were deleted earlier.
    `make installed-headers-check` is the durable half: every non-`static`,
    non-`inline` function declared at file scope in an installed header must
    resolve in `libdoppler.a` or the optional `libdoppler_stream.a`.

    **It found a third on arrival.** `ber_meter/ber_meter_core.h` still
    declared `theory_ser`, `theory_ber`, `esn0_db_for_ser`,
    `evm_scatter_floor_db`, `settle_syms` and `lock_symbol` — the
    pre-consolidation names, left behind when #539 moved those kernels to
    `ber_core.h` under a `ber_` prefix. None of the six existed under those
    names anywhere in the tree, and nothing in-tree called them, so the only
    thing they could do was compile at a downstream and fail to link. They
    were also six unprefixed global names in a public header.

    Absolute rather than a ratchet, and with no allowlist: the right count is
    zero, and a list would only be somewhere for a fourth to hide. A
    declaration with no definition is either implemented or its header stops
    being installed.
