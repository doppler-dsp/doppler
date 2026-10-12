- **`dp_carrier_acq_create` refuses the rectangular window.** Its header
    offers Hann, Kaiser and Blackman-Harris, and its threshold was never
    measured at rect, which it accepted anyway; the Python face could not
    pass it (#2185).
