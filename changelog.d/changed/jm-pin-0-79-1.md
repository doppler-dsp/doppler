- **just-makeit pin 0.78.1 → 0.79.1.** Pure tooling: `jm apply` changes no
    generated file. It brings the capability the ring-buffer migration was
    blocked on — a record declared once per component and taken *in* as
    well as returned (`--arg-type 'iq16_t[]'`, jm gh-1405, doppler-driven),
    with 0.79.1 fixing the manifest-first `apply` path doppler uses
    (gh-1411).
