

# File interp\_table\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**interp\_table**](dir_532d1478dbb04668a5390572613675ee.md) **>** [**interp\_table\_core.h**](interp__table__core_8h.md)

[Go to the documentation of this file](interp__table__core_8h.md)


```C++

#ifndef INTERP_TABLE_CORE_H
#define INTERP_TABLE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double _Complex *table;  
    size_t          n;      
    int             method; 
  } interp_table_state_t;

  interp_table_state_t *interp_table_create (const double _Complex *table,
                                             size_t table_len, int method);

  void interp_table_destroy (interp_table_state_t *state);

  void interp_table_reset (interp_table_state_t *state);

  size_t interp_table_execute_max_out (interp_table_state_t *state);

  size_t interp_table_execute (interp_table_state_t *state, const double *in,
                               size_t n_in, double _Complex *out,
                               size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* INTERP_TABLE_CORE_H */
```


