from doppler.track import MpskReceiverR


def test_create():
    obj = MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    )
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    ):
        pass


def test_destroy():
    obj = MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    )
    obj.destroy()
