"""The gallery scene, read for ghosts: receivers tracking no emitter.

Runs src/doppler/examples/async_dsss_pool_scene.py's own stream, pool and
matcher (make_stream, run, holder_of: an emitter is matched when the
receiver's carrier is within 60 Hz and its code phase within 3 chips of
the emitter's truth), on whatever tree is installed, and prints:

1. the pool's releases by kind (ghost, duplicate, lost);
2. the receiver-epochs spent tracking with no emitter matched, in total
   and per receiver, with each receiver's longest such stretch (epochs and
   seconds; one epoch is one code period, 1023 chips at 3.069 Mcps).

The scene is ten emitters at 2.2 GHz, 3.069 Mcps, Es/N0 20 dB at level 0,
levels within +-8 dB (a 16 dB spread), moving Doppler. Prediction, written
before the first run: 0 ghost and 0 duplicate releases, and unmatched
tracking only as pull-in transients, no stretch over 0.1 s, on both the
head and the baseline before the fine hand-over (b1b1090d5).

Usage:  python -I read_scene.py   (prints; the record keeps each tree's
output as <tree>.txt)
"""

import numpy as np

from doppler.examples import async_dsss_pool_scene as sc


def main() -> None:
    rec = sc.run(sc.make_stream())
    r = rec["released"]
    print(
        f"releases: ghost {r['ghost']}, duplicate {r['duplicate']}, "
        f"lost {r['lost']}"
    )
    held = sc.holder_of(rec)
    unmatched = (rec["state"] == 2) & (held < 0)
    print(
        f"receiver-epochs tracking with no emitter matched: "
        f"{int(unmatched.sum())} of {int((rec['state'] == 2).sum())} "
        f"tracking (epoch {sc.EPOCH_S * 1e3:.3f} ms)"
    )
    for i in range(sc.N_RECEIVERS):
        col = unmatched[:, i].astype(int)
        best = run = 0
        for v in col:
            run = run + 1 if v else 0
            best = max(best, run)
        print(
            f"  receiver {i}: {int(col.sum())} epochs, longest stretch "
            f"{best} epochs ({best * sc.EPOCH_S:.3f} s)"
        )


if __name__ == "__main__":
    np.set_printoptions(precision=3)
    main()
