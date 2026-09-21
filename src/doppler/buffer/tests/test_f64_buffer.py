"""Lifecycle smoke for F64Buffer: the members jm generates around the ring.

The ring's behaviour is test_buffer.py's, parametrized over all three
widths. What is here is only what arrives with the generated binding and
has no C twin: the context manager, idempotent destroy, and the guard on a
destroyed handle.
"""

import pytest

from doppler.buffer import F64Buffer


def test_create_refuses_a_ring_of_nothing():
    # NULL from create() is a bad size, so it is a ValueError -- not the
    # MemoryError a blanket NULL check would raise. Zero is the only one:
    # any size from 1 up is a ring of exactly that many samples.
    with pytest.raises(ValueError, match="at least 1"):
        F64Buffer(0)
    assert F64Buffer(1000).capacity == 1000


def test_context_manager_destroys_on_exit():
    with F64Buffer(1024) as buf:
        assert buf.capacity >= 1024
    with pytest.raises(RuntimeError):
        _ = buf.capacity


def test_destroy_is_idempotent_and_guards_every_member():
    buf = F64Buffer(1024)
    buf.destroy()
    buf.destroy()
    with pytest.raises(RuntimeError):
        buf.peek(1)
    with pytest.raises(RuntimeError):
        _ = buf.available
