- **`Resampler.set_state()` refuses a forged delay-line head.** A head
    outside the line made the next output read past it. Such a blob now
    raises `ValueError` and leaves the resampler unchanged (#2111).
