- **CCSDS Reed-Solomon (255,223)**, the E=16 outer code (131.0-B-3 section
    4.3), with the three conventions a textbook implementation gets wrong and
    a round trip cannot catch: the field is `x^8 + x^7 + x^2 + x + 1` (4.3.3)
    rather than the habitual one, the generator's roots are powers of `a^11`
    rather than consecutive powers of `a` (4.3.4), and symbols travel in the
    **dual (Berlekamp) basis** (4.3.9.1). Verified against Annex G, which
    publishes all 33 coefficients of `g(x)` — reproducing them exercises the
    field and the root stride together — and against 4.3.9.3's two basis
    matrices, required to invert each other across all 256 symbols so a single
    mis-transcribed bit cannot pass.
