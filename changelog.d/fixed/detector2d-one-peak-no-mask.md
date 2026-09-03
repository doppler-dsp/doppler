- **`detector2d::push` no longer pays for the peak list it does not use.**
    At the default `max_peaks = 1` the detector cleared a mask over the whole
    surface and ran the zone-excluding scan for one peak on every push, 22–43%
    slower than the argmax it replaced (measured on the release bench box,
    both builds). `det_peak_list` takes a NULL mask at one peak and runs the
    plain loop; the answer is unchanged, pinned by a tie-rich equivalence test.
    ([#1208](https://github.com/doppler-dsp/doppler/issues/1208))
