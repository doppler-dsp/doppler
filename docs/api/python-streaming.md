# Python Streaming API

Python bindings for the `dp_*` C streaming library via zero-copy C extension.
All socket types support the context manager protocol and clean up automatically
on garbage collection.

Source: [`python/src/dp_stream.c`](https://github.com/hunter-dsp/doppler/blob/main/python/src/dp_stream.c)

---

## Constants

Sample type constants for specifying wire format:

- **`CI32`** (int) - Complex int32: `int32_t` I, `int32_t` Q (8 bytes/sample)
- **`CF64`** (int) - Complex float64: `double` I, `double` Q (16 bytes/sample)
- **`CF128`** (int) - Complex float128: `long double` I, `long double` Q (32 bytes/sample)

---

## Socket Classes

### `Publisher(endpoint: str, sample_type: int)`

ZMQ PUB socket for broadcasting signal samples.

**Parameters:**
- `endpoint` (str): ZMQ endpoint to bind to (e.g., `"tcp://*:5555"`)
- `sample_type` (int): One of `CI32`, `CF64`, `CF128`

**Methods:**
- `send(samples, sample_rate, center_freq)` - Send samples with metadata
  - `samples`: NumPy array (complex64/complex128/complex256)
  - `sample_rate`: Sample rate in Hz
  - `center_freq`: Center frequency in Hz
- `close()` - Clean up resources

**Context Manager:** Yes (`with Publisher(...) as pub:`)

---

### `Subscriber(endpoint: str)`

ZMQ SUB socket for receiving signal samples.

**Parameters:**
- `endpoint` (str): ZMQ endpoint to connect to (e.g., `"tcp://localhost:5555"`)

**Methods:**
- `recv(timeout_ms=-1)` - Receive samples (blocks until data arrives or timeout)
  - `timeout_ms`: Timeout in milliseconds (-1 = infinite)
  - **Returns**: `(samples, header)` tuple
    - `samples`: NumPy array (zero-copy view into ZMQ buffer!)
    - `header`: dict with keys `sequence`, `timestamp_ns`, `sample_rate`, `center_freq`, `num_samples`, `sample_type`
  - **Raises**: `TimeoutError` if timeout expires
- `close()` - Clean up resources

**Context Manager:** Yes (`with Subscriber(...) as sub:`)

---

## Utilities

### `get_timestamp_ns() -> int`

Get current timestamp in nanoseconds since Unix epoch.

**Returns:** int - Nanoseconds since 1970-01-01 00:00:00 UTC
