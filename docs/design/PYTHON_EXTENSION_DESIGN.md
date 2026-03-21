# Python C Extension Design: From ctypes to Native Extension

## Current Implementation Analysis (client.py)

### Architecture
**Approach:** ctypes wrapper around `libdoppler.so`

**Flow:**
```

**Key Decision: Static Linking**
- Embed libzmq.a directly into dp_stream.so
- Zero dependency installation (`pip install` → just works)
- Industry standard (NumPy, Pillow, cryptography all do this)
- See [STATIC_VS_DYNAMIC.md](STATIC_VS_DYNAMIC.md) for detailed analysis
Python → ctypes → libdoppler.so → libzmq.so
```

### Key Components

#### 1. Library Loading (`_load_lib`, `_get_lib`)
```python
# Searches multiple paths for libdoppler.so
# Lazy loads + caches via CDLL
# Sets up all function signatures via _setup_ctypes
```

**Pros:**
- No compilation needed
- Easy to debug
- Works across Python versions without recompilation

**Cons:**
- ~5-10% overhead per call (ctypes marshalling)
- Manual type checking required
- No compile-time safety
- Extra shared library dependency

#### 2. Type Mapping
```python
# Current ctypes approach:
_Header(Structure)  # mirrors C struct dp_header_t
c_void_p            # opaque dp_pub*, dp_sub* pointers
```

**Issues:**
- Buffer copies on recv (lines 495-523)
- Manual memory management (dp_sub_free_samples)
- Type conversions: Python bytes ↔ C char*, NumPy ↔ C arrays

#### 3. Critical Path (recv - lines 436-536)

**Current Flow:**
1. Python → ctypes → `dp_sub_recv()`
2. C allocates buffer
3. ctypes → Python pointer
4. `np.ctypeslib.as_array()` wraps memory
5. `.copy()` to make safe NumPy array
6. `dp_sub_free_samples()` via ctypes
7. Build Header namedtuple

**Overhead per recv:**
- 2 ctypes calls (recv + free)
- Buffer copy for safety (line 501, 509, 523)
- Python object allocation overhead
- ZMQ socket option hackery (lines 262-304) — reaches into opaque struct!

#### 4. Send Path (lines 355-405)

**Current Flow:**
1. `np.ascontiguousarray()` — may copy
2. `.ctypes.data_as(c_void_p)` — pointer extraction
3. ctypes → `dp_pub_send_cf64()`
4. Keep Python object alive during call

**Issues:**
- CI32 requires repacking (lines 383-393) — expensive!
- Type dispatch in Python (overhead)

#### 5. Timeout Handling (lines 262-304)

**HACK ALERT:** Reaches into opaque `dp_ctx` struct!
```python
# Extracts zmq_socket pointer from dp_ctx_t
# Calls zmq_setsockopt directly via ctypes
```

**Why it's bad:**
- Breaks encapsulation
- Fragile (assumes struct layout)
- Loads libzmq.so separately
- ctypes overhead for every timed recv

---

## Proposed C Extension Design

### Goals
1. **Zero-copy recv** where possible
2. **Eliminate ctypes overhead** (~5-10% per call)
3. **Bake ZMQ in** — direct API, no struct hacking
4. **Type safety** at compile time
5. **Pythonic API** — same interface, faster backend

### Architecture

```

**Key Decision: Static Linking**
- Embed libzmq.a directly into dp_stream.so
- Zero dependency installation (`pip install` → just works)
- Industry standard (NumPy, Pillow, cryptography all do this)
- See [STATIC_VS_DYNAMIC.md](STATIC_VS_DYNAMIC.md) for detailed analysis
Python → dp_stream.so (C extension) → libzmq.so
         ↓
         Embeds all stream logic in extension
         No libdoppler.so dependency
```

### Module Structure

#### File: `python/src/dp_stream.c`

