

# File stream.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**stream**](dir_21b896cdbc030a0ded493211142b7733.md) **>** [**stream.h**](stream_8h.md)

[Go to the source code of this file](stream_8h_source.md)

_Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "clib_common.h"`
* `#include "dp_format.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_chunk\_t**](structdp__chunk__t.md) <br>_Reassembly geometry, present only when_ [_**DP\_FLAG\_CHUNKED**_](group__wire.md#define-dp_flag_chunked) _._ |
| struct | [**dp\_header\_t**](structdp__header__t.md) <br>_Frame metadata carried in every stream message._  |


















































## Detailed Description


Provides NATS-backed signal streaming using three messaging patterns:



|Pattern   |Sender function   |Receiver function   |Use case    |
|-----|-----|-----|-----|
|PUB/SUB   |dp\_pub\_\*   |dp\_sub\_\*   |Fan-out broadcast    |
|PUSH/PULL   |dp\_push\_\*   |dp\_pull\_\*   |Pipeline load-balance    |
|REQ/REP   |dp\_req\_\*   |dp\_rep\_\*   |Control metadata   |






Requires a running `nats-server` (`nats-server -js` for the PUSH/PULL JetStream work-queue tier). An endpoint is `"nats://host:port[/subject]"`; the subject defaults to `"default"` if omitted.


#### Quick start (C)




```C++
#include "stream/stream.h"

// Transmitter
dp_pub_t *pub = dp_pub_create("nats://127.0.0.1:4222/iq", CF64);
double _Complex samples[1024] = { ... };
dp_pub_send_cf64(pub, samples, 1024, 1e6, 2.4e9);
dp_pub_destroy(pub);

// Receiver (zero-copy)
dp_sub_t *sub = dp_sub_create("nats://127.0.0.1:4222/iq");
dp_msg_t *msg;  dp_header_t hdr;
dp_sub_recv(sub, &msg, &hdr);
double _Complex *cf64 = (double _Complex *)dp_msg_data(msg);
size_t n = dp_msg_num_samples(msg);
// use cf64[0..n-1] ...
dp_msg_free(msg);
dp_sub_destroy(sub);
```
 



    

------------------------------
The documentation for this class was generated from the following file `native/inc/stream/stream.h`

