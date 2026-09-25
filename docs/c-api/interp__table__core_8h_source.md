

# File interp\_table\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**interp\_table**](dir_ff54084ce651244803741a1f6d284d09.md) **>** [**interp\_table\_core.h**](interp__table__core_8h.md)

[Go to the documentation of this file](interp__table__core_8h.md)


```C++

#ifndef INTERP_TABLE_CORE_H
#define INTERP_TABLE_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
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


