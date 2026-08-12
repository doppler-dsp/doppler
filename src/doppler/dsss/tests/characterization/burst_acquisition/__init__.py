"""Characterization of `BurstAcquisition` — Pd / Pfa vs Es/N0.

`characterize.py` sweeps the noise floor and measures detection
probability against the true (Doppler bin, code phase) cell, plus the
CFAR false-alarm rate on noise-only frames, over random code phase and
random Doppler across the engine's whole capture range.

Distinct from the sibling `acquisition` subject, which drives
`Acquisition` (the continuous engine) rather than `BurstAcquisition`.
See the parent package for why this is not an example.

## Its gallery page shows a DIFFERENT file

This subject is the only one with a published page,
`docs/gallery/dsss-acq-characterization.md`, and that page embeds
`docs/assets/dsss_acq_characterization.png` — a separately committed
image which `make characterize` does **not** refresh and which
`make gallery` has never regenerated either (this script is not in
`GALLERY_SCRIPTS`). So the sweep now renders `burst_acquisition.png`
beside this file while the page keeps showing its own committed copy.

That is pre-existing rather than introduced here, and it is written down
rather than left to be discovered: the two can disagree, and the page's
image is the older of the two. Worth resolving in one direction — either
the page points at this subject's output, or the asset joins the gallery
pipeline — but that is a docs-pipeline decision, not part of moving the
sweep out of the per-push gate.
"""
