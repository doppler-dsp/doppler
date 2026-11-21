- **`Wlan`'s `window` default names its constant.** The signature reads
    `window: int = ...` and the docstring `default WLAN_WINDOW_STANDARD`
    instead of the number 4294967295; the behaviour is unchanged (an omitted
    window is the standard's `2 * oversample`, `window=0` is rectangular).
