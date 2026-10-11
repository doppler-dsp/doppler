- **The WFM and DSSS demos draw their spectrograms with the library's `Spectrogram`.**
    The WFM demo's hand-written loop dropped its last frame; both demos now keep
    every whole frame, and each checks the row count (#1894).
