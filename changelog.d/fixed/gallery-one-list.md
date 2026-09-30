- **`make gallery` re-renders every plot it publishes.** Its PNG move list is
    now derived from `GALLERY_SCRIPTS`, which gains `plan_background_demo.py`,
    and `make gallery-scripts-check` (on `lint`) fails on a committed plot no
    script re-renders; 19 existing orphans are a ratchet (#1644, #1647).
