- **`make bench-preflight` clears a release's bench outputs once main has
    them.** Every per-release bench clone kept its snapshot and rendered page:
    26 clones, about 10 GB, by 2026-10-14. The box now keeps one clone. The
    preflight removes what origin/main holds byte for byte, refuses over
    anything it lacks, and runs first in `make bench-interleaved`. The
    procedure is in `docs/dev/release.md` §2b.
