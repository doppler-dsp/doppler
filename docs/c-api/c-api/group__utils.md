

# Group utils



[**Modules**](modules.md) **>** [**utils**](group__utils.md)










































## Public Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**dp\_get\_timestamp\_ns**](#function-dp_get_timestamp_ns) (void) <br>_Return the current wall-clock time as nanoseconds since the UNIX epoch._  |
|  size\_t | [**dp\_sample\_size**](#function-dp_sample_size) ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) type) <br>_Return the byte size of one complex sample for_ `type` _._ |
|  const char \* | [**dp\_sample\_type\_str**](#function-dp_sample_type_str) ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) type) <br>_Return a short string name for_ `type` _("CI8", "CI16", "CI32", "CF32", "CF64", "CF128")._ |
|  const char \* | [**dp\_strerror**](#function-dp_strerror) (int err) <br>_Return a human-readable description of an error code._  |




























## Public Functions Documentation




### function dp\_get\_timestamp\_ns

_Return the current wall-clock time as nanoseconds since the UNIX epoch._
```
uint64_t dp_get_timestamp_ns (
    void
)
```



Uses CLOCK\_REALTIME. Useful for timestamping samples before calling a send function, or for measuring round-trip latency.




**Returns:**

Nanoseconds since epoch.







<hr>



### function dp\_sample\_size

_Return the byte size of one complex sample for_ `type` _._
```
size_t dp_sample_size (
    dp_sample_type_t type
)
```





**Parameters:**


* `type` Sample type enum value.



**Returns:**

Byte count (e.g. 2 for CI8, 4 for CI16, 8 for CI32/CF32, 16 for CF64, 32 for CF128).







<hr>



### function dp\_sample\_type\_str

_Return a short string name for_ `type` _("CI8", "CI16", "CI32", "CF32", "CF64", "CF128")._
```
const char * dp_sample_type_str (
    dp_sample_type_t type
)
```





**Parameters:**


* `type` Sample type enum value.



**Returns:**

Statically allocated, null-terminated string.







<hr>



### function dp\_strerror

_Return a human-readable description of an error code._
```
const char * dp_strerror (
    int err
)
```





**Parameters:**


* `err` Negative error code returned by any dp\_\* function.



**Returns:**

Statically allocated, null-terminated string.







<hr>

------------------------------
