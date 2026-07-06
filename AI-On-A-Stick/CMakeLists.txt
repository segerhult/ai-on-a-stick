cmake_minimum_required(VERSION 3.20)

include(FetchContent)

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

option(AI_ON_A_STICK_BUILD_LLAMA_SERVER "Fetch and bundle llama.cpp's llama-server runtime" OFF)
option(AI_ON_A_STICK_LLAMA_CUDA "Enable CUDA backend when building fetched llama.cpp" OFF)
set(AI_ON_A_STICK_LLAMA_CPP_REPOSITORY "https://github.com/ggml-org/llama.cpp.git" CACHE STRING
    "Git repository used for llama.cpp")
set(AI_ON_A_STICK_LLAMA_CPP_TAG "20a04b22063020cd0f29b7781f5352d7a6abf786" CACHE STRING
    "Git tag or commit used for llama.cpp")

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

if(AI_ON_A_STICK_BUILD_LLAMA_SERVER)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_ai_on_a_stick_llama_bundle_dir "${CMAKE_SOURCE_DIR}/bin/windows-x64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
            set(_ai_on_a_stick_llama_bundle_dir "${CMAKE_SOURCE_DIR}/bin/macos-arm64")
        else()
            set(_ai_on_a_stick_llama_bundle_dir "${CMAKE_SOURCE_DIR}/bin/macos-x64")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
            set(_ai_on_a_stick_llama_bundle_dir "${CMAKE_SOURCE_DIR}/bin/linux-arm64")
        else()
            set(_ai_on_a_stick_llama_bundle_dir "${CMAKE_SOURCE_DIR}/bin/linux-x64")
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform for llama-server bundling: ${CMAKE_SYSTEM_NAME}")
    endif()

    set(LLAMA_BUILD_COMMON ON CACHE BOOL "Build llama.cpp common library" FORCE)
    set(LLAMA_BUILD_SERVER ON CACHE BOOL "Build llama.cpp server" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "Disable extra llama.cpp tools" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "Disable llama.cpp examples" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "Disable llama.cpp tests" FORCE)
    set(LLAMA_BUILD_APP OFF CACHE BOOL "Disable llama.cpp unified app" FORCE)
    set(LLAMA_TOOLS_INSTALL OFF CACHE BOOL "Disable llama.cpp install rules for tools" FORCE)
    set(LLAMA_TESTS_INSTALL OFF CACHE BOOL "Disable llama.cpp install rules for tests" FORCE)
    set(LLAMA_OPENSSL OFF CACHE BOOL "Disable OpenSSL in fetched llama.cpp" FORCE)
    set(GGML_CUDA ${AI_ON_A_STICK_LLAMA_CUDA} CACHE BOOL "Enable CUDA backend in fetched llama.cpp" FORCE)

    FetchContent_Declare(
        llama_cpp
        GIT_REPOSITORY ${AI_ON_A_STICK_LLAMA_CPP_REPOSITORY}
        GIT_TAG ${AI_ON_A_STICK_LLAMA_CPP_TAG}
    )
    FetchContent_MakeAvailable(llama_cpp)

    get_target_property(_ai_on_a_stick_llama_server_output_dir llama-server RUNTIME_OUTPUT_DIRECTORY)
    if(NOT _ai_on_a_stick_llama_server_output_dir)
        set(_ai_on_a_stick_llama_server_output_dir "${CMAKE_BINARY_DIR}/bin")
    endif()

    add_custom_target(ai_on_a_stick_bundle_llama_server ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_ai_on_a_stick_llama_bundle_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_ai_on_a_stick_llama_server_output_dir}"
                "${_ai_on_a_stick_llama_bundle_dir}"
        DEPENDS llama-server
        COMMENT "Bundling llama-server into ${_ai_on_a_stick_llama_bundle_dir}"
    )
    add_dependencies(ai_on_a_stick ai_on_a_stick_bundle_llama_server)

    message(STATUS "llama.cpp auto-build enabled from ${AI_ON_A_STICK_LLAMA_CPP_REPOSITORY}@${AI_ON_A_STICK_LLAMA_CPP_TAG}")
    message(STATUS "llama-server bundle destination: ${_ai_on_a_stick_llama_bundle_dir}")
    if(AI_ON_A_STICK_LLAMA_CUDA)
        message(STATUS "llama.cpp CUDA backend enabled")
    endif()
endif()

if(WIN32)
    message(STATUS "Windows build configured for AI-On-A-Stick")
    if(DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(WARNING "SQLite3 must be discoverable on Windows. Recommended: set VCPKG_ROOT and use vcpkg.")
    endif()
endif()
