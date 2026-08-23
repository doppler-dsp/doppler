"""A ring can say it is finished, and a waiting consumer hears it.

Without an end-of-stream marker a consumer cannot tell "the producer is
slow" from "the producer has finished" — both look like an empty ring — so
`wait()` had nothing to do but spin. `close()` is the producer's half;
`wait()` then raises `EOFError` once the ring is drained.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer


def test_a_new_ring_is_open() -> None:
    buf = F32Buffer(1024)
    assert buf.closed is False


def test_close_is_visible_to_the_consumer() -> None:
    buf = F32Buffer(1024)
    buf.close()
    assert buf.closed is True


def test_wait_raises_eof_once_closed_and_drained() -> None:
    """The whole point: the wait ends instead of spinning forever."""
    buf = F32Buffer(1024)
    buf.close()
    with pytest.raises(EOFError):
        buf.wait(64)


def test_closing_does_not_discard_what_was_written() -> None:
    """A producer's last batch survives the close that follows it."""
    buf = F32Buffer(1024)
    buf.write(np.arange(64, dtype=np.complex64))
    buf.close()

    got = buf.wait(64)
    assert len(got) == 64
    assert got[0] == 0
    buf.consume(64)

    # Drained now, so the next wait ends rather than blocking.
    with pytest.raises(EOFError):
        buf.wait(64)


@pytest.mark.parametrize(
    ("cls", "dtype"),
    [(F32Buffer, np.complex64), (F64Buffer, np.complex128), (I16Buffer, None)],
)
def test_every_ring_type_has_the_same_vocabulary(cls, dtype) -> None:
    """The macro generates a type per instantiation, so f32 passing says
    nothing about f64 and i16."""
    buf = cls(1024)
    assert buf.closed is False
    buf.close()
    assert buf.closed is True
    with pytest.raises(EOFError):
        buf.wait(16)
