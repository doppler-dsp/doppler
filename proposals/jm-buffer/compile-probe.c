#define _GNU_SOURCE
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include "shim.h"
#include <numpy/arrayobject.h>
#define f32_buffer_destroy dp_f32_destroy
#include "frag.c"
