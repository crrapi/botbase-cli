cmake_minimum_required(VERSION 3.20)
project(botbase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(botbase_core STATIC
    src/core/client.cpp
    src/core/discovery.cpp
    src/core/input.cpp
    src/core/protocol.cpp
    src/core/signals.cpp
    src/core/socket.cpp
    src/core/util.cpp
    src/core/values.cpp
)
target_include_directories(botbase_core PUBLIC include)
target_compile_options(botbase_core PRIVATE -Wall -Wextra -Wpedantic)
