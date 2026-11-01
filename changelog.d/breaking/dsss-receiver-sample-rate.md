- **`DsssReceiver` takes `sample_rate`; `spc` is gone.** A whole number of
    samples a chip, at least one, as before; a fractional rate is refused
    (`AsyncDsssReceiver` takes any). `sample_rate=0`, the default, is two
    samples a chip at whatever `chip_rate` is -- the old `spc=2` -- and
    `AsyncDsssReceiver` and `AsyncDsssPool` default the same way.
