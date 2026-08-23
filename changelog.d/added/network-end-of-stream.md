- **A sender can say it has finished, and a receiver hears it.**
    `dp_pub_send_eos()` publishes a zero-payload `DP_KIND_EOS` frame; a
    receiving `*_recv` reports the new `DP_ERR_EOF`, and on the Python face
    `recv()` raises **`EOFError`** — Python's own word for this, rather than
    a bespoke exception or a `RuntimeError`, because a producer finishing is
    the ordinary end of a loop and not a failure.

    Until now "no frame arrived" meant either "the sender is idle" or "the
    sender is gone", and nothing could tell them apart. A timeout answers
    only "nothing **yet**", which is exactly what a consumer cannot act on.

    A **kind**, not a flag bit, and the choice is forced rather than
    stylistic: a receiver refuses any `flags` bit outside `DP_FLAG_KNOWN`,
    because an unknown block moves where the payload starts — so a new flag
    would be rejected by every existing receiver, while a new kind follows
    the precedent `DP_KIND_TLM` set. One qualification, found by building
    it: doppler's validator refuses a frame whose element size is zero, and
    an ending has no elements, so a receiver built before this rejects the
    marker as `DP_ERR_INVALID` rather than ignoring it. That is the safe
    failure, and the validator now checks an EOS frame *differently* rather
    than not at all — one that claims a payload is still refused.

    **What it does not promise.** PUB/SUB is at-most-once, so the marker can
    be dropped like any other frame: it turns the common case from "wait
    forever" into "finish promptly", not from unreliable into guaranteed,
    and a subscriber that must not hang still needs a timeout. PUSH/PULL
    delivers it at-least-once, so handling must be idempotent. Ring and file
    are reliable, because there the marker is a flag or a fact rather than a
    message. `DP_ERR_EOF` means the same thing on all of them.
