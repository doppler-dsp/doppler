- **PSD windows each frame with the periodic (DFT-even) form**
    ([#2053](https://github.com/doppler-dsp/doppler/issues/2053)), so every
    PSD reading moves, and with it the Spectrogram, the measure objects and
    the analyzer: Hann's ENBW is exactly 1.5 bins and a bin-centred Hann
    tone leaks nothing past its neighbours. The window functions themselves
    stay symmetric, for filter design.
