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
