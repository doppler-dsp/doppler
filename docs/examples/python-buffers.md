# Ring Buffers

Lock-free SPSC (single-producer, single-consumer) ring buffers backed by a
virtual-memory double-mapping, so the consumer always sees a contiguous
window across the wrap boundary.

## Producer / consumer pattern

`wait(n)` blocks until `n` samples are available and returns a zero-copy
view. Call `consume(n)` when done to release the slots back to the producer.

```python
from doppler.buffer import F64Buffer
import numpy as np
import threading

buf = F64Buffer(256)

def producer():
    data = (np.ones(256) + 2j * np.ones(256)).astype(np.complex128)
    ok = buf.write(data)          # non-blocking; False if full
    print(f"write ok: {ok}")

def consumer():
    view = buf.wait(256)          # blocks until 256 samples available
    print(f"received[:2]: {view[:2]}")
    buf.consume(256)              # release slots back to producer

t_c = threading.Thread(target=consumer)
t_p = threading.Thread(target=producer)
t_c.start()
t_p.start()
t_p.join()
t_c.join()

print(f"dropped: {buf.dropped}")
```

```text
write ok: True
received[:2]: [1.+2.j 1.+2.j]
dropped: 0
```

Start the consumer before the producer so `wait()` is already blocking when
data arrives. Always `join()` both threads — the consumer holds a view into
shared memory and must call `consume()` before the buffer can be reused.

## Buffer types

| Type        | Import           | NumPy dtype          | Min capacity | Notes                |
| ----------- | ---------------- | -------------------- | ------------ | -------------------- |
| `F32Buffer` | `doppler.buffer` | `complex64`          | 512          | CF32 IQ pairs        |
| `F64Buffer` | `doppler.buffer` | `complex128`         | 256          | CF64 IQ pairs        |
| `I16Buffer` | `doppler.buffer` | `int16, shape=(n,2)` | 1024         | col 0 = I, col 1 = Q |

!!! note "Min capacity is page-size dependent"

    These are the minima on a 4 KiB-page system (x86_64). The mmap-backed ring
    sizes to a whole page, so on 16 KiB-page systems (e.g. macOS arm64) the
    minima double — `F32Buffer` 1024, `F64Buffer` 512, `I16Buffer` 2048.

```python
from doppler.buffer import F32Buffer, I16Buffer

# F32 — half the memory footprint of F64
buf32 = F32Buffer(512)

# I16 — raw SDR output; wait() returns shape (n, 2)
buf16 = I16Buffer(1024)
```

## Overflow detection

`buf.dropped` counts samples lost to overrun since the buffer was created:

```python
buf = F64Buffer(256)
buf.write(np.zeros(256, dtype=np.complex128))
buf.write(np.zeros(256, dtype=np.complex128))  # overrun: consumer too slow
print(f"dropped: {buf.dropped}")
```

```text
dropped: 256
```