```c
/* Python C extension for doppler streaming
 * Directly embeds ZMQ — no libdoppler.so dependency
 */

#include <Python.h>
#include <numpy/arrayobject.h>
#include <zmq.h>
#include <time.h>

// =========================================================================
// Type Definitions
// =========================================================================

// Mirror C types (same as current libdoppler)
typedef enum {
    DP_CI32  = 0,
    DP_CF64  = 1,
    DP_CF128 = 2
} dp_sample_type_t;

typedef struct {
    uint32_t magic;        // 0x53494753 "SIGS"
    uint32_t version;
    uint32_t sample_type;
    uint64_t sequence;
    uint64_t timestamp_ns;
    double   sample_rate;
    double   center_freq;
    uint64_t num_samples;
    uint64_t reserved[4];
} dp_header_t;

// =========================================================================
// Publisher Object
// =========================================================================

typedef struct {
    PyObject_HEAD
    void *zmq_context;
    void *zmq_socket;
    dp_sample_type_t sample_type;
    uint64_t sequence;
    int socket_type;  // ZMQ_PUB
} PublisherObject;

static PyTypeObject PublisherType;

// Constructor
static PyObject *
Publisher_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    const char *endpoint;
    int sample_type = DP_CF64;

    if (!PyArg_ParseTuple(args, "s|i", &endpoint, &sample_type))
        return NULL;

    PublisherObject *self = (PublisherObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;

    self->zmq_context = zmq_ctx_new();
    if (!self->zmq_context) {
        Py_DECREF(self);
        PyErr_SetString(PyExc_RuntimeError, "zmq_ctx_new failed");
        return NULL;
    }

    self->zmq_socket = zmq_socket(self->zmq_context, ZMQ_PUB);
    if (!self->zmq_socket) {
        zmq_ctx_destroy(self->zmq_context);
        Py_DECREF(self);
        PyErr_SetString(PyExc_RuntimeError, "zmq_socket failed");
        return NULL;
    }

    if (zmq_bind(self->zmq_socket, endpoint) != 0) {
        zmq_close(self->zmq_socket);
        zmq_ctx_destroy(self->zmq_context);
        Py_DECREF(self);
        PyErr_Format(PyExc_RuntimeError, "zmq_bind failed: %s",
                     zmq_strerror(zmq_errno()));
        return NULL;
    }

    self->sample_type = sample_type;
    self->sequence = 0;
    self->socket_type = ZMQ_PUB;

    return (PyObject *)self;
}

// Destructor
static void
Publisher_dealloc(PublisherObject *self)
{
    if (self->zmq_socket) {
        zmq_close(self->zmq_socket);
    }
    if (self->zmq_context) {
        zmq_ctx_destroy(self->zmq_context);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

// send() method — ZERO-COPY
static PyObject *
Publisher_send(PublisherObject *self, PyObject *args)
{
    PyArrayObject *arr;
    double sample_rate, center_freq;

    if (!PyArg_ParseTuple(args, "O!dd", &PyArray_Type, &arr,
                          &sample_rate, &center_freq))
        return NULL;

    // Ensure contiguous array (may be zero-copy view already)
    PyArrayObject *contig = (PyArrayObject *)PyArray_ContiguousFromAny(
        (PyObject *)arr, NPY_COMPLEX128, 1, 1);
    if (!contig) return NULL;

    npy_intp n = PyArray_SIZE(contig);

    // Build header
    dp_header_t hdr = {
        .magic = 0x53494753,
        .version = 1,
        .sample_type = self->sample_type,
        .sequence = self->sequence++,
        .timestamp_ns = dp_get_timestamp_ns(),
        .sample_rate = sample_rate,
        .center_freq = center_freq,
        .num_samples = n,
    };

    // Send header + data as multipart (ZERO-COPY on data frame)
    Py_BEGIN_ALLOW_THREADS

    zmq_msg_t msg_hdr, msg_data;
    zmq_msg_init_size(&msg_hdr, sizeof(dp_header_t));
    memcpy(zmq_msg_data(&msg_hdr), &hdr, sizeof(dp_header_t));
    zmq_msg_send(&msg_hdr, self->zmq_socket, ZMQ_SNDMORE);

    // Zero-copy: zmq takes ownership of NumPy buffer
    // (requires custom free function to decref the Python object)
    zmq_msg_init_data(&msg_data, PyArray_DATA(contig),
                      n * sizeof(double complex),
                      numpy_free_fn, contig);  // contig kept alive
    zmq_msg_send(&msg_data, self->zmq_socket, 0);

    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

// Context manager support
static PyObject *
Publisher_enter(PublisherObject *self, PyObject *args)
{
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
Publisher_exit(PublisherObject *self, PyObject *args)
{
    // close() logic here
    Py_RETURN_NONE;
}

// =========================================================================
// Subscriber Object
// =========================================================================

typedef struct {
    PyObject_HEAD
    void *zmq_context;
    void *zmq_socket;
    int timeout_ms;  // No more struct hacking!
} SubscriberObject;

static PyTypeObject SubscriberType;

static PyObject *
Subscriber_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    const char *endpoint;

    if (!PyArg_ParseTuple(args, "s", &endpoint))
        return NULL;

    SubscriberObject *self = (SubscriberObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;

    self->zmq_context = zmq_ctx_new();
    self->zmq_socket = zmq_socket(self->zmq_context, ZMQ_SUB);

    // Subscribe to all (empty topic)
    zmq_setsockopt(self->zmq_socket, ZMQ_SUBSCRIBE, "", 0);

    if (zmq_connect(self->zmq_socket, endpoint) != 0) {
        zmq_close(self->zmq_socket);
        zmq_ctx_destroy(self->zmq_context);
        Py_DECREF(self);
        PyErr_Format(PyExc_RuntimeError, "zmq_connect failed: %s",
                     zmq_strerror(zmq_errno()));
        return NULL;
    }

    self->timeout_ms = -1;  // Block by default

    return (PyObject *)self;
}

static void
Subscriber_dealloc(SubscriberObject *self)
{
    if (self->zmq_socket) zmq_close(self->zmq_socket);
    if (self->zmq_context) zmq_ctx_destroy(self->zmq_context);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

// recv() method — ZERO-COPY
static PyObject *
Subscriber_recv(SubscriberObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"timeout_ms", NULL};
    int timeout_ms = -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &timeout_ms))
        return NULL;

    // Set timeout if specified (NO STRUCT HACKING!)
    if (timeout_ms >= 0) {
        zmq_setsockopt(self->zmq_socket, ZMQ_RCVTIMEO,
                       &timeout_ms, sizeof(timeout_ms));
    }

    Py_BEGIN_ALLOW_THREADS

    zmq_msg_t msg_hdr, msg_data;
    zmq_msg_init(&msg_hdr);
    zmq_msg_init(&msg_data);

    // Receive header
    int rc1 = zmq_msg_recv(&msg_hdr, self->zmq_socket, 0);
    int rc2 = -1;
    if (rc1 > 0) {
        rc2 = zmq_msg_recv(&msg_data, self->zmq_socket, 0);
    }

    Py_END_ALLOW_THREADS

    if (rc1 < 0) {
        int err = zmq_errno();
        if (err == EAGAIN && timeout_ms >= 0) {
            PyErr_Format(PyExc_TimeoutError,
                         "recv timed out after %d ms", timeout_ms);
        } else {
            PyErr_Format(PyExc_RuntimeError, "zmq_msg_recv failed: %s",
                         zmq_strerror(err));
        }
        zmq_msg_close(&msg_hdr);
        zmq_msg_close(&msg_data);
        return NULL;
    }

    // Parse header
    dp_header_t *hdr = (dp_header_t *)zmq_msg_data(&msg_hdr);
    size_t n = hdr->num_samples;

    // ZERO-COPY: wrap ZMQ buffer in NumPy array
    // (use custom destructor to call zmq_msg_close when array is freed)
    npy_intp dims[1] = {n};
    PyObject *arr = PyArray_SimpleNewFromData(
        1, dims, NPY_COMPLEX128, zmq_msg_data(&msg_data));

    if (!arr) {
        zmq_msg_close(&msg_hdr);
        zmq_msg_close(&msg_data);
        return NULL;
    }

    // Transfer ownership: when NumPy array is freed, close zmq_msg
    // (requires custom capsule destructor)
    PyArray_SetBaseObject((PyArrayObject *)arr,
                          wrap_zmq_msg(&msg_data));

    // Build header tuple
    PyObject *header = Py_BuildValue(
        "{s:K,s:K,s:d,s:d,s:K,s:s}",
        "sequence", hdr->sequence,
        "timestamp_ns", hdr->timestamp_ns,
        "sample_rate", hdr->sample_rate,
        "center_freq", hdr->center_freq,
        "num_samples", hdr->num_samples,
        "sample_type", sample_type_str(hdr->sample_type)
    );

    zmq_msg_close(&msg_hdr);
    // msg_data will be closed when arr is freed

    return Py_BuildValue("(OO)", arr, header);
}

// =========================================================================
// Module Definition
// =========================================================================

static PyMethodDef module_methods[] = {
    {"get_timestamp_ns", (PyCFunction)dp_get_timestamp_ns_py,
     METH_NOARGS, "Get current timestamp in nanoseconds"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef dp_stream_module = {
    PyModuleDef_HEAD_INIT,
    "dp_stream",
    "Doppler streaming — native ZMQ extension",
    -1,
    module_methods
};

PyMODINIT_FUNC
PyInit_dp_stream(void)
{
    import_array();

    PyObject *m = PyModule_Create(&dp_stream_module);
    if (!m) return NULL;

    // Register types
    if (PyType_Ready(&PublisherType) < 0) return NULL;
    if (PyType_Ready(&SubscriberType) < 0) return NULL;
    // ... Push, Pull, Requester, Replier

    Py_INCREF(&PublisherType);
    PyModule_AddObject(m, "Publisher", (PyObject *)&PublisherType);

    // Constants
    PyModule_AddIntConstant(m, "CI32", DP_CI32);
    PyModule_AddIntConstant(m, "CF64", DP_CF64);
    PyModule_AddIntConstant(m, "CF128", DP_CF128);

    return m;
}
```

