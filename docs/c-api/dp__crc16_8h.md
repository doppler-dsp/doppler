

# File dp\_crc16.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_crc16.h**](dp__crc16_8h.md)

[Go to the source code of this file](dp__crc16_8h_source.md)

_CRC-16-CCITT over a bit stream — the one CRC shared by every doppler frame producer and consumer._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**dp\_crc16\_ccitt**](#function-dp_crc16_ccitt) (const uint8\_t \* bits, size\_t n) <br>_CRC-16-CCITT (poly 0x1021, init 0xFFFF) over a bit stream, MSB-first._  |


























## Detailed Description


The DSSS burst frame convention is `sync | payload | CRC-16`, with the 16-bit trailer computed over the payload bits only and transmitted MSB-first. `burst_demod` validates it on receive and the wfmgen DSSS frame builder (`wfm_frame_dsss_chips`) appends it on transmit; both call this one inline so the two ends can never drift.


Header-only (like `wfm_synth_mls_poly`) so no component grows a link-line dependency for a 10-line kernel. 


    
## Public Static Functions Documentation




### function dp\_crc16\_ccitt 

_CRC-16-CCITT (poly 0x1021, init 0xFFFF) over a bit stream, MSB-first._ 
```C++
static inline uint16_t dp_crc16_ccitt (
    const uint8_t * bits,
    size_t n
) 
```



Each input byte carries ONE bit (0/1) in its LSB — the natural form for the unpacked bit arrays doppler's frame paths pass around, not a packed byte-stream CRC.




**Parameters:**


* `bits` Array of `n` bytes, each 0 or 1 (only the LSB is used). 
* `n` Number of bits. 



**Returns:**

The 16-bit CRC; transmit it MSB-first (`(crc >> 15) & 1` goes first on the wire). 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_crc16.h`

