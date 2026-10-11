- **The specan web server and recorder now tell the source the frame's
    `data_size`.** Each front end ran its own read, process and
    `set_fft_size` copy, and they disagreed: the recorder sent the cropped
    display bin count, and the web server never sent one, so the demo kept
    its default noise level. `SpecanEngine.step` is now the one loop, and the
    terminal, the server and the recorder all call it (#2159).
