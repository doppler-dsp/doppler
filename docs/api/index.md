---
description: "Where the API reference lives: the C headers rendered by Doxygen, and the Python reference generated from each package's __all__."
---

# API Reference

## C API

The C layer is the source of truth: every algorithm is implemented here once.

- [**C API Reference**](../c-api/index.md): file list, data structures and
    the function index, rendered from the headers' Doxygen.

## Python API

The Python modules are thin CPython extensions over the C ABI, with no
reimplementation.

- [**Python API**][doppler]: every public class, function and constant, one
    page per package, generated from the package's `__all__`. Each package's
    page opens with its overview, and each entry lists the guide and gallery
    pages that use it.

How to use each feature, with tested code, is in the
[Guides](../guide/index.md).

## Also here

- [Type System](../types.md): the C99 types every API takes and returns,
    complex samples included.
- [Memory Management](../memory_management.md): who allocates a buffer,
    who owns it and how long it stays valid, in C and in Python.
