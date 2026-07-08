

# File dp\_state\_pyhelp.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md)

[Go to the source code of this file](dp__state__pyhelp_8h_source.md)



































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_PY\_STATE\_METHODS**](dp__state__pyhelp_8h.md#define-dp_py_state_methods) (FNPFX, SELF\_T, HANDLE\_EXPR, CPFX) <br> |

## Macro Definition Documentation





### define DP\_PY\_STATE\_METHODS 

```C++
#define DP_PY_STATE_METHODS (
    FNPFX,
    SELF_T,
    HANDLE_EXPR,
    CPFX
) 
```



[**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md) — the state bytes interface for hand-written (no\_generate) CPython bindings.


jm's `serializable` flag generates the Python triplet (state\_bytes / get\_state / set\_state) for every jm-managed object. A `no_generate` module (e.g. `ddc_fn`) owns its binding by hand, so jm cannot generate it there — this macro provides the _same_ three methods so the hand-written face is byte-identical in behaviour and cannot drift from jm's output.


Include from a `*_ext.c` translation unit (after Python.h). Instantiate with the PyObject struct type, an expression yielding the live C handle (or NULL when closed/destroyed), and the C object prefix, e.g.:


DP\_PY\_STATE\_METHODS(Ddcr, DdcrObject, (self-&gt;closed ? NULL : self-&gt;h), ddcr)


then add to the type's PyMethodDef table:


{"state\_bytes", (PyCFunction)Ddcr\_state\_bytes, METH\_NOARGS, "Serialized state size in bytes."}, {"get\_state", (PyCFunction)Ddcr\_get\_state, METH\_NOARGS, "Serialize the engine's mutable state to bytes."}, {"set\_state", (PyCFunction)Ddcr\_set\_state, METH\_O, "Restore mutable state from a get\_state() blob."},


The C triplet (`<pfx>_state_bytes/get_state/set_state`) must follow the standard ABI (see [**dp\_state.h**](dp__state_8h.md)): set\_state returns DP\_OK (0) or a negative error. set\_state here enforces an exact size match, mirroring jm. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_state_pyhelp.h`

