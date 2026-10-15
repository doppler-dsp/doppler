- **`RateSync`'s and `MpskReceiver`'s loops write their own fields only
    after their children accept,** so a child's refusal leaves those fields
    unchanged (#2104). One case remains: when `MpskReceiver`'s carrier filter
    refuses after its timing loop was restored, the receiver is left partly
    restored, so discard it after that `ValueError` and re-create it.
