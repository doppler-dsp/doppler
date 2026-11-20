- **The capacity functions of `Ofdm.modulate` and `Wlan.ppdu` are named for
    what they do.** `dp_ofdm_modulate_why_max_out` is removed (use
    `dp_ofdm_modulate_max_out`, which it only forwarded to) and
    `dp_wlan_ppdu_why_max_out` is now `dp_wlan_ppdu_max_out`. The jm 0.133.0
    manifest key `max_out_fn` names the function the binding calls, so the
    alias the old derivation needed is gone (just-makeit#2680). Python is
    unchanged.
