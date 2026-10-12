- **`dp_carrier_acq_create` refuses the rectangular window: a C caller passing
    3 now gets NULL.** v0.65.0 and v0.66.0 built an object for it, though the
    header, the manifest and the Python face offer only Hann, Kaiser and
    Blackman-Harris; Python never accepted "rect" (#2185).
