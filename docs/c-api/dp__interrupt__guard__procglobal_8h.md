

# File dp\_interrupt\_guard\_procglobal.h



[**FileList**](files.md) **>** [**dp\_interrupt\_guard**](dir_001936014fd0d8bf32545bf8d71a57c6.md) **>** [**dp\_interrupt\_guard\_procglobal.h**](dp__interrupt__guard__procglobal_8h.md)

[Go to the source code of this file](dp__interrupt__guard__procglobal_8h_source.md)








































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_interrupt\_guard\_state\_adopt**](#function-dp_interrupt_guard_state_adopt) (void \* shared) <br> |
|  void \* | [**dp\_interrupt\_guard\_state\_ptr**](#function-dp_interrupt_guard_state_ptr) (void) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_INTERRUPT\_GUARD\_PG\_ATTR**](dp__interrupt__guard__procglobal_8h.md#define-dp_interrupt_guard_pg_attr)  `"\_jm\_pg\_dp\_interrupt\_guard"`<br> |
| define  | [**DP\_INTERRUPT\_GUARD\_PG\_CAPSULE**](dp__interrupt__guard__procglobal_8h.md#define-dp_interrupt_guard_pg_capsule)  `"doppler.dp\_interrupt\_guard.\_jm\_procglobal"`<br> |
| define  | [**DP\_INTERRUPT\_GUARD\_PG\_OWNER**](dp__interrupt__guard__procglobal_8h.md#define-dp_interrupt_guard_pg_owner)  `"doppler.interrupt.interrupt"`<br> |

## Public Functions Documentation




### function dp\_interrupt\_guard\_state\_adopt 

```C++
void dp_interrupt_guard_state_adopt (
    void * shared
) 
```




<hr>



### function dp\_interrupt\_guard\_state\_ptr 

```C++
void * dp_interrupt_guard_state_ptr (
    void
) 
```




<hr>
## Macro Definition Documentation





### define DP\_INTERRUPT\_GUARD\_PG\_ATTR 

```C++
#define DP_INTERRUPT_GUARD_PG_ATTR `"_jm_pg_dp_interrupt_guard"`
```




<hr>



### define DP\_INTERRUPT\_GUARD\_PG\_CAPSULE 

```C++
#define DP_INTERRUPT_GUARD_PG_CAPSULE `"doppler.dp_interrupt_guard._jm_procglobal"`
```




<hr>



### define DP\_INTERRUPT\_GUARD\_PG\_OWNER 

```C++
#define DP_INTERRUPT_GUARD_PG_OWNER `"doppler.interrupt.interrupt"`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_interrupt_guard/dp_interrupt_guard_procglobal.h`

