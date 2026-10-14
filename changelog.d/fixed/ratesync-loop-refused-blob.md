- **`RateSync`'s and `MpskReceiver`'s loops write their own fields only
    after their children accept,** so a child's refusal leaves those fields
    unchanged (#2104); a carrier-filter refusal after `MpskReceiver`'s
    timing loop restored is #2104's remaining case.
