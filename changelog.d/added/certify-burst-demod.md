- **`BurstDemod` is certified** —
    `src/doppler/dsss/tests/validation/burst_demod/results.md`, 15 limits on
    every push, plus three C sections. The gaps clustered on the read-backs,
    which are the part a caller actually consumes.

    **`frame_valid`'s negative case was the too-short path only.** It was
    asserted 1 on a clean burst and 0 on an **8-sample** input — but that
    returns before a CRC is ever computed. A `frame_valid` that ignored the
    trailer entirely and reported 1 for anything it decoded would have
    passed both assertions, on an object whose entire purpose is to tell a
    caller whether the payload can be trusted. Now measured on frames that
    **arrive intact and fail their own CRC** — a payload bit flipped after
    the trailer was computed, at both ends and the middle — with the bits
    still returned, which is what separates the CRC path from the too-short
    one. Sabotage-proven by forcing `frame_valid = 1` whenever a frame
    decodes.

    **`frame_offset` was only ever observed as 0** — its degenerate value,
    and exactly what a read-back hardwired to zero reports. Now measured
    against bursts carrying 0, 3 and 9 filler symbols before the sync, where
    it must equal the filler count.

    **`n_symbols` had no mention in either language**, and it does not mean
    what a reader might assume: it counts the whole despread data section —
    filler, sync, payload *and* trailer — not the payload. That distinction
    is invisible until the sync is away from offset zero, which is why it
    needed the same stimulus as `frame_offset`.

    **`reset()` was called by nothing in either language.** The read-backs
    are this object's whole output surface, so a reset that left them
    standing would report the *previous* burst's verdict for a burst that
    had not been demodulated — silently, and in the direction that matters
    (a stale `frame_valid = 1`). Checked with the precondition asserted
    first, so it cannot pass against state that was already clear.

    One sabotage failure worth recording, because it reads exactly like a
    test that cannot fail: the first attempt at hardwiring `frame_offset`
    matched the **initialiser** rather than the assignment, so the patch
    replaced `= 0` with `= 0` and the suite stayed green. A no-op sabotage
    and an untestable claim are indistinguishable from the outside — the
    patch has to be read, not just applied.
