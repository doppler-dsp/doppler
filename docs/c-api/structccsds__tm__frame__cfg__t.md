

# Struct ccsds\_tm\_frame\_cfg\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_frame\_cfg\_t**](structccsds__tm__frame__cfg__t.md)



_Which coding is applied to one Transfer Frame._ [More...](#detailed-description)

* `#include <ccsds_tm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**attach\_asm**](#variable-attach_asm)  <br> |
|  int | [**convolutional**](#variable-convolutional)  <br> |
|  int | [**randomise**](#variable-randomise)  <br> |
|  unsigned | [**rs\_depth**](#variable-rs_depth)  <br> |












































## Detailed Description


Every stage is optional because the standard makes it so: 9.2.1.1 has ASMs between Transfer Frames with no coding at all, 3.2.2 makes the randomiser conditional on the system designer, and Reed-Solomon and the convolutional code are separate sections a mission selects between. The combination of all four is _concatenated coding_ (section 5), which is the case the tests exercise end to end. 


    
## Public Attributes Documentation




### variable attach\_asm 

```C++
int ccsds_tm_frame_cfg_t::attach_asm;
```



Prepend the ASM, making the unit a CADU 


        

<hr>



### variable convolutional 

```C++
int ccsds_tm_frame_cfg_t::convolutional;
```



Apply the section-3 inner code 


        

<hr>



### variable randomise 

```C++
int ccsds_tm_frame_cfg_t::randomise;
```



Apply the section-10 pseudo-randomiser 


        

<hr>



### variable rs\_depth 

```C++
unsigned ccsds_tm_frame_cfg_t::rs_depth;
```



Interleaving depth; 0 for no outer code 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_frame.h`

