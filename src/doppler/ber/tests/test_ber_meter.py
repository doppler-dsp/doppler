from doppler.ber import BerMeter


def test_create():
    obj = BerMeter(4, 200, 0.99)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with BerMeter(4, 200, 0.99):
        pass


def test_destroy():
    obj = BerMeter(4, 200, 0.99)
    obj.destroy()


def test_score_needs_a_successful_alignment():
    """#2651: before any align(), or after a failed one, score() scores
    nothing; it used to count at lag 0 / phase 0."""
    import numpy as np

    rng = np.random.default_rng(1)
    truth = rng.integers(0, 4, size=800).astype(np.uint8)
    rx = np.exp(1j * (2 * np.pi * np.roll(truth, 37) / 4 + 1.0))
    rx = rx.astype(np.complex64)
    met = BerMeter(m=4)
    met.set_truth(truth)
    assert met.score(rx, hi=truth.size) == 0
    assert (met.symbols, met.errors) == (0, 0)
    assert not met.align(np.zeros_like(rx), n_marker=64)
    assert met.score(rx, hi=truth.size) == 0
    assert (met.symbols, met.errors) == (0, 0)


def test_theory_ber_is_chance_without_signal():
    """#2651: esn0 <= 0 is a coin flip per bit; a negative one was NaN."""
    from doppler.ber import ber_theory_ber

    for m in (2, 4, 8):
        assert ber_theory_ber(m, -1.0) == 0.5
        assert ber_theory_ber(m, 0.0) == 0.5
