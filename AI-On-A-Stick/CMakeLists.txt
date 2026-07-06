cmake_minimum_required(VERSION 3.20)
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

if(MSVC)
    target_compile_options(ai_on_a_stick PRIVATE /W4)
else()
    target_compile_options(ai_on_a_stick PRIVATE -Wall -Wextra -Wpedantic)
endif()
