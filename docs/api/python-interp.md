# Python Interp API

Periodic table lookup with interpolation, backed by
`interp_table_core.c`. Evaluates a complex128 table at arbitrary
(fractional) points, wrapping the index modulo the table length so the
table is treated as one period of a repeating waveform. Purely a function
of `(table, method, point)` — no running state.

Source:
[`src/doppler/interp/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/interp/__init__.py)

______________________________________________________________________

## How it works

The table holds one period of `n` complex samples. A query `point` selects
a sample by index; fractional indices are resolved by the configured
`method`:

- **`floor`** — `table[floor(point)]`, no interpolation.
- **`nearest`** — `table[round(point)]`.
- **`linear`** — linear blend of the two bracketing samples.

Indices wrap modulo `n`, so `point` may run past the end of the table and
land back at the start — the table is one period of a periodic signal.

______________________________________________________________________

## Examples

### Linear interpolation over a ramp

```python
from doppler.interp import InterpolatedTable
import numpy as np

ramp = InterpolatedTable(
    np.array([0.0, 1.0, 2.0], dtype=np.complex128))
ramp.execute(np.array([0.5, 1.1]))   # array([0.5+0.j, 1.1+0.j])
```

### Nearest-sample lookup

```python
from doppler.interp import InterpolatedTable
import numpy as np

t = InterpolatedTable(
    np.array([0.0, 1.0, 2.0], dtype=np.complex128), method="nearest")
t.n   # 3
```

______________________________________________________________________

::: doppler.interp.InterpolatedTable
