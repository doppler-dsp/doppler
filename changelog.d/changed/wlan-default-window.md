- **`Wlan` with no `window` now renders the standard's 100 ns transition.**
    The default was no window (`window=0`); it is `2 * oversample` samples
    at the output rate, so a default PPDU is that much longer and its symbol
    boundaries are cross-faded. At `oversample=4` the transmit mask's
    tightest margin is 7.9 dB where the rectangular default failed the
    30 MHz point. `window=0` is the old output. In C,
    `WLAN_WINDOW_STANDARD` is the new reserved `window` argument of
    `dp_wlan_create` and `dp_wlan_window_resolve` its one definition
    (#3190).
