- **just-makeit pin 0.91.0 → 0.92.0.** Pure tooling for doppler: `jm apply`
    regenerates nothing and `jm upgrade` finds schema 8 already current. The
    release carries `[project] c_prefix` and the `jm upgrade` respell
    (jm#1591 phase 3), which the `dp_` symbol prefix (#1545) adopts next.