---

## Performance Comparison

### Current (ctypes)
```python
# recv() path:
1. Python call → ctypes marshalling      (~500 ns)
2. dp_sub_recv() in libdoppler.so      (~2-5 μs)
3. ctypes → Python conversion            (~300 ns)
4. np.ctypeslib.as_array()               (~200 ns)
5. .copy() for safety                    (n * 16 bytes — memcpy)
6. dp_sub_free_samples() ctypes call     (~500 ns)
7. Build Header namedtuple               (~1 μs)

Total overhead: ~3-5 μs + memcpy
```

### Proposed (C extension)
```python
# recv() path:
1. Python call → C extension             (~50 ns)
2. zmq_msg_recv() directly               (~2-5 μs)
3. PyArray_SimpleNewFromData (zero-copy) (~100 ns)
4. Py_BuildValue for header              (~500 ns)

Total overhead: ~3-5 μs, NO memcpy
```

**Speedup:**
- ~10x on small arrays (< 1KB) — overhead dominates
- ~2x on medium arrays (1-100 KB) — copy overhead
- Negligible on large arrays (> 1 MB) — ZMQ dominates

**Memory:**
- Current: 2x array size (C buffer + Python copy)
- Proposed: 1x array size (zero-copy view)

---

## API Compatibility

### Goal: Drop-in replacement

