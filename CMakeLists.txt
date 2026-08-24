cmake_minimum_required(VERSION 3.24)

project(MissionHoldSkipper
    VERSION 0.1.0
    DESCRIPTION "Hold-to-skip mission scenes for GTA SA v1.0 US"
    LANGUAGES CXX
)

option(MHS_BUILD_TESTS "Build host-side tests for the pure logic" OFF)
set(MHS_VERSION "${PROJECT_VERSION}" CACHE STRING "version string baked into the plugin")

string(REPLACE "/GX" "/EHsc" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# the .asi only makes sense as a 32-bit windows dll
if(WIN32)
    find_package(mini REQUIRED)

    add_library(MissionHoldSkipper SHARED
        src/main.cpp
        src/config.cpp
        src/hold_skip.cpp
        src/ring.cpp
    )

    target_include_directories(MissionHoldSkipper PRIVATE src)

    target_compile_features(MissionHoldSkipper PRIVATE cxx_std_20)

    target_compile_definitions(MissionHoldSkipper PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        MHS_VERSION="${MHS_VERSION}"
    )

    target_compile_options(MissionHoldSkipper PRIVATE /W4 /permissive-)

    target_link_libraries(MissionHoldSkipper PRIVATE mini::mini)

    set_target_properties(MissionHoldSkipper PROPERTIES
        PREFIX ""
        SUFFIX ".asi"
    )

    add_custom_command(TARGET MissionHoldSkipper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/MissionHoldSkipper.ini"
            "$<TARGET_FILE_DIR:MissionHoldSkipper>/MissionHoldSkipper.ini"
        VERBATIM
    )
endif()

# geometry and hold state machine touch no game memory, so they build natively
if(MHS_BUILD_TESTS)
    enable_testing()

    add_executable(mhs_tests
        tests/test_main.cpp
        src/ring.cpp
    )
    target_include_directories(mhs_tests PRIVATE src)
    target_compile_features(mhs_tests PRIVATE cxx_std_20)
    target_compile_definitions(mhs_tests PRIVATE MHS_HOST_TEST)

    add_test(NAME mhs_tests COMMAND mhs_tests)
endif()
