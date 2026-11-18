- **`wlan`: `dp_wlan_layout` takes at most 1,366 Data symbols.** That is the
    largest PSDU at the slowest rate (`WLAN_N_SYM_MAX`); the old limit was
    65,533, which no PPDU reaches. `Wlan` no longer lists a "not a whole
    number" refusal that could not happen. See the
    [802.11 guide](docs/guide/wlan.md).
