- **The streaming wire format is v2: a 64-byte header, `DPSTREAM`, and BLUE
    format codes.** Both ends of a doppler stream must move together; the two
    formats are mutually unrecognisable at byte 0, which is the cleanest
    break available and better than a version field nothing read.

    What changed, and why each one:

    - **Magic is `DPSTREAM`, a `uint64_t`, replacing 4-byte `"SIGS"`.** An
        integer rather than a `char[8]` deliberately: the header is host byte
        order with no conversion, so a peer of the opposite endianness reads
        it byte-swapped and it stops matching. The format tag is the
        endianness probe too, for free.
    - **64 bytes, down from 96, with nothing dead in them.** `protocol` and
        `stream_id` were 8 bytes of DIFI provision nothing ever set, and
        `reserved[4]` was 32 more that every unchunked frame carried zeroed —
        while secretly being the chunk geometry. `version` was written as
        `0x00010000` and documented as `1`, and no receiver read it at all.
    - **`payload_bytes`, and a receiver that checks it.** v1 validated the
        magic and nothing else, so a header claiming more samples than its
        message carried produced an out-of-bounds read on both faces (the
        numpy array was sized from the header's own claim over a buffer
        nobody measured). A v2 receiver checks magic, version, unknown flag
        bits, the header's length claim against the transport's, and
        `num_samples × element size` against the same.
    - **Formats are their BLUE codes** (`CF64` is `"CD"`, `CI16` is `"CI"`),
        which collapses three disagreeing enumerations of the same five types
        — the stream's, `wfm_writer`'s "wavegen order" `stype`, and
        `wfm_sink.c`'s `WT_*` — plus a `FMTCH[]` translation table and a
        `BPS[]` size table. They now live in `native/inc/dp_format.h`, all
        `static inline`, so the file writer names a format without linking
        the transport.
    - **`TLM16` is a frame KIND, not a sample format.** Telemetry is not a
        sample encoding: it has no BLUE code, `num_samples` counts records,
        and only a Publisher emits it. `kind` says so and `format` stays a
        pure BLUE code. C gains `dp_pub_create_tlm()`; Python keeps
        `Publisher(ep, TLM16)`.
    - **The chunk geometry is a 24-byte block that rides only chunked
        frames**, and both it and the flag set are now public in `stream.h` —
        so the format can be implemented from the header doppler publishes,
        which it could not before.
    - **An unknown flag bit is rejected**, not ignored. That is what makes a
        later additive block safe inside major version 2.

    `header` dicts from `recv()` gain `kind`, `flags`, `payload_bytes` and
    `version`, and `sample_type` becomes `format`.

    The layout is now pinned by `_Static_assert`s on the size and every
    offset, and by `native/tests/test_stream_wire.c` — which needs no broker,
    so unlike the round-trip tests it runs everywhere. Sabotage-checked:
    swapping two fields fails the build with "version at 16".
