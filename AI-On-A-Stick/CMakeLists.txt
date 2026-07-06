cmake_minimum_required(VERSION 3.20)

if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(_ai_on_a_stick_vcpkg_toolchain "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    if(EXISTS "${_ai_on_a_stick_vcpkg_toolchain}")
        set(CMAKE_TOOLCHAIN_FILE "${_ai_on_a_stick_vcpkg_toolchain}" CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(ai_on_a_stick VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_executable(ai_on_a_stick
    src/main.cpp
    src/config.cpp
    src/database.cpp
    src/hardware.cpp
    src/http_server.cpp
    src/llama_runtime.cpp
    src/machine.cpp
)

target_include_directories(ai_on_a_stick PRIVATE src)
target_link_libraries(ai_on_a_stick PRIVATE SQLite3::SQLite3 Threads::Threads)

if(WIN32)
    target_compile_definitions(ai_on_a_stick PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(ai_on_a_stick PRIVATE /W4)
else()
    target_compile_options(ai_on_a_stick PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    message(STATUS "Windows build configured for AI-On-A-Stick")
    if(DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(WARNING "SQLite3 must be discoverable on Windows. Recommended: set VCPKG_ROOT and use vcpkg.")
    endif()
endif()