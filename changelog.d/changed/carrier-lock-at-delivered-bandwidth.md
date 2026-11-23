- **`AsyncDsssReceiver`'s carrier lock is sized at the bandwidth its
    carrier loop delivers.** That bandwidth is wider than the design rule's
    by the loop's detector delay, `ASYNC_DSSS_RX_CAR_LOOP_DELAY(segments)`
    (`2 - 1/segments`, a worst case: 1.75 at 4 segments). At the
    415.1-B-1 floor `carrier_lock_n_noncoh` is now 34 at 2.2 GHz (it was
    33\) and stays 43 at 2.8 GHz, so a lock decision at 2.2 GHz sums one
    more code period. The C function
    `dp_async_dsss_receiver_carrier_lock_n_noncoh()` takes the receiver's
    `segments`, and `dp_async_dsss_receiver_carrier_phase_var()` gives the
    variance it sizes at (#3285 designs the loop to deliver the rule's
    bandwidth instead).