```python
# Current API (works unchanged):
from doppler import Publisher, Subscriber, CF64

pub = Publisher("tcp://*:5555", CF64)
pub.send(samples, sample_rate=1e6, center_freq=2.4e9)

sub = Subscriber("tcp://localhost:5555")
data, hdr = sub.recv(timeout_ms=500)
```

**Changes needed:**
- Replace `client.py` ctypes wrapper with C extension import
- Update `__init__.py`:
  ```python
  from .dp_stream import (
      Publisher, Subscriber, Push, Pull,
      Requester, Replier,
      CI32, CF64, CF128,
      get_timestamp_ns,
  )
  ```

---

## Implementation Phases

### Phase 1: Core Extension (Publisher + Subscriber)
- `python/src/dp_stream.c` — basic PUB/SUB only
- Zero-copy recv
- Context manager support
- Timeout handling (direct ZMQ API)
- Test compatibility with existing `test_pubsub.py`

### Phase 2: Complete Pattern Support
- Push / Pull
- Requester / Replier
- All sample types (CI32, CF64, CF128)
- Error handling parity

### Phase 3: Optimization
- Zero-copy send (zmq_msg_init_data with custom free)
- GIL release for all blocking operations
- Pre-allocate common structures
- Fast-path for CF64 (most common)

