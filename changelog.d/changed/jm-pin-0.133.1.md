- **just-makeit pin 0.133.0 → 0.133.1.** A bit field of a wfm source
    (`data`, `fill`, `acq_code`, `data_code`, `mac_header`, `sync`) now raises
    `ValueError` for any element other than 0 or 1, whether it arrives as
    `bytes`, a list, an array or JSON; before, a `bytes` of octets was taken
    octet-for-bit and rendered a silent wrong waveform
    ([#3194](https://github.com/doppler-dsp/doppler/issues/3194)). Octets go
    through `np.unpackbits` first. `Synth.fs` is documented as its real
    default, 1e6 Hz, so the documented-defaults test's exemption is gone
    ([#3142](https://github.com/doppler-dsp/doppler/issues/3142)). Many
    docstrings now state `default V` where the manifest declares one.
