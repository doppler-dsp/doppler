# C Library

Build the library first (see [Build from Source](source.md)), then choose
one of the integration methods below.

## System install

Install headers and libraries to the system prefix (default `/usr/local`):

```sh
--8<-- "tests/install/cmake-install.sh:install"
```

Verify the install is visible to your toolchain:

```sh
--8<-- "tests/install/cmake-install.sh:verify"
```

### CMake — find_package

```cmake
find_package(doppler REQUIRED)
target_link_libraries(my_app PRIVATE doppler::doppler)
```

### pkg-config

```sh
gcc -o app main.c $(pkg-config --cflags --libs doppler)
```

!!! tip "Custom install prefix"

    ```sh
    --8<-- "tests/install/cmake-install.sh:custom-prefix"
    ```

## Link from the build tree (no install)

Useful during development — no `cmake --install` required.

**Shared library:**

```sh
--8<-- "tests/install/cmake-link.sh:shared"
```

!!! warning "rpath on Linux"

    The `-Wl,-rpath` flag bakes the build path into the binary. If you
    move the binary or the build tree, set `LD_LIBRARY_PATH` or re-link.

**Static library** (no runtime `.so` dependency):

```sh
--8<-- "tests/install/cmake-link.sh:static"
```

**CMake — add_subdirectory:**

```cmake
add_subdirectory(path/to/doppler)
target_link_libraries(my_app PRIVATE doppler::doppler)
```
