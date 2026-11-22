- **`dp_wlan_layout` refuses a too-long window before it touches the
    modulator.** A window above 80 × oversample used to be refused only after
    the L-STF and L-LTF classes were added to `o`. Every refusal by the
    arguments now leaves `o` untouched, as its header says. `dp_ofdm_add_class`
    no longer refuses a NaN `scale`: a NaN parameter gives NaN samples, the
    library's rule (#2103).
