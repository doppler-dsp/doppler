- **The `MpskReceiver` report's "not covered" note no longer reads as if the tree
    lacks framing.** It said *"FER is absent because this object has no framing"*,
    which was true of the object and became misleading once `wfm.Frame`,
    `ccsds_tm_frame.h` and the CCSDS chain landed a layer up — and
    `native/validation/rx_frame_fer.c` already measures FER on a receiver through
    them. So `rx-test.md` goal 4's fourth metric is reachable; it is just not
    reachable from a report scoped to one object, which is a different statement.

    The note now also records a composition gap worth knowing about:
    `mpsk_soft_demap` produces per-bit LLRs and **`MpskReceiver` exposes none**,
    so a caller feeding a soft-decision decoder demaps from `steps()` output
    themselves rather than asking the receiver for it. The receiver's claim
    inventory is unaffected — the LLRs came out of the `mpsk` constellation
    module, not the receiver, so no header claim changed.
