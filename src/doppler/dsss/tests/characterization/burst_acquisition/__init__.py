"""Characterization of `BurstAcquisition` — Pd / Pfa vs Es/N0.

`characterize.py` sweeps the noise floor and measures detection
probability against the true (Doppler bin, code phase) cell, plus the
CFAR false-alarm rate on noise-only frames, over random code phase and
random Doppler across the engine's whole capture range.

Distinct from the sibling `acquisition` subject, which drives
`Acquisition` (the continuous engine) rather than `BurstAcquisition`.
See the parent package for why this is not an example.

## The only subject with a published page

`docs/gallery/dsss-acq-characterization.md` embeds
`docs/assets/dsss_acq_characterization.png`, and that image is now
produced by **`make gallery`**, which re-runs this script with the asset
path as its argument (the `argv` its `__main__` accepts). It is therefore
the one characterization plot that is committed, and it is committed in
exactly one place.

It got there because nothing else refreshed it. The asset had been
hand-committed on 2026-07-11 and neither `make gallery` (this script was
never in `GALLERY_SCRIPTS`) nor `make characterize` touched it;
regenerating it moved 105,324 bytes to 122,397, so the published figure
was a month behind its own code.

`make characterize` still writes `burst_acquisition.png` beside this file
as a working artifact, and that copy is gitignored — one rendering is
committed, so there is nothing to diverge.
"""
