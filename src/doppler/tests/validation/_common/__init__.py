"""Shared validation machinery.

Anything here is used by more than one object's `validate.py`. The rule
that keeps this folder honest: something moves in when a SECOND object
needs it, not in anticipation of one — an abstraction with a single
caller is a guess about the second.

`linear_loop` is the first resident. It is the closed-loop reference
every steered object is measured against: the same detector -> loop
filter -> NCO structure each of them is, with the detector left as a
parameter so the ideal one (plain subtraction) establishes the limit and
each object's own discriminator is reported as a deduction from it.
"""
