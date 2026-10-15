

# File dp\_once.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_once.h**](dp__once_8h.md)

[Go to the documentation of this file](dp__once_8h.md)


```C++

#ifndef DP_ONCE_H
#define DP_ONCE_H

#ifdef _WIN32
typedef struct
{
  void *opaque; 
} dp_once_t;
#define DP_ONCE_INIT { 0 }
#else
#include <pthread.h>
typedef struct
{
  pthread_once_t once; 
} dp_once_t;
#define DP_ONCE_INIT { PTHREAD_ONCE_INIT }
#endif

void dp_once (dp_once_t *once, void (*fn) (void));

#endif /* DP_ONCE_H */
```


