- **`acq`: a burst at exactly half a coherent Doppler bin is detected again**
    ([#1002](https://github.com/doppler-dsp/doppler/issues/1002)). Scalloping
    costs ~3.9 dB at the worst case, and that was **not** margin a caller could
    buy back with signal — `test_stat` saturates against the code's own
    sidelobe floor, so a half-bin burst was invisible at *any* C/N0 (zero
    detections across a 12 dB sweep). The engine now zero-pads its **slow-time**
    transform; the code axis is untouched, because correlation along it is
    circular and padding there would change the correlation rather than
    interpolate it. Pfa over 80 000 frames is byte-identical.
