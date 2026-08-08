/*
 * tlm_read_dict.h — the marshalling behind read_dict(), once.
 *
 * Hand-written, and shared by two sacred fragments: Telemetry.read_dict()
 * (telemetry_ext_dp_tlm.c) and MemoryCapture.read_dict()
 * (telemetry_ext_dp_tlm_capture.c). They differ only in where the records
 * come from — a drain of the ring, or the capture's accumulator — so
 * everything after that is here rather than in both.
 *
 * It lives in a header because jm splits a module's binding into per-object
 * `_ext_<obj>.c` fragments with no sanctioned home for hand-written C shared
 * BETWEEN them. All the fragments are #included into one translation unit
 * (telemetry_ext.c), so an include-guarded static definition here is compiled
 * exactly once and is in scope for every caller.
 *
 * The regrouping itself is NOT here: dp_tlm_demux() in dp_tlm_core.c does it,
 * so a C consumer reading a .tlm16 off disk gets the identical split. This is
 * the Python marshalling only, and it is hand-written because the shape has no
 * manifest spelling — jm's `dict` property binds scalar values, not arrays.
 */
#ifndef DP_TLM_READ_DICT_H
#define DP_TLM_READ_DICT_H

/**
 * @brief Builds `{probe_name: values}` (or `{name: (n, values)}`) from
 * records.
 *
 * @param tlm      Context whose registry names the probes.
 * @param recs     Records to split; borrowed, only read.
 * @param n        Records in @p recs.
 * @param with_idx Non-zero to pair each value array with its sample indices.
 * @return New reference to the dict, or NULL with an exception set.
 */
static PyObject *
tlm_build_read_dict (dp_tlm_t *tlm, const dp_tlm_rec_t *recs, size_t n,
                     int with_idx)
{
  float    *values[DP_TLM_MAX_PROBES];
  uint64_t *index[DP_TLM_MAX_PROBES];
  size_t    caps[DP_TLM_MAX_PROBES];
  size_t    nprobes = dp_tlm_probe_count (tlm);

  /* The destination tables are fixed at DP_TLM_MAX_PROBES, and the counting
     and filling passes must agree on the SAME bound or the pair goes wrong in
     a way neither half can see: dp_tlm_demux clamps to the array maximum while
     dp_tlm_demux_counts writes as many counts as it is given, so a larger
     nprobes would size arrays that the fill then never writes — and
     PyArray_SimpleNew does not zero, so that is uninitialized memory reaching
     Python. The registry cannot exceed the maximum today; clamping here is
     what keeps that from being load-bearing at a distance. */
  if (nprobes > DP_TLM_MAX_PROBES)
    nprobes = DP_TLM_MAX_PROBES;

  dp_tlm_demux_counts (recs, n, caps, nprobes);

  PyObject *out = PyDict_New ();
  if (!out)
    return NULL;

  /* Every REGISTERED probe gets a key, including one that emitted nothing:
     a caller plotting per probe wants a stable key set, and an absent key
     would read as "no such probe" rather than "nothing yet". */
  for (size_t i = 0; i < nprobes; i++)
    {
      const char *name = dp_tlm_probe_name (tlm, i);
      if (!name)
        {
          PyErr_Format (PyExc_RuntimeError,
                        "read_dict: dp_tlm_probe_name returned NULL at %zu",
                        i);
          goto fail;
        }

      /* Two probes sharing a name would make the second insert REPLACE the
         first, dropping the dict's only reference to an array that values[]
         still points into — a use-after-free the moment dp_tlm_demux writes
         through it. dp_tlm_probe() is idempotent by name so the registry
         cannot hold two, but that invariant lives in another module and
         nothing local would notice if it moved. Built once and reused for
         the insert, so this costs a lookup rather than a second interning. */
      PyObject *key = PyUnicode_FromString (name);
      if (!key)
        goto fail;
      int dup = PyDict_Contains (out, key);
      if (dup != 0)
        {
          if (dup > 0)
            PyErr_Format (PyExc_RuntimeError,
                          "read_dict: two probes are both named '%s'", name);
          Py_DECREF (key);
          goto fail;
        }

      npy_intp  dim = (npy_intp)caps[i];
      PyObject *v   = PyArray_SimpleNew (1, &dim, NPY_FLOAT);
      if (!v)
        {
          Py_DECREF (key);
          goto fail;
        }
      values[i] = (float *)PyArray_DATA ((PyArrayObject *)v);
      index[i]  = NULL;

      PyObject *entry = v; /* the dict takes ownership below */
      if (with_idx)
        {
          PyObject *nn = PyArray_SimpleNew (1, &dim, NPY_UINT64);
          if (!nn)
            {
              Py_DECREF (v);
              Py_DECREF (key);
              goto fail;
            }
          index[i] = (uint64_t *)PyArray_DATA ((PyArrayObject *)nn);
          /* (n, values) — index first, so `for name, (n, v) in ...` reads in
             the same order as a plot's (x, y). */
          entry = PyTuple_Pack (2, nn, v);
          Py_DECREF (nn);
          Py_DECREF (v);
          if (!entry)
            {
              Py_DECREF (key);
              goto fail;
            }
        }
      int rc = PyDict_SetItem (out, key, entry);
      Py_DECREF (key);
      Py_DECREF (entry);
      if (rc < 0)
        goto fail;
    }

  /* The dict owns every buffer above, so the pointers stay valid here. */
  dp_tlm_demux (recs, n, values, with_idx ? index : NULL, caps, nprobes);
  return out;

fail:
  Py_DECREF (out);
  return NULL;
}

#endif /* DP_TLM_READ_DICT_H */
