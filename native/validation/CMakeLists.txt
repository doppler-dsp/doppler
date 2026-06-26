# Hand-owned Monte-Carlo validation harnesses (NOT jm-managed): each proves a
# component meets its theoretical bound. They link the static core library
# (pn + awgn + the component cores) and register a fast `--check` CTest.

add_executable(validate_ber_despreader
               ${CMAKE_CURRENT_SOURCE_DIR}/ber_despreader.c)
target_include_directories(validate_ber_despreader
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_ber_despreader PRIVATE doppler_lib_static m)
add_test(NAME validate_ber_despreader
         COMMAND validate_ber_despreader --check)
# BER Monte-Carlo: allow FP-reduction vectorization (the despread is a plain
# dot product; sign decision is insensitive to reassociation).
target_compile_options(validate_ber_despreader PRIVATE
                       -O3 -funroll-loops -ffast-math)
