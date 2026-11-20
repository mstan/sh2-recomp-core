add_executable(sh2_decode_test decode_test.c)
target_link_libraries(sh2_decode_test PRIVATE sh2-recomp-core::core)
if(MSVC)
    target_compile_options(sh2_decode_test PRIVATE /W4 /WX)
else()
    target_compile_options(sh2_decode_test PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME sh2_decode COMMAND sh2_decode_test)


add_executable(sh2_generate_step_fixture generate_step.c)
target_link_libraries(sh2_generate_step_fixture PRIVATE sh2_catalog_emitter)
set(fixture_catalog "${CMAKE_CURRENT_BINARY_DIR}/fixture_catalog.c")
add_custom_command(OUTPUT "${fixture_catalog}"
    COMMAND sh2_generate_step_fixture "${fixture_catalog}"
    DEPENDS sh2_generate_step_fixture
    VERBATIM)
add_executable(sh2_step_test step_test.c "${fixture_catalog}")
target_link_libraries(sh2_step_test PRIVATE sh2-recomp-core::interpreter)
add_test(NAME sh2_step COMMAND sh2_step_test)

add_executable(sh7604_bsc_test sh7604_bsc_test.c)
target_link_libraries(sh7604_bsc_test PRIVATE sh2-recomp-core::core)
add_test(NAME sh7604_bsc COMMAND sh7604_bsc_test)

add_executable(sh7604_system_test sh7604_system_test.c)
target_link_libraries(sh7604_system_test PRIVATE sh2-recomp-core::core)
add_test(NAME sh7604_system COMMAND sh7604_system_test)

add_executable(sh7604_cache_test sh7604_cache_test.c)
target_link_libraries(sh7604_cache_test PRIVATE sh2-recomp-core::core)
add_test(NAME sh7604_cache COMMAND sh7604_cache_test)

add_executable(sh7604_frt_test sh7604_frt_test.c)
target_link_libraries(sh7604_frt_test PRIVATE sh2-recomp-core::core)
add_test(NAME sh7604_frt COMMAND sh7604_frt_test)
