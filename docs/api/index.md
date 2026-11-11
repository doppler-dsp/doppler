# API Reference

## C API

The C layer is the source of truth — every algorithm is implemented here once.

- [**C API Reference**](../c-api/index.md) — file list, data structures, function index

## Python reference

- [**Python API**][doppler] — every public class, function and constant, one page per module, generated from the package's `__all__`

## Python API

The Python modules are thin CPython extensions over the C ABI — no reimplementation.

| Module                                     | Page                                           |
| ------------------------------------------ | ---------------------------------------------- |
| `doppler.wfm` — waveform generator         | [Python: Waveform Generator](python-wfmgen.md) |
| `doppler.track` — second-order loop filter | [Python: Loop Filter (track)](python-track.md) |
