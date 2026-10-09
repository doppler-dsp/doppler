- **`dp_<t>_framer_frames_in(fr, n)`, the ring framer's stream-level frame
    count.** The frames a stream yields once `n` more samples are in, however
    many feed-and-drain rounds that takes, so it is the bound a consumer sizes
    its output by. `frames_for` is the same count capped by one feed's room
    (`docs/design/ring-buffer.md`).
