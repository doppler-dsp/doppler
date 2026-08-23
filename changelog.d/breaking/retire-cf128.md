- **`CF128` is removed from the streaming wire protocol.** Its size was
    `sizeof(long double _Complex)` — 32 bytes on both x86-64 and aarch64 —
    while the *representation* differed: 80-bit extended on x86-64
    (`LDBL_MANT_DIG` 64), IEEE binary128 on aarch64 (`LDBL_MANT_DIG` 113).
    Every field a receiver checks agreed, so nothing rejected the frame and
    the samples decoded to nonsense. doppler publishes multi-arch images, so
    the two ends of such a stream genuinely can differ. A type that cannot
    cross the architectures the project ships is not a wire type.

    Gone: the `CF128` constant on both faces, `dp_pub_send_cf128` /
    `dp_push_send_cf128` / `dp_req_send_cf128` / `dp_rep_send_cf128`, and the
    `numpy.clongdouble` paths. **Wire value 2 is retired, not reused** — an
    older sender's frames must stay unrecognised rather than be read as some
    newer type. Use `CF64`, which is what every doppler example already sent.

    The removal forced a second fix. A retired value sits *inside* the enum's
    numeric range, so the ordinal guard the senders ran (`type < CI32 ||   type > CF32`) accepted it and would have built zero-length frames. There
    are now `dp_sample_type_is_valid()` and `dp_sample_type_is_iq()`,
    **derived from `dp_sample_size()`** so one table decides what a type is:
    a type with no size is not a type. Sabotage-checked — restoring the
    ordinal guard makes the new reject test fail.

    Three stale claims died with it: the stub, the stream test module and a
    `wfm` test each said the Python receiver decodes "only CI32/CF64/CF128",
    which stopped being true several releases ago (a CF32 round-trip test sat
    eight tests below one of those docstrings). A gallery note and an example
    docstring described the same closed gap. All five now state the real set.
