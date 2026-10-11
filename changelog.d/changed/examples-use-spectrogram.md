- **The WFM and DSSS demos draw their spectrograms with the library's `Spectrogram`.**
    The WFM demo's hand-written loop dropped its last frame; both demos now keep
    every whole frame, and each checks the row count (#1894).
    The DSSS demo now flushes the owed tail frame, and both demos assert their
    physical checks: the CW's bin in the WFM demo, and payload above silence in
    the DSSS demo.
