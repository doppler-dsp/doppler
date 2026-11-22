"""The general OFDM modulator, held against an independent numpy render.

``ref_render`` below is written from the contract in ``ofdm_core.h`` and
shares no code with the C: it builds each grid with explicit loops, inverts it
with ``numpy.fft`` (not the library's ``fft``), and overlap-adds the window
from the formula. It is the oracle for the whole render across randomly drawn
layouts that exercise every field. It names no standard and the tests claim
none.
"""

import numpy as np
import pytest

from doppler.ofdm import Ofdm

# the C sentinels OFDM_CP_DEFAULT and OFDM_ALL_PHASES; test_ofdm_core.c holds
# that both are 2**32 - 1
CP_DEFAULT = 2**32 - 1
ALL_PHASES = 2**32 - 1


def _useful(c):
    return c["useful"] or c["n_fft"]


def _run_cp(c, r):
    return c["cp"] if r["cp"] == CP_DEFAULT else r["cp"]


def build(layout):
    """Declare ``layout`` (classes, runs) on a fresh object."""
    o = Ofdm()
    for c in layout["classes"]:
        i = o.add_class(
            c["n_fft"],
            c["map"],
            useful=c["useful"],
            cyclic_prefix=c["cp"],
            window=c["window"],
            scale=c["scale"],
            n_phase=c["n_phase"],
        )
        if len(c["gain"]):
            o.add_subcarrier_gain(i, c["gain"])
        if len(c["polarity"]):
            o.add_polarity(i, c["polarity"])
        for phase, k, v in c["fixed"]:
            o.add_fixed(i, phase, k, v)
    for r in layout["runs"]:
        o.add_run(
            r["cls"],
            r["count"],
            cyclic_prefix=r["cp"],
            polarity_offset=r["pol"],
            first_phase=r["phase"],
        )
    return o


def symbols(layout):
    """(class, run, index in the run) of each OFDM symbol, in order."""
    for r in layout["runs"]:
        for s in range(r["count"]):
            yield layout["classes"][r["cls"]], r, s


