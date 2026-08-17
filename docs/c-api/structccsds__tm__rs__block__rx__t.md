

# Struct ccsds\_tm\_rs\_block\_rx\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_rs\_block\_rx\_t**](structccsds__tm__rs__block__rx__t.md)



_What_ [_**ccsds\_tm\_rs\_decode\_block**_](ccsds__tm__rs_8h.md#function-ccsds_tm_rs_decode_block) _found in one codeblock._[More...](#detailed-description)

* `#include <ccsds_tm_rs.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**codewords**](#variable-codewords)  <br> |
|  unsigned | [**corrected**](#variable-corrected)  <br> |
|  unsigned | [**symbols**](#variable-symbols)  <br> |
|  unsigned | [**uncorrectable**](#variable-uncorrectable)  <br> |












































## Detailed Description


`codewords - uncorrectable` is how many are good afterwards, and [**symbols**](structccsds__tm__rs__block__rx__t.md#variable-symbols) is the repair work the outer code actually did — the quantity that says whether the inner code is delivering what the outer one was sized for. 


    
## Public Attributes Documentation




### variable codewords 

```C++
unsigned ccsds_tm_rs_block_rx_t::codewords;
```



Codewords in the block, i.e. the depth 
 


        

<hr>



### variable corrected 

```C++
unsigned ccsds_tm_rs_block_rx_t::corrected;
```



How many needed and received repair 
 


        

<hr>



### variable symbols 

```C++
unsigned ccsds_tm_rs_block_rx_t::symbols;
```



Symbol errors repaired across the block 
 


        

<hr>



### variable uncorrectable 

```C++
unsigned ccsds_tm_rs_block_rx_t::uncorrectable;
```



How many the decoder refused 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_rs.h`

