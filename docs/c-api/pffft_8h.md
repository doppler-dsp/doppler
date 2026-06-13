

# File pffft.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**pffft**](dir_2e0e79537247ed1eb65cd0be05071701.md) **>** [**pffft.h**](pffft_8h.md)

[Go to the source code of this file](pffft_8h_source.md)



* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct [**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) | [**PFFFT\_Setup**](#typedef-pffft_setup)  <br> |
| enum  | [**pffft\_direction\_t**](#enum-pffft_direction_t)  <br> |
| enum  | [**pffft\_transform\_t**](#enum-pffft_transform_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**pffft\_aligned\_free**](#function-pffft_aligned_free) (void \*) <br> |
|  void \* | [**pffft\_aligned\_malloc**](#function-pffft_aligned_malloc) (size\_t nb\_bytes) <br> |
|  void | [**pffft\_destroy\_setup**](#function-pffft_destroy_setup) ([**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) \*) <br> |
|  [**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) \* | [**pffft\_new\_setup**](#function-pffft_new_setup) (int N, [**pffft\_transform\_t**](pffft_8h.md#enum-pffft_transform_t) transform) <br> |
|  int | [**pffft\_simd\_size**](#function-pffft_simd_size) (void) <br> |
|  void | [**pffft\_transform**](#function-pffft_transform) ([**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) \* setup, const float \* input, float \* output, float \* work, [**pffft\_direction\_t**](pffft_8h.md#enum-pffft_direction_t) direction) <br> |
|  void | [**pffft\_transform\_ordered**](#function-pffft_transform_ordered) ([**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) \* setup, const float \* input, float \* output, float \* work, [**pffft\_direction\_t**](pffft_8h.md#enum-pffft_direction_t) direction) <br> |
|  void | [**pffft\_zconvolve\_accumulate**](#function-pffft_zconvolve_accumulate) ([**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) \* setup, const float \* dft\_a, const float \* dft\_b, float \* dft\_ab, float scaling) <br> |
|  void | [**pffft\_zreorder**](#function-pffft_zreorder) ([**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) \* setup, const float \* input, float \* output, [**pffft\_direction\_t**](pffft_8h.md#enum-pffft_direction_t) direction) <br> |




























## Public Types Documentation




### typedef PFFFT\_Setup 

```C++
typedef struct PFFFT_Setup PFFFT_Setup;
```



Opaque struct holding internal stuff (precomputed twiddle factors) this struct can be shared by many threads as it contains only read-only data. 


        

<hr>



### enum pffft\_direction\_t 

```C++
enum pffft_direction_t {
    PFFFT_FORWARD,
    PFFFT_BACKWARD
};
```



Direction of the transform 


        

<hr>



### enum pffft\_transform\_t 

```C++
enum pffft_transform_t {
    PFFFT_REAL,
    PFFFT_COMPLEX
};
```



Type of transform 


        

<hr>
## Public Functions Documentation




### function pffft\_aligned\_free 

```C++
void pffft_aligned_free (
    void *
) 
```




<hr>



### function pffft\_aligned\_malloc 

```C++
void * pffft_aligned_malloc (
    size_t nb_bytes
) 
```



the float buffers must have the correct alignment (16-byte boundary on intel and powerpc). This function may be used to obtain such correctly aligned buffers. 


        

<hr>



### function pffft\_destroy\_setup 

```C++
void pffft_destroy_setup (
    PFFFT_Setup *
) 
```




<hr>



### function pffft\_new\_setup 

```C++
PFFFT_Setup * pffft_new_setup (
    int N,
    pffft_transform_t transform
) 
```



Prepare for performing transforms of size N  the returned [**PFFFT\_Setup**](pffft_8h.md#typedef-pffft_setup) structure is read-only so it can safely be shared by multiple concurrent threads.


Will return NULL if N is not suitable (too large / no decomposable with simple integer factors..) 


        

<hr>



### function pffft\_simd\_size 

```C++
int pffft_simd_size (
    void
) 
```



return 4 or 1 wether support SSE/Altivec instructions was enable when building pffft.c 


        

<hr>



### function pffft\_transform 

```C++
void pffft_transform (
    PFFFT_Setup * setup,
    const float * input,
    float * output,
    float * work,
    pffft_direction_t direction
) 
```



Perform a Fourier transform , The z-domain data is stored in the most efficient order for transforming it back, or using it for convolution. If you need to have its content sorted in the "usual" way, that is as an array of interleaved complex numbers, either use pffft\_transform\_ordered , or call pffft\_zreorder after the forward fft, and before the backward fft.


Transforms are not scaled: PFFFT\_BACKWARD(PFFFT\_FORWARD(x)) = N\*x. Typically you will want to scale the backward transform by 1/N.


The 'work' pointer should point to an area of N (2\*N for complex fft) floats, properly aligned. If 'work' is NULL, then stack will be used instead (this is probably the best strategy for small FFTs, say for N &lt; 16384).


input and output may alias. 


        

<hr>



### function pffft\_transform\_ordered 

```C++
void pffft_transform_ordered (
    PFFFT_Setup * setup,
    const float * input,
    float * output,
    float * work,
    pffft_direction_t direction
) 
```



Similar to pffft\_transform, but makes sure that the output is ordered as expected (interleaved complex numbers). This is similar to calling pffft\_transform and then pffft\_zreorder.


input and output may alias. 


        

<hr>



### function pffft\_zconvolve\_accumulate 

```C++
void pffft_zconvolve_accumulate (
    PFFFT_Setup * setup,
    const float * dft_a,
    const float * dft_b,
    float * dft_ab,
    float scaling
) 
```



Perform a multiplication of the frequency components of dft\_a and dft\_b and accumulate them into dft\_ab. The arrays should have been obtained with pffft\_transform(.., PFFFT\_FORWARD) and should not\* have been reordered with pffft\_zreorder (otherwise just perform the operation yourself as the dft coefs are stored as interleaved complex numbers).


the operation performed is: dft\_ab += (dft\_a \* fdt\_b)\*scaling


The dft\_a, dft\_b and dft\_ab pointers may alias. 


        

<hr>



### function pffft\_zreorder 

```C++
void pffft_zreorder (
    PFFFT_Setup * setup,
    const float * input,
    float * output,
    pffft_direction_t direction
) 
```



call pffft\_zreorder(.., PFFFT\_FORWARD) after pffft\_transform(...,
PFFFT\_FORWARD) if you want to have the frequency components in the correct "canonical" order, as interleaved complex numbers.


(for real transforms, both 0-frequency and half frequency components, which are real, are assembled in the first entry as F(0)+i\*F(n/2+1). Note that the original fftpack did place F(n/2+1) at the end of the arrays).


input and output should not alias. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/pffft/pffft.h`