def need(c, phase):
    n = int(np.count_nonzero(c["map"]))
    for ph, k, _ in c["fixed"]:
        if ph in (phase, ALL_PHASES) and c["map"][k + c["n_fft"] // 2]:
            n -= 1
    return n


def ref_render(layout, points):
    """The whole layout as complex128, then the final window fall."""
    syms = list(symbols(layout))
    chunks, pos = [], 0
    for c, r, s in syms:
        n = c["n_fft"]
        phase = (r["phase"] + s) % c["n_phase"]
        pol = c["polarity"]
        pv = pol[(r["pol"] + s) % len(pol)] if len(pol) else 1.0
        grid = np.zeros(n, complex)  # index k + n/2
        taken = np.zeros(n, bool)
        for ph, k, v in c["fixed"]:
            if ph in (phase, ALL_PHASES):
                grid[k + n // 2] = v * pv
                taken[k + n // 2] = True
        for idx in range(n):
            if c["map"][idx] and not taken[idx]:
                grid[idx] = points[pos]
                pos += 1
        if len(c["gain"]):
            grid = grid * c["gain"]
        x = np.fft.ifft(np.fft.ifftshift(grid)) * n * c["scale"]
        u, cp = _useful(c), _run_cp(c, r)
        body = x[np.arange(u) % n]
        chunks.append((np.concatenate([body[u - cp :], body]), x, u, c, r))
    assert pos == len(points)

    out = []
    tail = np.zeros(0, complex)
    for i, (sym, x, u, c, _r) in enumerate(chunks):
        n = c["n_fft"]
        w_in = c["window"]
        rise = np.sin(np.pi * (np.arange(w_in) + 0.5) / (2 * w_in)) ** 2
        sym = sym.astype(complex)
        sym[:w_in] *= rise
        sym[: len(tail)] += tail
        out.append(sym)
        nc = chunks[i + 1][3] if i + 1 < len(chunks) else c
        w = nc["window"]
        nrise = np.sin(np.pi * (np.arange(w) + 0.5) / (2 * w)) ** 2
        tail = x[(u + np.arange(w)) % n] * (1 - nrise)
    return np.concatenate(out), tail


def draw_layout(rng):
    """A random layout touching every field."""
    classes = []
    for _ in range(int(rng.integers(1, 4))):
        n = int(rng.choice([8, 12, 16]))
        m = (rng.random(n) < 0.7).astype(np.uint8)
        useful = int(rng.choice([0, 0, n, n + n // 2, 2 * n]))
        u = useful or n
        cp = int(rng.integers(0, u // 2 + 1))
        n_phase = int(rng.integers(1, 5))
        fixed, seen = [], set()
        for _ in range(int(rng.integers(0, 4))):
            k = int(rng.integers(-n // 2, n // 2))
            ph = (
                int(rng.integers(0, n_phase))
                if rng.random() < 0.6
                else ALL_PHASES
            )
            if any(
                (k == kk and (ph in (pp, ALL_PHASES) or pp == ALL_PHASES))
                for pp, kk in seen
            ):
                continue
            seen.add((ph, k))
            fixed.append((ph, k, complex(*rng.normal(size=2))))
        fixed.sort(key=lambda e: (e[0], e[1]))
        classes.append(
            {
                "n_fft": n,
                "map": m,
                "useful": useful,
                "cp": cp,
                "window": int(rng.integers(0, min(cp + u, 5) + 1)),
                "scale": float(rng.uniform(0.3, 2.0)),
                "gain": (
                    (rng.normal(size=n) + 1j * rng.normal(size=n)).astype(
                        np.complex64
                    )
                    if rng.random() < 0.5
                    else np.zeros(0, np.complex64)
                ),
                "n_phase": n_phase,
                "polarity": (
                    (rng.normal(size=3) + 1j * rng.normal(size=3)).astype(
                        np.complex64
                    )
                    if rng.random() < 0.5
                    else np.zeros(0, np.complex64)
                ),
                "fixed": fixed,
            }
        )
    runs = []
    for _ in range(int(rng.integers(1, 5))):
        ci = int(rng.integers(0, len(classes)))
        c = classes[ci]
        u = _useful(c)
        runs.append(
            {
                "cls": ci,
                "count": int(rng.integers(1, 4)),
                "cp": CP_DEFAULT
                if rng.random() < 0.6
                else int(rng.integers(c["window"], u + 1)),
                "pol": int(rng.integers(0, 7)),
                "phase": int(rng.integers(0, c["n_phase"])),
            }
        )
    return {"classes": classes, "runs": runs}


def points_for(layout, rng):
    n = sum(
        need(c, (r["phase"] + s) % c["n_phase"]) for c, r, s in symbols(layout)
    )
    return (rng.normal(size=n) + 1j * rng.normal(size=n)).astype(np.complex64)


SEEDS = range(24)


@pytest.mark.parametrize("seed", SEEDS)
def test_matches_the_independent_render(seed):
    rng = np.random.default_rng(seed)
    layout = draw_layout(rng)
    pts = points_for(layout, rng)
    want, want_tail = ref_render(layout, pts)
    o = build(layout)
    assert o.points_remaining() == pts.size
    got = o.modulate(pts)
    assert got.dtype == np.complex64
    assert got.size == want.size
    tol = 2e-5 * max(1.0, float(np.abs(want).max()))
    np.testing.assert_allclose(got, want, atol=tol, rtol=0)
    tail = o.finish()
    assert tail.size == want_tail.size
    np.testing.assert_allclose(tail, want_tail, atol=tol, rtol=0)
    assert o.finish().size == 0


@pytest.mark.parametrize("seed", SEEDS)
def test_a_split_render_resumes_bit_for_bit(seed):
    rng = np.random.default_rng(1000 + seed)
    layout = draw_layout(rng)
    pts = points_for(layout, rng)
    whole = build(layout)
    one = whole.modulate(pts)
    one_tail = whole.finish()

    cum = np.cumsum(
        [
            need(c, (r["phase"] + s) % c["n_phase"])
            for c, r, s in symbols(layout)
        ]
    )
    for cut in range(1, len(cum)):
        a = build(layout)
        first = a.modulate(pts[: cum[cut - 1]])
        blob = a.get_state()
        assert isinstance(blob, bytes) and len(blob) == a.state_bytes()
        b = build(layout)
        b.set_state(blob)
        rest = b.modulate(pts[cum[cut - 1] :])
        assert np.array_equal(np.concatenate([first, rest]), one)
        assert np.array_equal(b.finish(), one_tail)


def test_state_rejects_another_layout_and_a_bad_blob():
    rng = np.random.default_rng(7)
    layout = draw_layout(rng)
    pts = points_for(layout, rng)
    a = build(layout)
    c0, r0, _ = next(symbols(layout))
    a.modulate(pts[: need(c0, r0["phase"] % c0["n_phase"])])
    blob = a.get_state()
    other = {**layout, "classes": [dict(c) for c in layout["classes"]]}
    other["classes"][0]["scale"] = layout["classes"][0]["scale"] + 1.0
    with pytest.raises(ValueError):
        build(other).set_state(blob)
    with pytest.raises(ValueError):
        build(layout).set_state(b"\xff" + blob[1:])
    with pytest.raises(ValueError):
        build(layout).set_state(blob[:-1])
    with pytest.raises(TypeError):
        build(layout).set_state("blob")  # type: ignore[arg-type]


def _base_layout():
    m = np.zeros(8, np.uint8)
    m[[1, 2, 3, 5, 6, 7]] = 1
    return {
        "classes": [
            {
                "n_fft": 8,
                "map": m,
                "useful": 0,
                "cp": 2,
                "window": 1,
                "scale": 1.0,
                "gain": (np.arange(8) + 1j).astype(np.complex64),
                "n_phase": 2,
                "polarity": np.array([1, -1], np.complex64),
                "fixed": [(0, 3, 1j), (ALL_PHASES, -1, 2.0)],
            }
        ],
        "runs": [
            {"cls": 0, "count": 3, "cp": CP_DEFAULT, "pol": 0, "phase": 0}
        ],
    }


def _variants():
    """Each one differs from the base layout in exactly one thing."""

    def edit(fn):
        lay = _base_layout()
        fn(lay)
        return lay

    c = lambda lay: lay["classes"][0]  # noqa: E731
    r = lambda lay: lay["runs"][0]  # noqa: E731
    return {
        "scale": edit(lambda L: c(L).update(scale=1.5)),
        "cp": edit(lambda L: c(L).update(cp=3)),
        "useful": edit(lambda L: c(L).update(useful=9)),
        "n_phase": edit(lambda L: c(L).update(n_phase=3)),
        "map": edit(lambda L: c(L)["map"].__setitem__(1, 0)),
        "gain": edit(lambda L: c(L)["gain"].__setitem__(2, 9)),
        "no gain": edit(lambda L: c(L).update(gain=np.zeros(0, np.complex64))),
        "polarity": edit(lambda L: c(L)["polarity"].__setitem__(1, 3)),
        "no polarity": edit(
            lambda L: c(L).update(polarity=np.zeros(0, np.complex64))
        ),
        "fixed value": edit(
            lambda L: c(L).update(fixed=[(0, 3, 2j), (ALL_PHASES, -1, 2.0)])
        ),
        "fixed phase": edit(
            lambda L: c(L).update(fixed=[(1, 3, 1j), (ALL_PHASES, -1, 2.0)])
        ),
        "fixed k": edit(
            lambda L: c(L).update(fixed=[(0, 2, 1j), (ALL_PHASES, -1, 2.0)])
        ),
        "extra fixed": edit(lambda L: c(L)["fixed"].append((1, -3, 1.0))),
        "run count": edit(lambda L: r(L).update(count=4)),
        "run cp": edit(lambda L: r(L).update(cp=1)),
        "run polarity offset": edit(lambda L: r(L).update(pol=1)),
        "run first phase": edit(lambda L: r(L).update(phase=1)),
        "extra run": edit(lambda L: L["runs"].append(dict(r(L)))),
    }


@pytest.mark.parametrize("name", sorted(_variants()))
def test_every_part_of_the_layout_is_a_reject_key(name):
    a = build(_base_layout())
    blob = a.get_state()
    variant = _variants()[name]
    with pytest.raises(ValueError):
        build(variant).set_state(blob)
    build(_base_layout()).set_state(blob)  # the layout itself is accepted


def test_builders_refuse_with_value_error():
    o = Ofdm()
    m = np.ones(8, np.uint8)
    with pytest.raises(ValueError):
        o.add_class(8, m[:7])
    with pytest.raises(ValueError):
        o.add_class(8, m, cyclic_prefix=9)
    c = o.add_class(8, m, n_phase=2)
    with pytest.raises(ValueError):
        o.add_subcarrier_gain(c, np.ones(7, np.complex64))
    with pytest.raises(ValueError):
        o.add_polarity(c, np.zeros(0, np.complex64))
    with pytest.raises(ValueError):
        o.add_fixed(c, 2, 0, 1.0)
    with pytest.raises(ValueError):
        o.add_fixed(c, 0, 4, 1.0)
    with pytest.raises(ValueError):
        o.add_run(c + 1, 1)
    with pytest.raises(ValueError, match="no run"):
        o.check()
    o.add_run(c, 1)
    o.check()  # the minimal pair: a run, and it is usable
    assert o.add_run(c, 1) == 1  # a checked layout is re-opened, not refused
    o.check()


def _small():
    m = np.zeros(8, np.uint8)
    m[[1, 2, 3, 5, 6, 7]] = 1
    o = Ofdm()
    return o, m


def test_check_names_a_window_longer_than_the_symbol():
    o, m = _small()
    c = o.add_class(8, m, useful=4, cyclic_prefix=2, window=6)
    o.add_run(c, 1, cyclic_prefix=0)  # a 4-sample symbol under a 6 window
    with pytest.raises(ValueError, match="window is longer"):
        o.check()
    ok, m = _small()  # the pair: the same run on an 8-sample body
    c = ok.add_class(8, m, useful=8, cyclic_prefix=2, window=6)
    ok.add_run(c, 1, cyclic_prefix=0)
    ok.check()


def test_check_names_two_fixed_entries_on_one_subcarrier():
    o, m = _small()
    c = o.add_class(8, m, n_phase=2)
    o.add_fixed(c, 0xFFFFFFFF, 2, 1.0)
    o.add_fixed(c, 1, 2, 1.0)
    o.add_run(c, 1)
    with pytest.raises(ValueError, match="one subcarrier"):
        o.check()
    ok, m = _small()  # the pair: the second entry on another subcarrier
    c = ok.add_class(8, m, n_phase=2)
    ok.add_fixed(c, 0xFFFFFFFF, 2, 1.0)
    ok.add_fixed(c, 1, 3, 1.0)
    ok.add_run(c, 1)
    ok.check()


def test_modulate_on_an_unusable_layout_raises_the_same_sentence_check_does():
    o, m = _small()
    o.add_class(8, m)  # no run
    with pytest.raises(ValueError, match="no run"):
        o.modulate(np.ones(6, np.complex64))
    with pytest.raises(ValueError, match="no run"):
        o.check()


def test_a_refused_call_raises_its_reason_and_moves_nothing():
    o = Ofdm()
    m = np.zeros(8, np.uint8)
    m[[1, 2, 3, 5, 6, 7]] = 1
    o.add_class(8, m, cyclic_prefix=2)
    o.add_run(0, 3)
    assert o.points_remaining() == 18
    with pytest.raises(ValueError, match="inside an OFDM symbol"):
        o.modulate(np.ones(5, np.complex64))
    with pytest.raises(ValueError, match="more points than the layout"):
        o.modulate(np.ones(24, np.complex64))
    with pytest.raises(ValueError, match="need >="):
        o.modulate(np.ones(12, np.complex64), out=np.empty(3, np.complex64))
    assert o.points_remaining() == 18
    assert o.modulate(np.ones(12, np.complex64)).size == 20
    assert o.points_remaining() == 6


def test_check_leaves_nothing_a_later_call_or_a_blob_can_see():
    """Sealing is derived state, recomputed on first use, so it is not state.

    `check()` raises or returns None and seals the layout; every method that
    needs the seal calls the check itself. If sealing changed what a later
    call does, the flag would belong in the blob. It does not: a layout that
    was checked first has the same blob, the same render and the same window
    tail as one that never was, and a refused check changes nothing.
    """
    a, m = _small()
    a.add_class(8, m, cyclic_prefix=2, window=2)
    a.add_run(0, 3)
    b, m = _small()
    b.add_class(8, m, cyclic_prefix=2, window=2)
    b.add_run(0, 3)
    assert b.check() is None
    assert a.get_state() == b.get_state()
    pts = (np.arange(a.points_remaining()) + 1j).astype(np.complex64)
    assert np.array_equal(a.modulate(pts), b.modulate(pts))
    assert np.array_equal(a.finish(), b.finish())
    assert a.get_state() == b.get_state()

    c, m = _small()
    c.add_class(8, m)  # no run: refused, twice, the same way
    before = c.get_state()
    for _ in range(2):
        with pytest.raises(ValueError, match="no run"):
            c.check()
    assert c.get_state() == before


def test_a_layout_extended_mid_stream_renders_as_if_built_whole():
    m = np.zeros(8, np.uint8)
    m[[1, 2, 3, 5, 6, 7]] = 1
    pts = (np.arange(30) + 1j * np.arange(30)).astype(np.complex64)

    def make(runs):
        o = Ofdm()
        o.add_class(8, m, cyclic_prefix=2, window=3)
        for n in runs:
            o.add_run(0, n)
        return o

    whole = make([3, 2])
    one = whole.modulate(pts)
    grown = make([3])
    a = grown.modulate(pts[:18])
    grown.add_run(0, 2)
    assert grown.points_remaining() == 12
    b = grown.modulate(pts[18:])
    assert np.array_equal(np.concatenate([a, b]), one)
    assert np.array_equal(grown.finish(), whole.finish())


def test_reset_and_context_manager():
    m = np.zeros(8, np.uint8)
    m[[1, 2, 3]] = 1
    with Ofdm() as o:
        o.add_class(8, m, cyclic_prefix=2, window=2)
        o.add_run(0, 2)
        pts = np.arange(6, dtype=np.complex64) + 1
        first = o.modulate(pts)
        o.reset()
        assert np.array_equal(o.modulate(pts), first)


def test_destroy():
    o = Ofdm()
    o.destroy()
