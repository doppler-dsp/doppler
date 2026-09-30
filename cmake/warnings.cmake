# Compiler warnings for doppler's OWN targets -- and -Werror (doppler#1658).
#
# Until this file the library built with no warning flag at all: 0 of 691
# compile lines in compile_commands.json carried -Wall. An unhandled enum
# value (#1642, #1657), a dead helper or a comparison that could never be
# true was visible to no gate -- #1642 surfaced only because clang enables
# -Wswitch by default and gcc does not.
#
# WHERE. Included near the top of the root CMakeLists.txt (so fft's
# CMakeLists can name DP_VENDOR_NO_WARN), and applied by dp_apply_warnings()
# at its END, once every target exists. It walks the directory tree rather
# than setting add_compile_options() up front, for two reasons:
#
#   * jm generates most per-module CMakeLists.txt, and a flag written into one
#     is drift. A walk reaches every generated target from here, unedited.
#   * vendor/ is upstream code and keeps upstream's warnings. A target whose
#     every source is under vendor/ (cJSON's wfm_cjson_core) is skipped here;
#     a vendored source compiled INSIDE a doppler target (pocketfft and pffft
#     in fft_core) gets DP_VENDOR_NO_WARN as a source property in its own
#     directory, because a source property set here would not reach a target
#     defined in another directory. nats.c is built by a separate cmake call
#     and never sees these flags at all.
#
# WHAT. -Wall -Wextra, and -Werror, with two kinds of exception and no other:
#
#   * Flow-sensitive classes stay warnings (DP_WARN_ONLY). Whether gcc reports
#     -Wmaybe-uninitialized or -Wstringop-truncation depends on its version and
#     the inliner's decisions at -O3, and CI spans gcc 8 to 15: an error that
#     appears on one toolchain and not another is a gate nobody can run
#     locally. doppler's code is at ZERO in these classes too; they are warned,
#     not ignored.
#   * The CPython extension modules (MODULE_LIBRARY) are jm-generated glue,
#     and three classes there are the shape of the CPython ABI rather than a
#     defect: a METH_NOARGS function takes an `args` it never reads
#     (-Wunused-parameter), a method table casts to PyCFunction
#     (-Wcast-function-type), and `{NULL}` ends a PyMethodDef table
#     (-Wmissing-field-initializers). Suppressed on those targets only, and
#     the library, the tests and the benches keep all three as errors.
#
# clang-cl reads MSVC's option syntax, and its bare `-Wall` means
# -Weverything, so every flag here goes through dp_gnu_compile_options(),
# which spells it `/clang:-Wall` there. (MSVC's own cl.exe cannot build
# doppler at all; see the root CMakeLists.txt.)
#
# -Werror is OPT-IN (DOPPLER_WERROR, default OFF). A consumer building from
# source -- the sdist through pip, vcpkg, a deb/rpm, add_subdirectory or
# FetchContent -- gets the warnings and never an error, so a warning class
# some future compiler adds to -Wall cannot stop them building a doppler they
# do not maintain. doppler's own builds pass ON from the Makefile's configure
# (WERROR_FLAG), which is what `make build`, `make test` and CI run.

option(DOPPLER_WERROR
       "Treat compiler warnings in doppler's own code as errors" OFF)

set(DP_WARNING_FLAGS -Wall -Wextra)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # Names gcc 8 already knows: gcc rejects -Wno-error=<unknown> outright.
    set(DP_WARN_ONLY maybe-uninitialized stringop-truncation
        stringop-overflow format-truncation array-bounds)
else()
    set(DP_WARN_ONLY "")
endif()

set(DP_GLUE_SUPPRESS
    -Wno-unused-parameter -Wno-cast-function-type
    -Wno-missing-field-initializers)
# jm generates a static `_enum_index` helper into a module with an enum
# property whether or not the module uses it (just-makeit#1745): dead code,
# a real finding and jm's to fix, so it stays a WARNING in the glue until the
# pin carries the fix.
set(DP_GLUE_WARN_ONLY unused-function)

# A vendored source compiled inside a doppler target: no warnings at all.
# -w outranks -Werror on both gcc and clang (measured), and clang-cl spells
# it the same way through /clang:.
if(MSVC AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(DP_VENDOR_NO_WARN /clang:-w)
else()
    set(DP_VENDOR_NO_WARN -w)
endif()

# Every buildsystem target under `dir`, recursively.
function(_dp_collect_targets dir out)
    get_property(_t DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_s IN LISTS _subs)
        _dp_collect_targets("${_s}" _more)
        list(APPEND _t ${_more})
    endforeach()
    set(${out} ${_t} PARENT_SCOPE)
endfunction()

# True when every real source of `target` lives under vendor/. Generator
# expressions ($<TARGET_OBJECTS:...>) are objects compiled elsewhere, and
# say nothing about this target's own compile line.
function(_dp_is_vendor_only target out)
    get_target_property(_srcs ${target} SOURCES)
    get_target_property(_sdir ${target} SOURCE_DIR)
    set(_own 0)
    set(_vendor 0)
    foreach(_s IN LISTS _srcs)
        if(_s MATCHES "^\\$<")
            continue()
        endif()
        if(NOT IS_ABSOLUTE "${_s}")
            set(_s "${_sdir}/${_s}")
        endif()
        if(_s MATCHES "^${CMAKE_SOURCE_DIR}/vendor/")
            math(EXPR _vendor "${_vendor} + 1")
        else()
            math(EXPR _own "${_own} + 1")
        endif()
    endforeach()
    if(_vendor GREATER 0 AND _own EQUAL 0)
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(dp_apply_warnings)
    _dp_collect_targets("${CMAKE_SOURCE_DIR}" _targets)
    set(_compiled EXECUTABLE STATIC_LIBRARY SHARED_LIBRARY MODULE_LIBRARY
        OBJECT_LIBRARY)
    foreach(_t IN LISTS _targets)
        get_target_property(_type ${_t} TYPE)
        if(NOT _type IN_LIST _compiled)
            continue()
        endif()
        _dp_is_vendor_only(${_t} _vendor_only)
        if(_vendor_only)
            continue()
        endif()
        set(_flags ${DP_WARNING_FLAGS})
        set(_warn_only ${DP_WARN_ONLY})
        if(_type STREQUAL "MODULE_LIBRARY")
            list(APPEND _flags ${DP_GLUE_SUPPRESS})
            list(APPEND _warn_only ${DP_GLUE_WARN_ONLY})
        endif()
        if(DOPPLER_WERROR)
            list(APPEND _flags -Werror)
            foreach(_w IN LISTS _warn_only)
                list(APPEND _flags -Wno-error=${_w})
            endforeach()
        endif()
        dp_gnu_compile_options(${_t} PRIVATE ${_flags})
    endforeach()
endfunction()
