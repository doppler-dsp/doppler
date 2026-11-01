- **`Dll` takes any finite samples a chip of at least one, not only whole
    ones.** Its partials close on the code NCO's phase, so a period need not
    be whole samples: at 2.2 a chip (11 MSa/s over 5 Mcps) it tracks to
    0.0011 chip, as at 2. `sps` is a float in Python and a `double` in C
    (the ABI changes: rebuild), and NaN, inf or under 1 is refused, by
    `dp_dll_create()` and by `dp_dll_init()`, which now returns `int`.
