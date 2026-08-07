

# File wfm\_path.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_path.h**](wfm__path_8h.md)

[Go to the source code of this file](wfm__path_8h_source.md)

_Sibling-path construction shared by the wfm reader and writer._ [More...](#detailed-description)

* `#include <stdio.h>`
* `#include <string.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**wfm\_meta\_path**](#function-wfm_meta_path) (const char \* path, char \* out, size\_t cap) <br>_Name the_ `.sigmf-meta` _sidecar belonging to the capture at_`path` _._ |
|  void | [**wfm\_swap\_ext**](#function-wfm_swap_ext) (const char \* path, const char \* ext, char \* out, size\_t cap) <br>_Swap_ `path's` _final extension for_`ext` _._ |


























## Detailed Description


Several file types come in pairs — a BLUE detached capture is `<base>.hdr` + `<base>.det`, a SigMF one is `<base>.sigmf-data` + `<base>.sigmf-meta` — so both halves of the library have to derive one member's name from the other's. Reader and writer MUST agree on that derivation, or the writer emits a sidecar at a name the reader will not look for; one definition here is what makes that impossible. 


    
## Public Static Functions Documentation




### function wfm\_meta\_path 

_Name the_ `.sigmf-meta` _sidecar belonging to the capture at_`path` _._
```C++
static inline void wfm_meta_path (
    const char * path,
    char * out,
    size_t cap
) 
```



Two derivations, because two different things are being named:



* `"cap.sigmf-data"` → `"cap.sigmf-meta"` (SWAP). Not a choice: SigMF defines a capture as the `<base>.sigmf-data` + `<base>.sigmf-meta` pair, and conformant tools find the second half by exactly that name.
* `"cap.raw"` → `"cap.raw.sigmf-meta"` (APPEND). Anything else is not a SigMF capture, so no external tool looks for its metadata under any name, which leaves the derivation free — and a free choice should be the one that cannot collide. Swapping would give `cap.raw` and a genuine `cap.sigmf-data` in one directory the SAME sidecar name, so writing one capture would silently overwrite the other's metadata. Appending keeps the sidecar 1:1 with the file it describes, which is also what makes it safe to read back by exact name: wfm\_reader\_create deliberately does NOT sniff for `<base>.sigmf-meta` beside an arbitrary file, because a shared base name hijacked two unrelated files the first time that was tried.






**Parameters:**


* `path` capture (data) path. 
* `out` destination buffer; always NUL-terminated, truncated if `cap` is too small. 
* `cap` bytes available at `out`. 




        

<hr>



### function wfm\_swap\_ext 

_Swap_ `path's` _final extension for_`ext` _._
```C++
static inline void wfm_swap_ext (
    const char * path,
    const char * ext,
    char * out,
    size_t cap
) 
```



`"foo/cap.prm"` + `".det"` → `"foo/cap.det"`. A path whose basename carries no dot simply gains the extension, so `"cap"` → `"cap.det"`.


The dot must be in the BASENAME: a dotted directory (`"../cap"`, `"v1.2/cap"`) is not an extension, and treating it as one would write the sibling into the wrong place entirely.




**Parameters:**


* `path` source path. 
* `ext` replacement extension, leading dot included. 
* `out` destination buffer; always NUL-terminated, truncated if `cap` is too small. 
* `cap` bytes available at `out`. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_path.h`

