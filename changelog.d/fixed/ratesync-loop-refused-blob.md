- **`RateSync`'s timing loop and `MpskReceiver`'s loops are left as they were
    when they refuse a state blob.** Each wrote its own fields before a
    child could refuse, so a refused restore left them holding the blob's
    values. The fields are now written only once the children have accepted
    (#2104).
