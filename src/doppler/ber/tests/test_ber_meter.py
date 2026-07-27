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
