- **13 more bindings are jm-generated** (acq, ber_meter, async_dsss_receiver,
    burst_acq, burst_capture, corr2d, detector, detector2d, dll, fir, frame,
    mpsk_receiver, psd; doppler#1446). A jm fix now reaches their fragments
    and a hand edit turns `make drift-check` red. Two visible effects: a
    no-argument method such as `BerMeter.ber()` rejects stray positional
    arguments instead of ignoring them, and `BpskReceiver` / `MpskReceiverR`
    raise the create error their manifest declares, not a generic
    `MpskReceiver` message. The `-Wall -Wextra` exempt list goes 33 -> 15.
