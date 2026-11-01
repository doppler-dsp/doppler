- **`AsyncDsssPool`'s search runs serially at any `threads`.** `threads`
    now steps the receivers only. Fanning the search's tiles across the
    cores cost 1.5x at ±50 kHz and 8x one epoch deep, and made the pool's
    scene example 1.6x slower (13.0 s against 8.1 s), with the same result.
    A standalone engine still fans by default: doppler#2744.
