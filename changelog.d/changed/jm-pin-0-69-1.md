- **just-makeit pin 0.68.0 → 0.69.1.** Brings the fix for #1052:
    `Composer.stream(realtime=)` is a sample rate in **Hz**, and now says so on
    both faces — plus a warning when the first block would take over a minute
    of wall clock (`realtime=1.0` on a 1000 block is 1000 s of silence), and a
    `ValueError` on a negative rate that used to disable pacing quietly. Also
    picks up 0.69.0's `doppler.h`, which no longer wraps its component
    includes in `extern "C"` and so can be included from C++.
