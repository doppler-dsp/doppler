

# File core.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**core.h**](core_8h.md)

[Go to the documentation of this file](core_8h.md)


```C++


#ifndef DP_CORE_H
#define DP_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

  int dp_init (void);

  void dp_cleanup (void);

  const char *dp_version (void);

  const char *dp_build_info (void);

#ifdef __cplusplus
}
#endif

#endif /* DP_CORE_H */
```
