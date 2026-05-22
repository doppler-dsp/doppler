# NCO — Raw Phase Accumulator

`NCO` exposes the bare uint32 phase accumulator — useful when you need the
raw phase word rather than a sin/cos lookup.  `freq` is a normalised
frequency in cycles/sample.

## Raw uint32 phase

```python
from doppler.source import NCO

nco = NCO(0.25)
ph = nco.steps_u32(16)
print(ph)
```

```text
[         0 1073741824 2147483648 3221225472          0 1073741824
 2147483648 3221225472          0 1073741824 2147483648 3221225472
          0 1073741824 2147483648 3221225472]
```

The accumulator wraps at 2³² (0.25 × 2³² = 2³⁰ = 1073741824 per step).

## Overflow carry

`steps_u32_ovf` returns `(phases, carry)` — `carry[i]` is 1 whenever the
accumulator wrapped on that sample:

```python
nco = NCO(0.25)
phases, carry = nco.steps_u32_ovf(16)
print(carry)
```

```text
[0 0 0 1 0 0 0 1 0 0 0 1 0 0 0 1]
```

The carry fires at indices 3, 7, 11, 15 — once every full cycle.

## Scaled output

Scale the phase into `[0, nmax)` with a fixed-point multiply (no division):

```python
nco2 = NCO(0.25, nmax=1000)
scaled = nco2.steps_u32_scaled(16)
print(scaled)
```

```text
[  0 250 500 750   0 250 500 750   0 250 500 750   0 250 500 750]
```
