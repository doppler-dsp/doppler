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

add_executable(validate_costas_jitter
               ${CMAKE_CURRENT_SOURCE_DIR}/costas_jitter.c)
target_include_directories(validate_costas_jitter
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_costas_jitter PRIVATE doppler_lib_static m)
target_compile_options(validate_costas_jitter PRIVATE -O3 -funroll-loops)
add_test(NAME validate_costas_jitter
         COMMAND validate_costas_jitter --check)

add_executable(validate_dll_jitter
               ${CMAKE_CURRENT_SOURCE_DIR}/dll_jitter.c)
target_include_directories(validate_dll_jitter
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_dll_jitter PRIVATE doppler_lib_static m)
target_compile_options(validate_dll_jitter PRIVATE -O3 -funroll-loops)
add_test(NAME validate_dll_jitter
         COMMAND validate_dll_jitter --check)

add_executable(validate_carrier_mpsk_scurve
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_mpsk_scurve.c)
target_include_directories(validate_carrier_mpsk_scurve
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_mpsk_scurve PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_mpsk_scurve PRIVATE -O3)
add_test(NAME validate_carrier_mpsk_scurve
         COMMAND validate_carrier_mpsk_scurve --check)

add_executable(validate_carrier_mpsk_jitter
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_mpsk_jitter.c)
target_include_directories(validate_carrier_mpsk_jitter
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_mpsk_jitter PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_mpsk_jitter PRIVATE -O3 -funroll-loops)
add_test(NAME validate_carrier_mpsk_jitter
         COMMAND validate_carrier_mpsk_jitter --check)

add_executable(validate_carrier_nda_scurve
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_scurve.c)
target_include_directories(validate_carrier_nda_scurve
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_nda_scurve PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_scurve PRIVATE -O3)
add_test(NAME validate_carrier_nda_scurve
         COMMAND validate_carrier_nda_scurve --check)

add_executable(validate_carrier_nda_pullin
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_pullin.c)
target_include_directories(validate_carrier_nda_pullin
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_nda_pullin PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_pullin PRIVATE -O3 -funroll-loops)
add_test(NAME validate_carrier_nda_pullin
         COMMAND validate_carrier_nda_pullin --check)

add_executable(validate_carrier_nda_step_response
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_step_response.c)
target_include_directories(validate_carrier_nda_step_response
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_nda_step_response
                      PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_step_response PRIVATE -O3)
add_test(NAME validate_carrier_nda_step_response
         COMMAND validate_carrier_nda_step_response --check)

add_executable(validate_mpsk_receiver_ber
               ${CMAKE_CURRENT_SOURCE_DIR}/mpsk_receiver_ber.c)
target_include_directories(validate_mpsk_receiver_ber
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_mpsk_receiver_ber PRIVATE doppler_lib_static m)
target_compile_options(validate_mpsk_receiver_ber PRIVATE -O3 -funroll-loops)
add_test(NAME validate_mpsk_receiver_ber
         COMMAND validate_mpsk_receiver_ber --check)

add_executable(validate_symsync_lock
               ${CMAKE_CURRENT_SOURCE_DIR}/symsync_lock.c)
target_include_directories(validate_symsync_lock
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_symsync_lock PRIVATE doppler_lib_static m)
target_compile_options(validate_symsync_lock PRIVATE -O3 -funroll-loops)
add_test(NAME validate_symsync_lock
         COMMAND validate_symsync_lock --check)
