- **just-makeit pin 0.89.0 → 0.90.1**, and the project at jm's schema 8:
    headers live under `native/inc/doppler/`, every include spelled
    `"doppler/..."` (jm#1583) — the source half of the Breaking entry above.
    The gates that named header paths read them from one place,
    `scripts/_layout.py`.
