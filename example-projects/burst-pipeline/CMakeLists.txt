cmake_minimum_required(VERSION 3.16)
project(doppler_burst_pipeline C)

# A burst waveform end to end: describe a frame, generate the train, sweep it
# through a Plan, write BLUE, read it back and time both halves.
#
# Point CMake at an install prefix (a `cmake --install` tree or an extracted
# release tarball) with
#   -DCMAKE_PREFIX_PATH=/path/to/doppler-install
# or rely on a system install. Frame description, composition, the Plan cache
# and the BLUE reader/writer are all in the pure-C core, with no stream/NATS
# seam — so the core alone is enough and no optional component is needed.
#
# `m` is on the link line because THIS program calls log10() and fabs() to
# report its own measurements. doppler's own libm use is its business and
# comes with the imported target; a downstream that does maths of its own
# links libm for that reason, exactly as it would without doppler. Dropped
# here it fails at the link, not at runtime, which is how you find out.
find_package(doppler REQUIRED)

add_executable(burst_pipeline_shared main.c)
target_link_libraries(burst_pipeline_shared PRIVATE doppler::doppler m)

if(TARGET doppler::doppler-static)
    add_executable(burst_pipeline_static main.c)
    target_link_libraries(burst_pipeline_static PRIVATE doppler::doppler-static m)
endif()
