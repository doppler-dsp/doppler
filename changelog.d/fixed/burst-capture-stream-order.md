- **`BurstCapture` and `DsssBurstReceiver` return the same bursts at any
    push block size, however closely the bursts are packed**
    ([#1527](https://github.com/doppler-dsp/doppler/issues/1527)).
    Four bursts at a quarter of `min_gap` came back as one wrong window when
    pushed whole, and as four exact bursts in 1000-sample blocks. A
    detection now joins a pending burst only if that burst's window had not
    yet arrived when the detection was made. A detection held for a
    `release()` verdict no longer pins the history ring for a whole push.
    `min_gap` still holds, and now looks conservative
    ([#1530](https://github.com/doppler-dsp/doppler/issues/1530)).
