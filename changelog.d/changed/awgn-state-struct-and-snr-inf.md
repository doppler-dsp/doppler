- **`dp_awgn_state_t` loses its unread `vs` member (`sizeof` 304 → 48), and a
    `wfm` synth at `snr = -inf` (or below about −388 dB over fs) fails at
    create.** Nothing in the tree embeds the struct. The synth used to emit
    inf/NaN noise for such an SNR; it now fails, and the exception class it
    raises is #1800's to fix (#2084).
