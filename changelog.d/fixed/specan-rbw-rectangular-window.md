- **The spectrum analyzer no longer paints -13 dB sidelobes at some RBWs.**
    The window length was rounded to a power of two, so an RBW of `fs_out/2^k`
    got a rectangular window (Kaiser beta 0); the specan demo (4 kHz) showed
    exactly that. Every RBW now gets beta ~12, about -90 dB sidelobes.
