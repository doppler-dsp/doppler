from doppler.telemetry import Telemetry


def test_create():
    obj = Telemetry(ring_records=16384)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_context_manager():
    with Telemetry(ring_records=16384):
        pass


def test_destroy():
    obj = Telemetry(ring_records=16384)
    obj.destroy()
