cmake_minimum_required(VERSION 3.20)

project(botbase
    VERSION 2.0.0
    DESCRIPTION "Command-line client and C++ library for sys-botbase"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BOTBASE_BUILD_TESTS "Build the test suite" ON)
option(BOTBASE_BUILD_EXAMPLES "Build the library examples" ON)
option(BOTBASE_STATIC "Link the botbase binary statically, for portable Linux releases" OFF)

set(BOTBASE_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast)

add_library(botbase_core STATIC
    src/core/client.cpp
    src/core/discovery.cpp
    src/core/input.cpp
    src/core/protocol.cpp
    src/core/signals.cpp
    src/core/socket.cpp
    src/core/util.cpp
    src/core/values.cpp)
add_library(botbase::core ALIAS botbase_core)
target_include_directories(botbase_core PUBLIC include)
target_compile_options(botbase_core PRIVATE ${BOTBASE_WARNINGS})

add_executable(botbase
    src/cli/args.cpp
    src/cli/commands.cpp
    src/cli/host_cache.cpp
    src/cli/main.cpp
    src/cli/session.cpp)
target_link_libraries(botbase PRIVATE botbase_core)
target_compile_definitions(botbase PRIVATE BOTBASE_VERSION="${PROJECT_VERSION}")
target_compile_options(botbase PRIVATE ${BOTBASE_WARNINGS})
if(BOTBASE_STATIC)
    target_link_options(botbase PRIVATE -static)
endif()

include(GNUInstallDirs)
install(TARGETS botbase RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BOTBASE_BUILD_EXAMPLES)
    foreach(example build_id memory_probe)
        add_executable(example_${example} examples/${example}.cpp)
        target_link_libraries(example_${example} PRIVATE botbase_core)
        target_compile_options(example_${example} PRIVATE ${BOTBASE_WARNINGS})
    endforeach()
endif()

if(BOTBASE_BUILD_TESTS)
    enable_testing()

    add_executable(unit_tests tests/unit/unit_tests.cpp)
    target_link_libraries(unit_tests PRIVATE botbase_core)
    target_compile_options(unit_tests PRIVATE ${BOTBASE_WARNINGS})
    add_test(NAME unit COMMAND unit_tests)

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME cli
            COMMAND ${Python3_EXECUTABLE} -m unittest -v test_cli
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli)
        set_tests_properties(cli PROPERTIES ENVIRONMENT "BOTBASE_BIN=$<TARGET_FILE:botbase>")
    endif()
endif()
