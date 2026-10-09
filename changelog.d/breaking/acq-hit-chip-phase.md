- **`doppler.dsss.handoff.dll_init_chip_from_acq` is removed; read the hit's
    eighth element, `hit[7]`.** Every `Acquisition` and `BurstAcquisition`
    hit is now an 8-tuple ending in `chip_phase`, the Dll seed, computed once
    in C with the dwell advance applied when a carrier is set (the Python
    copy did not carry it: 0.9 chip late at 20 ppm on the 40 dB-Hz floor).
    Replace `dll_init_chip_from_acq(hit[1], spc, sf)` with `hit[7]`. A
    `BurstAcquisition` is built from a sampled template, one chip per
    sample, so its `hit[7]` is in template samples: divide by your samples
    per chip for chips. See
    [#1257](https://github.com/doppler-dsp/doppler/issues/1257).
