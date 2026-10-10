"""Every loop that embeds the shared 2nd-order loop filter refuses a bandwidth
or damping outside its domain, at create and in every setter (doppler#2112).

The domain is ``dp_loop_filter_params_ok``: ``bn >= 0`` and ``zeta > 0``, both
finite, whose gains come out finite. A finite ``bn`` of 1e200 squares the
loop's ``th`` to infinity and its integral gain to NaN, so it is outside.

A refused retune changes nothing. The getter then never reports a bandwidth
the loop is not running, and ``CarrierNda`` no longer rescales its unchanged
gains by 1/2pi on each refusal. The writable ``bn`` property refuses without
raising until just-buildit/just-makeit#2182, so the refusal shows as the value
and the serialized state staying put.
"""

import pytest

from doppler.track import CarrierMpsk, CarrierNda, Costas, SymbolSync

LOOP = "bn >= 0 and zeta > 0, both finite, whose loop gains come out finite"

# Each loop at a configuration it builds; a case changes only the loop's
# own bandwidth or damping.
LOOPS = {
    "Costas": (
        Costas,
        {"bn": 0.05, "zeta": 0.707, "init_norm_freq": 0.01, "tsamps": 16},
    ),
    "SymbolSync": (SymbolSync, {"sps": 4, "bn": 0.01, "zeta": 0.707}),
    "CarrierMpsk": (
        CarrierMpsk,
        {
            "bn": 0.05,
            "zeta": 0.707,
            "init_norm_freq": 0.01,
            "tsamps": 16,
            "m": 4,
        },
    ),
    "CarrierNda": (
        CarrierNda,
        {
            "bn": 0.01,
            "zeta": 0.707,
            "init_norm_freq": 0.0,
            "sps": 8,
            "n": 2,
            "m": 4,
        },
    ),
}
BAD = [
    {"bn": float("nan")},
    {"bn": -0.01},
    {"bn": 1e200},
    {"zeta": 0.0},
    {"zeta": float("inf")},
]


@pytest.mark.parametrize("name", LOOPS)
@pytest.mark.parametrize("change", BAD, ids=lambda c: f"{c}")
def test_create_refuses_and_names_the_domain(name, change):
    cls, kw = LOOPS[name]
    cls(**kw)  # the control: this argument, not another, is refused
    with pytest.raises(ValueError, match=f"{name}: invalid parameter.*{LOOP}"):
        cls(**dict(kw, **change))


@pytest.mark.parametrize("name", LOOPS)
def test_a_refused_retune_changes_nothing(name):
    cls, kw = LOOPS[name]
    loop = cls(**kw)
    held, bn = loop.get_state(), loop.bn
    # Twice: a refusal must not compound.
    for _ in range(2):
        loop.bn = float("nan")
        loop.bn = 1e200
    assert loop.bn == bn
    assert loop.get_state() == held
    if hasattr(loop, "configure"):
        for bad in ((float("nan"), 0.707), (0.05, float("inf"))):
            with pytest.raises(ValueError, match="configure failed"):
                loop.configure(*bad)
        assert loop.bn == bn
        assert loop.get_state() == held
    # The control: a good retune is taken.
    loop.bn = 0.02
    assert loop.bn == 0.02
