- **`AsyncDsssReceiver`: the refine → track hand-over advances the code
    phase by the clock dilation over the refine (#1249).** The live chain
    was re-seeded with the hand-off's own code phase, valid only on an
    undilated clock; at 20 ppm the code runs 100 chips/s ahead, and a Dll
    seeded 1.5–5 chips off never locked: at 20 ppm with a 500 Hz/s ramp on
    top, 2 of 3 hand-offs settled at 45 dB-Hz and none at the floor. Now
    10 of 10 at 45 dB-Hz in 70 ms (design §12.9); what remains at the floor
    is carrier loop SNR, measured there (#1252).
