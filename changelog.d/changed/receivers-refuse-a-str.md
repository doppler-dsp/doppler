- **A `str` passed to a `BurstDemod` or `DsssBurstReceiver` bit code is
    refused** (`data_code`, `acq_code`, and `set_preamble`'s code), naming
    `field_bits()`, rather than parsed as a number: their bindings now come
    from jm's current template, with its output-size and error checks too
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620)).
