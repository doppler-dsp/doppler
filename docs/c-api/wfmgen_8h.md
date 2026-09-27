

# File wfmgen.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfmgen.h**](wfmgen_8h.md)

[Go to the source code of this file](wfmgen_8h_source.md)








































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_doppler\_wfmgen**](#function-dp_doppler_wfmgen) (int argc, char \* argv) <br>_Run the wfmgen composer CLI in-process (argv in, exit code out)._  |




























## Public Functions Documentation




### function dp\_doppler\_wfmgen 

_Run the wfmgen composer CLI in-process (argv in, exit code out)._ 
```C++
int dp_doppler_wfmgen (
    int argc,
    char * argv
) 
```



Parses `argv` exactly as the `wfmgen` binary does (`--type`, `--count`, `--from-file`, `--output`, `--record`, the file-type/wire/endian flags, the `nats://` sink, `--realtime` pacing, …), composes the waveform, and writes it to the chosen destination (a file, stdout, or a NATS PUB subject). Output is byte-identical to invoking the CLI with the same arguments — it is the same code path, not a reimplementation.


Process-global only in the ways the CLI is, and each is stated:
* it writes to `stdout` / `stderr` and creates the `--output` / `--record` files;
* it installs SIGINT and SIGTERM handlers for the length of the call, so a stop signal ends the run cleanly instead of killing it mid-write, and puts the caller's own handlers back before it returns, on every exit;
* on Windows it sets `stdout` to binary mode for the process, because IQ written in text mode is corrupted, and leaves it so.




It registers no `atexit` hooks, so it is safe to call repeatedly within one process. Not reentrant across threads: it shares `stdout` and the process's signal handlers.




**Parameters:**


* `argc` Argument count, including `argv``[0]` (the program name). 
* `argv` Argument vector; `argv``[0]` is used only in diagnostics/usage. 



**Returns:**

0 on success; a non-zero shell exit code on a usage or I/O error (mirrors the CLI: 1 = runtime/I/O failure, 2 = bad arguments).



```C++
// Generate a 4096-sample QPSK capture to a file, in-process.
char *av[] = { "wfmgen", "--type", "qpsk", "--count", "4096",
               "--output", "out.cf32", NULL };
int rc = dp_doppler_wfmgen(7, av);   // rc == 0; out.cf32 written
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfmgen.h`

