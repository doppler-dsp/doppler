- **`PSD(window="rect")`, and the per-frame kernel behind `PSD` as a public C
    call.** A rectangular window joins Hann, Kaiser and Blackman-Harris, and
    `dp_psd_frame_power` / `dp_psd_frame_db` expose the one frame's spectrum that
    `accumulate` already folds, so a spectrogram row and a one-frame PSD are the
    same numbers. Existing windows' output is unchanged, bit for bit.
