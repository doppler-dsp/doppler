- **A file-backed ring recreated at a new size is really zeroed.** On POSIX a
    wrong-size file was resized with one `ftruncate()`, which keeps the old
    bytes — so `dp_*_create_backed()` reported `existed = 0` ("created, and
    zeroed") over the previous ring's samples. Now cut to zero and regrown, as
    the Windows path always did. Found by the ring's claim inventory
    ([#1438](https://github.com/doppler-dsp/doppler/issues/1438)).
