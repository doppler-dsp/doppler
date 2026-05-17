

# Group sampletypes



[**Modules**](modules.md) **>** [**sampletypes**](group__sampletypes.md)



[More...](#detailed-description)


































































## Detailed Description


Floating-point complex types use C99 `<complex.h>`:



|Wire type  |C type  |bytes/sample  |
|-----|-----|-----|
|DP\_CF32  |`float` \_Complex  |8  |
|DP\_CF64  |`double` \_Complex  |16  |
|DP\_CF128  |`long` double \_Complex  |32  |






Integer complex types have no C99 equivalent. They are represented as interleaved I/Q arrays where each complex sample occupies two consecutive elements of the underlying integer type:



|Wire type  |C element type  |bytes/sample  |array length  |
|-----|-----|-----|-----|
|DP\_CI8  |`int8_t`  |2  |2×n  |
|DP\_CI16  |`int16_t`  |4  |2×n  |
|DP\_CI32  |`int32_t`  |8  |2×n  |






For `n` complex samples the send functions accept a pointer to `2*n` elements of the integer type (element 2k = I, 2k+1 = Q). 


    

------------------------------


