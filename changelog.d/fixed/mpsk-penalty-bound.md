- **The M-PSK differential-penalty bound now follows its own error counts.**
    The old 2.2x ceiling passed or failed one seed's draw: the 8PSK 14 dB cell
    (2.17x on 1 281 errors) sat under it by luck. Each cell is now bounded by
    2x plus four sigma of its own count, and the "paired, so luck cancels"
    claim is withdrawn (#2603; the thin QPSK 10 dB cell is #2624).
