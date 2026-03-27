"""
doppler.polyphase
=================

Kaiser window design formulas for polyphase FIR filter banks.
"""

from ._polyphase import kaiser_beta, kaiser_taps, kaiser_prototype

__all__ = ["kaiser_beta", "kaiser_taps", "kaiser_prototype"]
