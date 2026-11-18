---
description: "Render an OFDM signal of any structure from a layout of classes and runs, with no standard and no wfmgen involved, and check it by transforming it back."
---

# OFDM

[`doppler.ofdm`][doppler.ofdm] is a general OFDM modulator. It turns
constellation points into baseband samples for any OFDM signal a **layout**
can describe, and it names no standard: a standard's preset is a layout that
something else builds, and wfmgen's `ofdm` waveform type is one more caller.
[The design page](../design/ofdm.md) says why the layout has the shape it has.

A layout is a list of **classes**, each one kind of OFDM symbol, and a list of
**runs**, each a count of OFDM symbols of one class. A class says the
transform length, which subcarriers may carry a data point (the others carry
nothing), the **fixed entries** the modulator places itself (pilots, training
values) with the pattern phase each belongs to, an optional polarity sequence
that multiplies them, optional per-subcarrier gains, how many samples the
body reads from the transform, the cyclic prefix, and the window.

`Ofdm` holds its layout and is built by calls: `add_class`, then
`add_subcarrier_gain` and `add_polarity` for the class's optional arrays,
`add_fixed` for each fixed entry, and `add_run`. `modulate` renders whole OFDM
symbols from the points it is given, taking from them exactly as many as each
symbol's data subcarriers need, and `finish` returns the window's last fall.
`points_remaining` says how many points the rest of the layout consumes, so a
whole layout renders in one call.

```python
--8<-- "src/doppler/examples/ofdm_demo.py:ofdm"
```

The script goes on to check what it made: a forward FFT of each OFDM symbol's
body, with the cyclic prefix dropped, returns the grid that went in, with
every data point on its subcarrier, every pilot at its value times the
polarity, and nothing on the null subcarriers. That is the check to reach for
with any layout.

**What comes out.** `modulate` and `finish` return a one-dimensional
`complex64` array of samples, a new one on every call that the caller owns
(pass `out=` to write into a `complex64` array of your own instead; from C the
caller always supplies the buffer and sizes it with `modulate_max_out`).
The samples are the **unnormalized** inverse transform of the grid, multiplied
by the class's `scale` (1.0 by default): a grid of unit entries gives a mean
power equal to the number of occupied subcarriers (52 in the example above),
not 1, so pass `scale=1/np.sqrt(occupied)` to `add_class` for unit power.

**Refusals.** A build call that cannot be honoured raises `ValueError`,
changing nothing. A layout that is not usable (no run, a window longer than an
OFDM symbol of its class, two fixed entries on one subcarrier in one phase)
is found out when it is checked, and `check` says which: it raises
`ValueError` with a sentence (`the layout has no run`) and returns `None`
when the layout is usable. `modulate` checks on first use and refuses the
same way: it raises `ValueError` with the sentence for an unusable layout,
for a point count that ends inside an OFDM symbol, and for one that runs past
the layout, leaving the modulator untouched in every case. A call that
renders nothing, because no points remain or because zero points were given
where the next OFDM symbol needs some, returns an empty array and is not an
error. A buffer too short is never the library's own refusal in Python: it
sizes the output itself, and an `out=` shorter than `modulate_max_out` is
rejected by the binding (`out has N elements, need >= M`) before the call.
In C the same refusal is a negative return from `dp_ofdm_modulate_why()` with
a sentence. Adding to a checked layout re-opens it and keeps the position and
the window tail, so a signal can be extended while it is rendered.

The same lifecycle from C, with the caller owning both buffers and the
refusal's reason read from `dp_ofdm_check_why()`. It checks itself and exits
non-zero if anything it prints is wrong:

```c
--8<-- "native/examples/ofdm_demo.c"
```

**Resuming.** The modulator is serializable: `get_state` and `set_state`
carry the position in the layout and the window tail, and the layout is the
reject key, so a blob restores only into a modulator built with the same
layout. A call renders whole OFDM symbols, so a render splits and resumes on
an OFDM-symbol boundary and is bit for bit the same as one call.
