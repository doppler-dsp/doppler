- **`BurstCapture` captures across a wide `doppler_uncertainty`**
    ([#1512](https://github.com/doppler-dsp/doppler/issues/1512)). Its
    `doppler_bins` read back only the coherent depth, it converted every tiled
    hit's Doppler to 0, and refine folded its cells about 0. A new
    `acq_bin_doppler_hz()` is the one bin-to-Hz conversion.
