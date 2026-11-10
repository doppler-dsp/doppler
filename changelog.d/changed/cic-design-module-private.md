- **`doppler.resample.cic_design` is the function, no longer a module.**
    The CIC design helpers live in the private `_cic_design.py`, because a
    module named like its own `cic_design` function hid that function from
    the API reference. `from doppler.resample import cic_design` (and the
    other four helpers) works as before; the module path
    `doppler.resample.cic_design` no longer imports (#3004).
