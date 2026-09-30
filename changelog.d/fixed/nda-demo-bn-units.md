- **The NDA carrier gallery plot shows the loop it describes again.**
    `mpsk_nda_theory_demo.py` kept `bn=0.02` after #300 made `bn`
    cycles/sample, so its loop ran twice as wide with ~3x the jitter. It now
    builds the 0.01 cycles/sample loop and asserts the jitter against the
    loop's own theory (#1675).
