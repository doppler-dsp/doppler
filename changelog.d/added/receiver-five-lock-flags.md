- **AsyncDsssReceiver's status record publishes all five lock flags.** Beside
    `locked` and `code_locked`, `status()` (and `AsyncDsssPool.status()`)
    now carries `carrier_locked`, `demod_carrier_locked` and `sync_locked`,
    so a downstream program opens the error-rate window the certification
    record scores in without reading private state. The window itself is a
    public function, `dp_ber_lock_all()` in `doppler/ber/ber_core.h`: the
    latest of N flags sustained. `MpskReceiver` gains `sync_locked`, its
    timing lock decision beside `locked`
    ([#3350](https://github.com/doppler-dsp/doppler/issues/3350)).
