- **`Resampler.set_state()` refuses a blob whose delay-line head lies
    outside the line.** It took the head from the blob unchecked, and the
    size check cannot see a forged field, so the next output read past the
    delay line. Such a blob now raises `ValueError` and leaves the resampler
    as it was. An object that nests a resampler (`DopplerChannel`,
    `RateConverter`, the waveform synthesizer's shaper) no longer reads past
    it either; `RateConverter` does not yet report the refusal (#2104)
    (#2111).