### Phase 4: Remove ctypes Dependency
- Delete `client.py`
- Update docs
- Benchmark comparison

---

## Key Differences from ctypes Approach

| Aspect | ctypes (current) | C Extension (proposed) |
|--------|-----------------|------------------------|
| **Dependency** | libdoppler.so | Built-in (links libzmq directly) |
| **Call overhead** | ~500 ns/call | ~50 ns/call |
| **Buffer copy** | Required for safety | Zero-copy via PyArray base object |
| **Memory** | 2x (C + Python) | 1x (shared) |
| **Timeout** | Struct hacking | Direct zmq_setsockopt() |
| **Type safety** | Runtime | Compile-time |
| **GIL handling** | Manual BEGIN/END | Automatic in C |
| **Error messages** | Indirect | Direct Python exceptions |
| **Debugging** | gdb → ctypes → C | gdb → C |

---

## Risk Analysis

### Benefits
✅ **10x faster for small messages** (< 1KB)
✅ **Zero-copy** — 50% memory reduction
✅ **No struct hacking** — robust timeout API
✅ **Type safety** — compile-time checks
✅ **Simpler deployment** — one .so instead of two

### Risks
⚠️ **More complex to maintain** — C code vs Python
⚠️ **Platform-specific builds** — need manylinux wheels
⚠️ **Debugging harder** — C extension crashes are opaque
⚠️ **NumPy ABI** — must match build-time version

### Mitigation
- Keep C code simple and well-tested
- Use CI matrix (Linux, macOS, multiple Python versions)
- Comprehensive error handling (PyErr_Format everywhere)
- Document build process clearly
- Keep ctypes version as fallback (optional)

---

## Build Integration

### CMakeLists.txt
```cmake
# python/CMakeLists.txt
add_library(dp_stream MODULE
    src/dp_stream.c
)

target_include_directories(dp_stream PRIVATE
    ${NumPy_INCLUDE_DIR}
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(dp_stream PRIVATE
    zmq          # Direct ZMQ link
    Python3::Python
    m
)

set_target_properties(dp_stream PROPERTIES
    PREFIX ""
    SUFFIX "${PY_EXT_SUFFIX}"
    OUTPUT_NAME "dp_stream"
)

# Copy to package
add_custom_command(TARGET dp_stream POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:dp_stream>
        ${PY_PKG_DIR}/dp_stream${PY_EXT_SUFFIX}
)
```

---

## Testing Strategy

### Unit Tests (existing tests should pass unchanged)
```bash
pytest python/doppler/tests/test_pubsub.py -v
# All 6 tests should pass with C extension backend
```

### Benchmark
```python
# Compare ctypes vs C extension
import time
import numpy as np
from doppler import Publisher, Subscriber

n_samples = 1024
samples = np.random.randn(n_samples) + 1j*np.random.randn(n_samples)

pub = Publisher("tcp://*:5555")
sub = Subscriber("tcp://localhost:5555")

# Warmup
for _ in range(100):
    pub.send(samples, 1e6, 2.4e9)
    sub.recv()

# Benchmark
start = time.perf_counter()
for _ in range(10000):
    pub.send(samples, 1e6, 2.4e9)
    sub.recv()
elapsed = time.perf_counter() - start

print(f"Throughput: {10000/elapsed:.1f} msg/sec")
print(f"Latency: {elapsed/10000*1e6:.1f} μs/msg")
```

**Expected Results:**
- ctypes: ~200 μs/msg @ 1K samples
- C extension: ~100 μs/msg @ 1K samples (2x faster)

---

## Conclusion

Moving from ctypes to a native C extension provides:
- **Significant performance gains** (2-10x faster)
- **Memory efficiency** (zero-copy, 50% reduction)
- **Cleaner API** (no struct hacking for timeouts)
- **Better integration** (direct ZMQ, one .so)

The trade-off is increased build complexity, but this is standard for high-performance Python libraries (NumPy, SciPy, etc.).

**Recommendation:** Implement in phases, keeping ctypes version as fallback during transition.
