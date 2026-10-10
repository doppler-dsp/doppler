- **The receiver examples no longer count a publisher restart as dropped
    frames.** `native/examples/receiver.c` and `src/doppler/examples/receiver.py` took
    `seq - last - 1` unconditionally, so a sequence reset to 0 wrapped the C
    counter to about 1.8e19 and added a negative in Python. Only a forward
    jump counts now; a repeat or a backwards jump adds nothing (#2017).
