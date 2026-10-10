- **`PSD` and `AccTrace` refuse an invalid argument with a `ValueError` that
    names the rules, instead of a reasonless `MemoryError`.** An exp-mode
    `alpha=-0.5` used to report running out of memory. Code catching
    `MemoryError` around either constructor must catch `ValueError` now; a
    genuine allocation failure reads as `ValueError` too (#1986).
