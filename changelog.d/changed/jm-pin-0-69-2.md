- **just-makeit pin 0.68.0 → 0.69.2.** Brings the fix for #1052:
    `Composer.stream(realtime=)` is a sample rate in **Hz**, and now says so on
    both faces — plus a warning when the first block would take over a minute
    of wall clock (`realtime=1.0` on a 1000 block is 1000 s of silence), and a
    `ValueError` on a negative rate that used to disable pacing quietly. Also
    picks up 0.69.0's `doppler.h`, which no longer wraps its component
    includes in `extern "C"` and so can be included from C++, and **0.69.2's
    fix for just-buildit/just-makeit#1164** — 0.69.0's gh-1154 DOC finding
    reported 28 authored `@code` doctest fences as drift, which is why this
    pin sat on 0.69.1 unmerged rather than suppressing them.
