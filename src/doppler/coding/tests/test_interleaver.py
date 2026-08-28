"""`Interleaver` — the object face of the block-interleaving permutation.

The PERMUTATION is pinned in C (test_dp_interleave.c against its index map,
test_interleaver_core.c against the object's contract). This file tests what
only the binding can get wrong: the argument marshaling, the refusals reaching
Python as exceptions rather than empty arrays, and `out=`.

doppler#1031.
"""

import numpy as np
import pytest

from doppler.coding import Interleaver


def test_create_and_geometry():
    il = Interleaver(rows=8, cols=32, unit_bits=8)
    assert (il.rows, il.cols, il.unit_bits) == (8, 32, 8)
    assert il.block_bits == 8 * 32 * 8
    # The link budget, named for what it buys rather than for the matrix.
    assert il.burst_len == 8
    assert il.separation == 32


def test_unit_bits_defaults_to_bit_interleaving():
    assert Interleaver(rows=4, cols=4).unit_bits == 1


@pytest.mark.parametrize(
    "rows,cols,unit_bits", [(0, 4, 1), (4, 0, 1), (4, 4, 0)]
)
def test_a_degenerate_geometry_is_refused(rows, cols, unit_bits):
    """Each zero alone, because a guard on the product would accept two of
    the three."""
    with pytest.raises(ValueError, match="non-zero"):
        Interleaver(rows=rows, cols=cols, unit_bits=unit_bits)


def test_round_trip():
    il = Interleaver(rows=4, cols=6)
    x = np.arange(il.block_bits * 3, dtype=np.uint8) % 2
    y = np.asarray(il.interleave(x))
    assert not np.array_equal(y, x), "the permutation moved something"
    assert np.array_equal(np.asarray(il.deinterleave(y)), x)


def test_a_square_block_is_its_own_inverse():
    """Reading a rows x cols matrix by columns is writing a cols x rows one by
    rows, so the two directions differ only by exchanging the arguments -- and
    when they are equal, interleaving twice is the identity."""
    il = Interleaver(rows=6, cols=6)
    x = np.arange(36, dtype=np.uint8)
    assert np.array_equal(
        np.asarray(il.interleave(np.asarray(il.interleave(x)))), x
    )


def test_soft_deinterleave_is_the_same_permutation():
    """The receive path for `DsssBurstReceiver.llrs`: an outer decoder wants
    LLRs de-interleaved BEFORE it runs, and slicing to hard bits first throws
    away the confidence the soft output exists to carry."""
    il = Interleaver(rows=5, cols=7)
    hard = (np.arange(35, dtype=np.uint8) * 13) % 251
    soft = hard.astype(np.float32)
    assert np.array_equal(
        np.asarray(il.deinterleave_soft(soft)),
        np.asarray(il.deinterleave(hard)).astype(np.float32),
    )


def test_interleave_soft_does_not_exist():
    """A transmitter has bits, not LLRs. Building the symmetric half would be
    designing for a caller that does not exist."""
    assert not hasattr(Interleaver(rows=2, cols=2), "interleave_soft")


@pytest.mark.parametrize(
    "method", ["interleave", "deinterleave", "deinterleave_soft"]
)
def test_a_partial_block_raises_rather_than_returning_empty(method):
    """The kernel signals a refusal with 0, and jm has no declarative hook to
    turn that into an exception for a variable_output method
    (just-buildit/just-makeit#1159) -- so the sacred fragment raises by hand,
    and this is what holds it there. An empty array would be a silent wrong
    answer: the frame comes back short and nothing says why."""
    il = Interleaver(rows=3, cols=4)
    dtype = np.float32 if method.endswith("soft") else np.uint8
    bad = np.zeros(il.block_bits + 1, dtype=dtype)
    with pytest.raises(ValueError, match="whole number of blocks"):
        getattr(il, method)(bad)


def test_the_refusal_names_the_block_size():
    """A caller who got the geometry wrong needs the number, not just a no."""
    il = Interleaver(rows=3, cols=4, unit_bits=8)
    with pytest.raises(ValueError, match="block_bits = 96"):
        il.interleave(np.zeros(7, dtype=np.uint8))


def test_out_buffer():
    il = Interleaver(rows=4, cols=4)
    x = np.arange(16, dtype=np.uint8)
    out = np.zeros(16, dtype=np.uint8)
    got = il.interleave(x, out=out)
    assert np.array_equal(np.asarray(got), np.asarray(il.interleave(x)))


def test_reset_is_a_no_op_that_leaves_the_geometry():
    """A reset that quietly cleared the geometry would leave every later call
    refusing -- the failure a "does nothing" function can still have."""
    il = Interleaver(rows=2, cols=3)
    x = np.arange(6, dtype=np.uint8)
    before = np.asarray(il.interleave(x)).copy()
    il.reset()
    assert il.block_bits == 6
    assert np.array_equal(np.asarray(il.interleave(x)), before)


def test_context_manager_and_destroy():
    with Interleaver(rows=2, cols=2) as il:
        assert il.block_bits == 4
    il2 = Interleaver(rows=2, cols=2)
    il2.destroy()
