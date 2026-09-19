- **`validate_receiver_pullin` no longer reads past its noise buffer.** It
    sized the noise for one synth block, but the Doppler channel can return
    more than that per call, so it read one sample past the end. That was
    silent on Linux and a SEGFAULT on Windows; AddressSanitizer names the
    line. Its test harness only; the receiver itself was never affected.
