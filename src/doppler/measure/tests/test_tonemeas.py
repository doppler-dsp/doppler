from doppler.measure import ToneMeasure


def test_create():
    obj = ToneMeasure(8192, 1.0, 8, 1.0, 0, 90.0, 0)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with ToneMeasure(8192, 1.0, 8, 1.0, 0, 90.0, 0):
        pass


def test_destroy():
    obj = ToneMeasure(8192, 1.0, 8, 1.0, 0, 90.0, 0)
    obj.destroy()
