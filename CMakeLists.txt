cmake_minimum_required(VERSION 3.16)
project(sh2_recomp_core LANGUAGES C)

add_library(sh2_recomp_core STATIC src/decode.c)
add_library(sh2-recomp-core::core ALIAS sh2_recomp_core)
target_include_directories(sh2_recomp_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sh2_recomp_core PUBLIC c_std_11)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
