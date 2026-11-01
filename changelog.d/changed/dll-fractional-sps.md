- **`Dll` takes any positive samples a chip, not only whole ones.** Its
    partials close on the code NCO's phase and its taps sit at each
    sample's dwell centre, so a code period need not be a whole number of
    samples: at 2.2 samples a chip (11 MSa/s over 5 Mcps) it tracks to
    0.0011 chip, as at 2. `sps` is a float in Python and a `double` in
    `dp_dll_create()`/`dp_dll_init()`; integer callers are unchanged.
