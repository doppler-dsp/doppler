"""
doppler.polyphase
=================

Polyphase FIR filter-bank designer for fractional-rate resampling.

Quick start
-----------
>>> from doppler.polyphase import design_bank
>>> bank = design_bank()          # 4096 phases × 19 taps, Kaiser, 60 dB
>>> bank.shape
(4096, 19)

>>> bank_ls = design_bank(method="firls")   # least-squares (needs SciPy)

>>> from doppler.polyphase import to_c_header, to_npy
>>> to_c_header(bank, path="polyphase_bank.h")  # write C header
>>> to_npy(bank, "polyphase_bank.npy")           # write numpy array
"""

from ._polyphase import (PolyphaseBank, design_bank, plot_response,
                         to_c_header, to_npy)

__all__ = ["PolyphaseBank", "design_bank", "plot_response",
           "to_c_header", "to_npy"]
