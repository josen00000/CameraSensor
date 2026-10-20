cmake_minimum_required(VERSION 3.24)
project(sfc_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# sfc_core static library
add_library(sfc_core STATIC
    src/version.cpp
    src/pcg32.cpp
    src/xxhash_wrapper.cpp
    src/fixed_step_clock.cpp
    src/prop_sim.cpp
    src/capture_trigger.cpp
    src/wall_time_budget.cpp
    src/latency_histogram.cpp
)

target_include_directories(sfc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sfc_core SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/pcg-cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/xxhash
)

target_compile_definitions(sfc_core PRIVATE PCG_LITTLE_ENDIAN=1)

target_compile_features(sfc_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(sfc_core PRIVATE /W4 /WX)
else()
    target_compile_options(sfc_core PRIVATE -Wall -Wextra -Werror)
endif()

# Enable testing
enable_testing()

# Fetch GoogleTest at a pinned release tag
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(sfc_core_tests
    tests/version_test.cpp
    tests/pcg32_test.cpp
    tests/xxhash_test.cpp
    tests/fixed_step_clock_test.cpp
    tests/prop_sim_test.cpp
    tests/capture_trigger_test.cpp
    tests/wall_time_budget_test.cpp
    tests/latency_histogram_test.cpp
    tests/queue_contract_test.cpp
)

target_link_libraries(sfc_core_tests
    PRIVATE
        sfc_core
        GTest::gtest_main
)

target_compile_features(sfc_core_tests PRIVATE cxx_std_20)

# Compiler warnings as errors for tests
if(MSVC)
    target_compile_options(sfc_core_tests PRIVATE /W4 /WX)
else()
    target_compile_options(sfc_core_tests PRIVATE -Wall -Wextra -Werror)
endif()

include(GoogleTest)
gtest_discover_tests(sfc_core_tests)
