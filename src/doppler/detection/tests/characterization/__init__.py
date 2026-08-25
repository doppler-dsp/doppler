"""Characterization sweeps for `doppler.detection` — the long ones.

See `src/doppler/dsss/tests/characterization/__init__.py` for the category's
full rationale. In short: a **validation** report asks whether the certified
limits still hold and runs on every push; a **characterization** asks how the
object behaves across its whole envelope, costs minutes, and runs only under
`make characterize`.

The one subject here is `models`, and the question it answers is
the one this module cannot answer analytically about itself: **every Pd on
offer is a semi-analytical model, so how far can each be trusted?** The only
external truth for a probability is a frequency, which means Monte-Carlo, and
enough draws to resolve a 1e-6 tail is not something to spend on every push.
"""
