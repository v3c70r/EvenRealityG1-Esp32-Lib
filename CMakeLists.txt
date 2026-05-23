cmake_minimum_required(VERSION 3.16)
project(eveng1lib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EVENG1_BUILD_TESTS "Build unit tests" ON)

# ─── Library Sources ───

set(EVENG1_GRAPHICS_SOURCES
    src/graphics/framebuffer.cpp
    src/graphics/rasterizer.cpp
    src/graphics/render_style.cpp
    src/graphics/font_bitmap.cpp
    src/graphics/crc32_xz.cpp
)

set(EVENG1_PROTOCOL_SOURCES
    src/protocol/g1_protocol.cpp
    src/protocol/touch_parser.cpp
)

set(EVENG1_TRANSPORT_SOURCES
    src/transport/null_transport.cpp
)

set(EVENG1_DEVICE_SOURCES
    src/devices/g1_display.cpp
)

add_library(eveng1
    ${EVENG1_GRAPHICS_SOURCES}
    ${EVENG1_PROTOCOL_SOURCES}
    ${EVENG1_TRANSPORT_SOURCES}
    ${EVENG1_DEVICE_SOURCES}
)

target_include_directories(eveng1 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(eveng1 PUBLIC cxx_std_17)

# ─── Tests ───

if(EVENG1_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 not found, downloading header...")
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.7.1
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    enable_testing()

    function(add_eveng1_test name)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE eveng1 Catch2::Catch2WithMain)
        target_compile_features(${name} PRIVATE cxx_std_17)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_eveng1_test(test_framebuffer)
    add_eveng1_test(test_rasterizer)
    add_eveng1_test(test_command_buffer)
    add_eveng1_test(test_dirty_rect)
    add_eveng1_test(test_dither)
    add_eveng1_test(test_font)
    add_eveng1_test(test_crc32)
    add_eveng1_test(test_protocol)
endif()
