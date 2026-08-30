cmake_minimum_required(VERSION 3.24)

project(MissionHoldSkipper
    VERSION 0.1.0
    DESCRIPTION "Hold-to-skip mission scenes for GTA III, Vice City and San Andreas"
    LANGUAGES CXX
)

option(MHS_BUILD_TESTS "Build host-side tests for the pure logic" OFF)
set(MHS_VERSION "${PROJECT_VERSION}" CACHE STRING "version string baked into the plugin")

string(REPLACE "/GX" "/EHsc" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# one .asi per game, same sources, the backend and the define pick the game
function(mhs_add_plugin target)
    cmake_parse_arguments(ARG "" "GAME;BACKEND" "" ${ARGN})

    add_library(${target} SHARED
        src/main.cpp
        src/config.cpp
        src/hold_skip.cpp
        src/ring.cpp
        ${ARG_BACKEND}
    )

    target_include_directories(${target} PRIVATE src)

    target_compile_features(${target} PRIVATE cxx_std_20)

    target_compile_definitions(${target} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        MHS_VERSION="${MHS_VERSION}"
        ${ARG_GAME}
    )

    target_compile_options(${target} PRIVATE /W4 /permissive-)

    target_link_libraries(${target} PRIVATE mini::mini)

    set_target_properties(${target} PROPERTIES
        PREFIX ""
        SUFFIX ".asi"
    )

    # the plugin reads the ini next to itself, so every game gets its own copy
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/MissionHoldSkipper.ini"
            "$<TARGET_FILE_DIR:${target}>/${target}.ini"
        VERBATIM
    )
endfunction()

# the .asi only makes sense as a 32-bit windows dll
if(WIN32)
    find_package(mini REQUIRED)

    mhs_add_plugin(MissionHoldSkipper GAME MHS_GAME_SA BACKEND src/game/sa/backend.cpp)
    mhs_add_plugin(MissionHoldSkipperIII GAME MHS_GAME_III BACKEND src/game/iii/backend.cpp)
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
