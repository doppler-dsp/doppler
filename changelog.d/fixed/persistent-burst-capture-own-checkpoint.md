- **`PersistentBurstCapture` restores its own post-push checkpoint.** The
    object that created the backing file refused every blob it took after a
    push, while a fresh object over the same file accepted them: the guard
    asked whether the file had been *adopted*, not whether it *held* the
    span the blob names. It now asks the second, on either flavour, and also
    refuses a span the ring has wrapped past
    ([#1190](https://github.com/doppler-dsp/doppler/issues/1190)).
