

# Group errors



[**Modules**](modules.md) **>** [**errors**](group__errors.md)



[More...](#detailed-description)

































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_ERR\_INIT**](group__errors.md#define-dp_err_init)  `-1`<br> |
| define  | [**DP\_ERR\_INVALID**](group__errors.md#define-dp_err_invalid)  `-4`<br> |
| define  | [**DP\_ERR\_MEMORY**](group__errors.md#define-dp_err_memory)  `-6`<br> |
| define  | [**DP\_ERR\_RECV**](group__errors.md#define-dp_err_recv)  `-3`<br> |
| define  | [**DP\_ERR\_SEND**](group__errors.md#define-dp_err_send)  `-2`<br> |
| define  | [**DP\_ERR\_TIMEOUT**](group__errors.md#define-dp_err_timeout)  `-5`<br> |
| define  | [**DP\_OK**](group__errors.md#define-dp_ok)  `0`<br> |

## Detailed Description


Every send/recv function returns one of these values. Use [**dp\_strerror()**](group__utils.md#function-dp_strerror) to obtain a human-readable description. 


    
## Macro Definition Documentation





### define DP\_ERR\_INIT 

```
#define DP_ERR_INIT `-1`
```



Initialisation failed (ZMQ context/socket). 


        

<hr>



### define DP\_ERR\_INVALID 

```
#define DP_ERR_INVALID `-4`
```



Invalid argument. 


        

<hr>



### define DP\_ERR\_MEMORY 

```
#define DP_ERR_MEMORY `-6`
```



Memory allocation failure. 


        

<hr>



### define DP\_ERR\_RECV 

```
#define DP_ERR_RECV `-3`
```



Receive failed or timed out (EAGAIN). 


        

<hr>



### define DP\_ERR\_SEND 

```
#define DP_ERR_SEND `-2`
```



Send failed. 


        

<hr>



### define DP\_ERR\_TIMEOUT 

```
#define DP_ERR_TIMEOUT `-5`
```



Operation timed out. 


        

<hr>



### define DP\_OK 

```
#define DP_OK `0`
```



Success. 


        

<hr>

------------------------------


