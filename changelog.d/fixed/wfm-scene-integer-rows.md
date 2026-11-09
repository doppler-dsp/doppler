- **A scene's integer fields are range-checked before they are
    converted.** A forged `"num_samples"` past what a `size_t` holds was
    converted with undefined behavior, and a Plan's cache buffer was then
    sized by an unchecked `num_samples * 8` that wrapped. Every int,
    `uint32`, `uint64` and count field now refuses a value it cannot hold,
    naming the key and its place, and the Plan sizes that buffer with
    `dp_mallocn`.
