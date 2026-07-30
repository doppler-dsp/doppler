# Hand-owned: just-makeit generates bindings, tests and benches, but a CLI
# tool is this project's own. `iqtools_lib_static` already carries capture_core
# (the root CMakeLists wires it in via target_sources), and it in turn pulls
# doppler through the imported target, so this is a two-line target.
add_executable(iq_info iq_info.c)
target_link_libraries(iq_info PRIVATE iqtools_lib_static doppler::doppler-static m)
target_include_directories(iq_info PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
