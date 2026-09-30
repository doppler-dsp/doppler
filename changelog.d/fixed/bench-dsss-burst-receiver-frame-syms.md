- **The `dsss_burst_receiver` C benchmark times the whole frame.** It told
    the receiver `frame_syms = 32` for a 61-symbol burst, so it sliced half
    a frame and none of its bursts passed the CRC. The length is now read
    off the burst the benchmark builds: 30 of 30 bursts pass, where 0 did
    (#1669).
