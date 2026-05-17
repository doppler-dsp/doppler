from doppler.resample import Resampler


def test_create():
    obj = Resampler(0.0)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with Resampler(0.0):
        pass


def test_destroy():
    obj = Resampler(0.0)
    obj.destroy()
