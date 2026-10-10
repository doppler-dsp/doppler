- **The ring framer's `set_state` refuses a carry no stream can produce.**
    A snapshot whose counters agreed (`written - frames * hop == live`) but
    claimed fewer than `frame_n - hop` live samples with a frame already out
    was accepted. After that, `pending()` wrapped to about 1.8e19 and `flush()`
    emitted a row of nothing. Once a frame has been handed out, a drained
    framer always holds at least `frame_n - hop` samples, so such a blob is
    now `DP_ERR_INVALID`. This tightens the framed face shipped in v0.65.0;
    the framer's certification adds it as a limit.
