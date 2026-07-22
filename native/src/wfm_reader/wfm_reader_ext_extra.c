/*
 * wfm_reader_ext_extra.c — hand-written, Python-aware helpers for the Reader.
 *
 * jm wires this in when it exists (gh-543) but never creates or modifies it;
 * it is #included by wfm_reader_ext.c after the generated fragment, so the
 * generated `.keywords` getter forward-declares wfm_reader_keyword_value()
 * above itself and this file provides the body.
 *
 * value_fn for the `.keywords` dict property: build the Python value of the
 * i'th BLUE extended-header keyword. It lives here rather than in the pure-C
 * core because the value's type is data-dependent — it comes from the
 * keyword's own type code in the file — so the function returns a PyObject *
 * directly (value_type = "object"), which needs Python.h that a DSP core must
 * not pull in.
 */

#include "wfm/wfm_keywords.h"
#include "wfm_reader/wfm_reader_core.h"

/*
 * The value of the i'th keyword, as a new reference (or NULL with an exception
 * set). jm's generated dict loop only calls this for 0 <= i < num_keywords, so
 * the keyword pointer is never NULL.
 *
 * The Python type follows the keyword's type code: `A` is a str (the wire form
 * carries no NUL, so the length is explicit); the integer and float codes give
 * an int/float when the keyword holds one element and a list when it holds
 * several — which is what almost every real keyword holds one of.
 */
PyObject *
wfm_reader_keyword_value (const wfm_reader_state_t *state, size_t i)
{
  const wfm_keyword_t *kw = wfm_reader_keyword (state, i);

  if (kw->type == 'A')
    return PyUnicode_FromStringAndSize ((const char *)kw->value,
                                        (Py_ssize_t)kw->count);

  /* Build the elements first, then collapse to a scalar when there is
     exactly one. */
  PyObject *list = PyList_New ((Py_ssize_t)kw->count);
  if (!list)
    return NULL;
  for (size_t k = 0; k < kw->count; k++)
    {
      const uint8_t *p    = kw->value + k * kw->elem_size;
      PyObject      *item = NULL;
      switch (kw->type)
        {
        case 'B':
          item = PyLong_FromLong ((long)*(const int8_t *)p);
          break;
        case 'I':
          {
            int16_t v;
            memcpy (&v, p, sizeof v);
            item = PyLong_FromLong ((long)v);
            break;
          }
        case 'L':
        case 'T': /* deprecated spelling of a 32-bit integer */
          {
            int32_t v;
            memcpy (&v, p, sizeof v);
            item = PyLong_FromLong ((long)v);
            break;
          }
        case 'X':
          {
            int64_t v;
            memcpy (&v, p, sizeof v);
            item = PyLong_FromLongLong ((long long)v);
            break;
          }
        case 'F':
          {
            float v;
            memcpy (&v, p, sizeof v);
            item = PyFloat_FromDouble ((double)v);
            break;
          }
        default: /* 'D' — the decoder admits no other type */
          {
            double v;
            memcpy (&v, p, sizeof v);
            item = PyFloat_FromDouble (v);
            break;
          }
        }
      if (!item)
        {
          Py_DECREF (list);
          return NULL;
        }
      PyList_SET_ITEM (list, (Py_ssize_t)k, item); /* steals item */
    }
  if (kw->count == 1)
    {
      PyObject *scalar = PyList_GET_ITEM (list, 0);
      Py_INCREF (scalar);
      Py_DECREF (list);
      return scalar;
    }
  return list;
}
