- **The SigMF annotation reports what was rendered, not the range's `lo`.**
    Each row took its timing from a replay of the ranged draws and its
    frequency and SNR straight off the source struct — exact about *when* and
    wrong about *what*, which is the half nobody audits. Measured against the
    capture: up to **1224 Hz and 6.0 dB** out, a different operating point
    entirely. Both halves come from one `wfm_compose_draws()` row now, a new
    C API that reports each instance's drawn `freq`/`f_end`/`snr`/`level`
    alongside its timing, and the composer renders through the same helpers.
    `wfmgen:level_db` is new — a drawn `--level` had no key at all.
    [#1086](https://github.com/doppler-dsp/doppler/issues/1086).
