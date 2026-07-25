from doppler.track import RateSync


def test_create():
    obj = RateSync(4.0, "rrc", 0.35, 8, 2, 1024, 0.01, 0.707, "gardner")
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with RateSync(4.0, "rrc", 0.35, 8, 2, 1024, 0.01, 0.707, "gardner"):
        pass


def test_destroy():
    obj = RateSync(4.0, "rrc", 0.35, 8, 2, 1024, 0.01, 0.707, "gardner")
    obj.destroy()
