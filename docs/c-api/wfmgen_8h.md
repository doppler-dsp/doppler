

# File wfmgen.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfmgen.h**](wfmgen_8h.md)

[Go to the source code of this file](wfmgen_8h_source.md)








































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**doppler\_wfmgen**](#function-doppler_wfmgen) (int argc, char \* argv) <br>_Run the wfmgen composer CLI in-process (argv in, exit code out)._  |




























## Public Functions Documentation




### function doppler\_wfmgen 

_Run the wfmgen composer CLI in-process (argv in, exit code out)._ 
```C++
int doppler_wfmgen (
    int argc,
    char * argv
) 
```



Parses `argv` exactly as the `wfmgen` binary does (`--type`, `--count`, `--from-file`, `--output`, `--record`, the container/wire/endian flags, the `zmq://` sink, `--realtime` pacing, …), composes the waveform, and writes it to the chosen destination (a file, stdout, or a ZMQ PUB endpoint). Output is byte-identical to invoking the CLI with the same arguments — it is the same code path, not a reimplementation.


Process-global only in the ways the CLI is: it may write to `stdout` / `stderr` and create the `--output` / `--record` files. It installs no signal handlers, registers no `atexit` hooks, and keeps no mutable global state, so it is safe to call repeatedly within one process. Not reentrant across threads (it shares `stdout`).




**Parameters:**


* `argc` Argument count, including `argv``[0]` (the program name). 
* `argv` Argument vector; `argv``[0]` is used only in diagnostics/usage. 



**Returns:**

0 on success; a non-zero shell exit code on a usage or I/O error (mirrors the CLI: 1 = runtime/I/O failure, 2 = bad arguments).



```C++
// Generate a 4096-sample QPSK capture to a file, in-process.
char *av[] = { "wfmgen", "--type", "qpsk", "--count", "4096",
               "--output", "out.cf32", NULL };
int rc = doppler_wfmgen(7, av);   // rc == 0; out.cf32 written
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfmgen.h`

