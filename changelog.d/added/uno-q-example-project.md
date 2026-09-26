- **`example-projects/uno-q/`: an RTL-SDR receive front end**, `cu8` →
    `U8ToF32` → DDC → PSD, self-testing on a synthetic capture (gated as
    `make uno-q-check` on Linux x86-64, aarch64 and macOS) or reading a live
    `cu8` stream. Measured on an Arduino UNO Q: 8.3× real time on one core,
    and `-mcpu=cortex-a53` (not `-march=native`) is the tuning that helps.
