cmake_minimum_required(VERSION 3.20)
project(botbase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bb_cpp
    cpp/experiment.cpp
    cpp/socket.cpp
    cpp/protocol.cpp
    cpp/input.cpp
)
target_include_directories(bb_cpp PRIVATE cpp)
target_compile_options(bb_cpp PRIVATE -Wall -Wextra)

add_executable(cpp_unit cpp/tests/unit.cpp cpp/input.cpp)
target_include_directories(cpp_unit PRIVATE cpp)

enable_testing()
add_test(NAME cpp_unit COMMAND cpp_unit)

add_executable(botbase cpp/cli/main.cpp cpp/socket.cpp cpp/protocol.cpp cpp/input.cpp)
target_include_directories(botbase PRIVATE cpp)
target_compile_options(botbase PRIVATE -Wall -Wextra)
