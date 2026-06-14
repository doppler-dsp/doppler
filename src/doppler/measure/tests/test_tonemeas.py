from doppler.measure import ToneMeasure


def test_create():
    obj = ToneMeasure("kaiser", 8192, 1.0, 12.0, 2, 8, 1.0, 0)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with ToneMeasure("kaiser", 8192, 1.0, 12.0, 2, 8, 1.0, 0):
        pass


def test_destroy():
    obj = ToneMeasure("kaiser", 8192, 1.0, 12.0, 2, 8, 1.0, 0)
    obj.destroy()
