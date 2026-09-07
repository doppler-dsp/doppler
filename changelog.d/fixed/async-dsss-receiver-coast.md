- **A departed emitter's receiver holds its loops instead of running them
    on noise** (#1271, design §10, §12.19). Free-running, a code loop swept
    its phase at up to 90 chips per second through every live emitter's,
    captured one crossing slowly enough and flickered its code flag on it
    sixteen times in fourteen seconds, so the release never came. Once
    locked, both flags down now hold both loops at the state marked with
    both flags up (one flag down is a degrade and the loops run); a
    neighbour 2 kHz off crossing at 4 chips/s is followed on every seed
    without the hold and on none with it. `dll_set_coast()` /
    `dll_hold_here()`; the receiver blob is version 4, the Dll's 11.
