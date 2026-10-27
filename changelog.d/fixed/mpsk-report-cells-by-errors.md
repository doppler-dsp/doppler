- **The M-PSK validation report sizes each cell by its error count.** A fixed
    200k symbols left QPSK at 10 dB with ~300 errors, so its penalty band was
    ±0.58x. Each cell now targets 4 000 coherent errors (clamped to 200k–2.5M
    symbols), and that cell's band is ±0.16x (#2624).
