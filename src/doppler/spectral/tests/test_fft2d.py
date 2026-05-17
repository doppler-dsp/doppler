from doppler.spectral import FFT2D


def test_create():
    obj = FFT2D(64, 64, -1, 1)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with FFT2D(64, 64, -1, 1):
        pass


def test_destroy():
    obj = FFT2D(64, 64, -1, 1)
    obj.destroy()
