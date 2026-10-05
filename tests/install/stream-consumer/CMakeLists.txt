# The CMake face of the three-ways consumer. Docs include the marked
# region; build-three-ways.sh builds it for real on every release.
# --8<-- [start:cmake]
cmake_minimum_required(VERSION 3.16)
project(app C)

# Point CMake at the install prefix: -DCMAKE_PREFIX_PATH=<prefix>
find_package(doppler REQUIRED)

add_executable(app app.c)
target_link_libraries(app PRIVATE doppler::doppler)
# Using dp_pub_*/dp_sub_*? Add the optional stream component:
target_link_libraries(app PRIVATE doppler::stream)
# --8<-- [end:cmake]

# Not in the docs region: a consumer of ONLY the stream API, to hold that the
# shared libraries resolve each other (see stream_only.c).
add_executable(app_stream_only stream_only.c)
target_link_libraries(app_stream_only PRIVATE doppler::stream)
